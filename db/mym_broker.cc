#include <queue>
#include <sstream>
#include "rocksdb/mym_broker.h"
#include "mycelium/distributor.h"
#include "mycelium/converter.h"
#include "mycelium/augmenter.h"
#include "mycelium/mynooper.h"

namespace ROCKSDB_NAMESPACE {

MymBroker::MymBroker(const std::string& cfname,
                     bool cf_created,
                     const char *dbfilepath,
                     Options& options,
                     int num_splits)
    : options_(options)
{
    // Build once, then emit descriptors with parents correctly populated
    CFPlan plan = buildPlan(cfname);
    auto descriptors = emitDescriptors(plan);

    std::vector<ColumnFamilyHandle*> cf_handles;
    Status s;

    if (!cf_created) {
        s = DB::Open(options_, dbfilepath, &db_);
        assert(s.ok());

        s = db_->CreateColumnFamilies(descriptors, &cf_handles);
        assert(s.ok());
    } else {
        // Opening an existing DB with all CFs present: include default explicitly first
        std::vector<ColumnFamilyDescriptor> open_descs;
        open_descs.emplace_back(kDefaultColumnFamilyName, ColumnFamilyOptions(options_));
        open_descs.insert(open_descs.end(), descriptors.begin(), descriptors.end());

        s = DB::Open(options_, dbfilepath, open_descs, &cf_handles, &db_);
        assert(s.ok());
    }

    s = db_->AddTransformingDestinationCfds(cfname);
    assert(s.ok());

    saveColFamHandlesByName(plan, descriptors, cf_handles, cfname);
}

int MymBroker::Read(const std::string &key, const std::set<int>* positions, std::string &result)
{
    rocksdb::ReadOptions ro;
    Status s;
    size_t n_levels=int_cf_meta_.size();
    for (size_t level=0; level < n_levels; level++) {
        auto handles = int_cf_meta_.find(level);
        if (handles != int_cf_meta_.end() && !handles->second.empty()) {
            const auto& [name, meta] = *handles->second.begin();
            s = db_->Get(ro, meta.cf_handle_, key, &result);
            if (s.ok()) return 0;
        }
    }
    
    /*while (true) {
        std::unordered_map<std::string, ColFamMeta> level_handles = int_cf_meta_[level];
        if (level_handles.size() == 0) {
            break;
        }
        level++;

        for (auto lvl_hdl : level_handles) {
            if (lvl_hdl.first.find("_secondary_") != std::string::npos) {
                continue;
            }
            if (positions != nullptr && positions->size() > 0) {
                int column_search = checkColumnSearch(lvl_hdl.second, positions);
                if (column_search == 0) {
                    continue;
                } else if (column_search == 1) {
                    std::string partial_result;
                    s = db_->Get(ReadOptions(), lvl_hdl.second.cf_handle_, key, &partial_result);
                    if (!s.ok()) {
                        break;
                    }
                    result += partial_result;
                } else {
                    s = db_->Get(ReadOptions(), lvl_hdl.second.cf_handle_, key, &result);
                    if (s.ok()) {
                        return 0;
                    }
                }
            } else {
                std::string partial_result;
                s = db_->Get(ReadOptions(), lvl_hdl.second.cf_handle_, key, &partial_result);
                if (!s.ok()) {
                    break;
                }
                result += partial_result;
            }
        }
        if (s.ok()) {
            return 0;
        }
    }*/
    
    return 1;
}

int MymBroker::IndexRead(const std::string &key, const std::set<int>* positions, std::vector<std::string> &result)
{
    Status s;

    // search for index handles in level-1 handles
    std::unordered_map<std::string, ColFamMeta> level_1_handles = int_cf_meta_[1];

    std::string valkeystr;
    for (auto idx_hdl : level_1_handles) {
        if (idx_hdl.first.find("_secondary_") != std::string::npos) {
            s = db_->Get(ReadOptions(), idx_hdl.second.cf_handle_, key, &valkeystr);
            if (!s.ok()) {
                return Status::kNotFound;
            }
            break;
        }
    }

    ColumnFamilyHandle* primary_hdl = nullptr;
    for (auto pri_hdl : level_1_handles) {
        if (pri_hdl.first.find("_indexed_data_cf") != std::string::npos) {
            primary_hdl = pri_hdl.second.cf_handle_;
        }
    }

    if (valkeystr != "") {
        std::vector<std::string> valkeys = parsePrimaryKeys(valkeystr);
        for (auto valkey : valkeys) {
            std::string valresult;
            s = db_->Get(ReadOptions(), user_cf_meta_.cf_handle_, key, &valresult);
            if (valresult != "") {
                result.push_back(valresult);
                continue;
            }

            if (primary_hdl != nullptr) {
                s = db_->Get(ReadOptions(), primary_hdl, valkey, &valresult);
                if (valresult != "") {
                    result.push_back(valresult);
                }
            }
        }   
    }

    if (result.size() > 0) {
        return 0;
    }
    return 1;
}

int MymBroker::Scan(const std::string &begin_key, int scan_length, const std::set<int> *positions,
                 std::vector<std::string> &result)
{
    // The following are configs for avoiding OOM issue
    rocksdb::ReadOptions ro;
    ro.fill_cache = false;
    ro.pin_data   = false;
    ro.readahead_size = 2 << 20;

    for (const auto& [lvl, handles] : int_cf_meta_) {
        if (handles.empty()) continue;
        for (const auto& [name, meta] : handles) {
            std::unique_ptr<rocksdb::Iterator> it(db_->NewIterator(ro, meta.cf_handle_));
            it->SeekToFirst();
            for (int i = 0; it->Valid() && i < scan_length; i++) {
                result.push_back(it->value().ToString());
                it->Next();
            }
            break;
        }
    }
    
    /*int level = 1;
    while (true) {
        std::unordered_map<std::string, ColFamMeta> level_handles = int_cf_meta_[level];
        if (level_handles.size() == 0) {
            break;
        }
        level++;

        std::vector<std::string> level_result;
        for (auto lvl_hdl : level_handles) {
            if (lvl_hdl.first.find("_secondary_") != std::string::npos) {
                continue;
            }

            if (positions != nullptr && positions->size() > 0) {
                int column_search = checkColumnSearch(lvl_hdl.second, positions);
                if (column_search == 0) {
                    continue;
                } else if (column_search == 1) {
                    auto it2 = db_->NewIterator(ReadOptions(), lvl_hdl.second.cf_handle_);
                    for (int i = 0; it2->Valid() && i < scan_length; i++) {
                        if (level_result.size() == 0) {
                            level_result.push_back(it2->value().ToString());
                        } else {
                            level_result[i] += it2->value().ToString();
                        }
                        it2->Next();
                    }
                } else {
                    auto it2 = db_->NewIterator(ReadOptions(), lvl_hdl.second.cf_handle_);
                    for (int i = 0; it2->Valid() && i < scan_length; i++) {
                        result.push_back(it2->value().ToString());
                        it2->Next();
                    }
                    break;
                }
            } else {
                auto it2 = db_->NewIterator(ReadOptions(), lvl_hdl.second.cf_handle_);
                for (int i = 0; it2->Valid() && i < scan_length; i++) {
                    if (level_result.size() == 0) {
                        level_result.push_back(it2->value().ToString());
                    } else {
                        level_result[i] += it2->value().ToString();
                    }
                    it2->Next();
                }
            }
        }
        result.insert(result.end(), level_result.begin(), level_result.end());
    }

    delete it;

    if (result.size() > 0) {
        return 0;
    }*/
    return 0;
}

int MymBroker::Insert(const std::string &key, std::string &values)
{
    Status s = db_->Put(WriteOptions(), user_cf_meta_.cf_handle_, key, values);
    if (s.ok()) {
        return 0;
    }
    return 1;
}

int MymBroker::Delete(const std::string &key)
{
    // Delete from the base CF first.
    Status s = db_->Delete(WriteOptions(), user_cf_meta_.cf_handle_, key);
    if (!s.ok()) {
        return 1;
    }

    // Propagate tombstone to every derived CF so the grove stays consistent.
    // int_cf_meta_: level → (cf_name → ColFamMeta)
    for (auto& [level, name_to_meta] : int_cf_meta_) {
        // Skip level 0 — that is the base CF itself (already deleted above).
        if (level == 0) continue;
        for (auto& [cf_name, cf_meta] : name_to_meta) {
            Status ds = db_->Delete(WriteOptions(), cf_meta.cf_handle_, key);
            if (!ds.ok()) {
                // Log and continue: partial propagation is preferable to
                // leaving the base deletion un-reflected in derived trees.
                // Full consistency can be restored via catch-up compaction.
                fprintf(stderr,
                        "[MymBroker] Delete: failed to delete key from "
                        "derived CF '%s': %s\n",
                        cf_name.c_str(), ds.ToString().c_str());
            }
        }
    }
    return 0;
}

CFPlan MymBroker::buildPlan(const std::string& root_cf)
{
    // Validate that each transformer has a schemaDescriptor
    if (options_.schemaDescriptors.size() != options_.transformers.size()) {
        throw std::runtime_error("Expected the number of transformers to equal the number of SchemaDescriptors.");
    }
    if (options_.schemaDescriptors.size() > 4) {
        throw std::runtime_error("Having more than 4 transformers is not supported.");
    }

    CFPlan plan;
    plan.root = root_cf;

     // Seed root node with transformer[0]/schema[0]
    {
        CFNode root;
        root.name = root_cf;
        root.level = 0;
        root.opts = ColumnFamilyOptions(options_);
        root.opts.transformers.clear();
        root.opts.schemaDescriptors.clear();
        if (!options_.transformers.empty())   root.opts.transformers.push_back(options_.transformers[0]);
        if (!options_.schemaDescriptors.empty()) root.opts.schemaDescriptors.push_back(options_.schemaDescriptors[0]);
        root.opts.cf_name = root_cf;

        plan.name2idx[root.name] = (int)plan.nodes.size();
        plan.nodes.push_back(std::move(root));
    }

    // BFS over layers of transformers; for each parent at layer i, create children for layer i+1
    for (size_t i = 0; i < options_.transformers.size(); ++i) {
        const auto* schema = options_.schemaDescriptors[i].get();
        const auto* transformer = options_.transformers[i].get();
        const bool has_next = (i + 1 < options_.transformers.size());

        // Grab snapshot of nodes size at start of this layer
        const size_t layer_start = 0;   // we rely on each node.level
        // Iterate all nodes at level==i
        for (size_t n = 0; n < plan.nodes.size(); ++n) {
            if (plan.nodes[n].level != (int)i) continue;

            const int parent_idx = static_cast<int>(n);
           
            // 1) Depending on schema, add children and record them into parent's destination_column_families
            std::vector<std::string> child_names;
            const auto tmask = static_cast<int>(transformer->Supports());
            
            if (tmask & static_cast<int>(mycelium::TransformerType::DISTRIBUTOR)) {
                auto* trptr = dynamic_cast<const mycelium::Distributor*>(transformer);
                const int splits = trptr->GetNumSplits();
                child_names.reserve(splits);
                for (int k = 0; k < splits; ++k) {
                    child_names.emplace_back(plan.nodes[parent_idx].name + "_split_cf_" + std::to_string(k));
                }

            } else if (tmask & static_cast<int>(mycelium::TransformerType::CONVERTER)) {
                child_names.emplace_back(make_child_name(plan.nodes[parent_idx].name, "_converted_cf"));

            } else if (tmask & static_cast<int>(mycelium::TransformerType::AUGMENTER)) {
                child_names.emplace_back(make_child_name(plan.nodes[parent_idx].name, "_indexed_data_cf"));
                // secondary index CFs (no further transformers)
                auto* trptr = dynamic_cast<const mycelium::Augmenter*>(transformer);
                size_t index_num = trptr->GetPositionedIndexKeys().size();
                for (size_t k = 0; k < index_num; ++k) {
                    child_names.emplace_back(plan.nodes[parent_idx].name + "_secondary_index_cf" + std::to_string(k));
                }

            } else if (tmask & static_cast<int>(mycelium::TransformerType::MYNOOPER)) {
                child_names.emplace_back(make_child_name(plan.nodes[parent_idx].name, "_identity_cf"));

            } else {
                // Unknown / no-op: no children
            }

            // 2) Update parent now (still safe; we haven't grown plan.nodes yet)
            {
                auto& parent = plan.nodes[parent_idx]; // re-fetch by index
                parent.children.insert(parent.children.end(), child_names.begin(), child_names.end());
                auto& dests = parent.opts.destination_column_families;
                dests.insert(dests.end(), child_names.begin(), child_names.end());
            }
  
            // 3) Append children AFTER parent updated (vector may reallocate; we won't touch parent again)
            for (const auto& cname : child_names) {
                CFNode child;
                child.name  = cname;
                child.level = static_cast<int>(i) + 1;
  
                if (cname.find("_secondary_index_cf") != std::string::npos) {
                    ColumnFamilyOptions si_opts(options_);
                    si_opts.transformers.clear();
                    si_opts.schemaDescriptors.clear();
                    si_opts.merge_operator = std::make_shared<SecondaryIndexMergeOperator>();
                    si_opts.cf_name = cname;
                    child.opts = std::move(si_opts);
                } else {
                    ColumnFamilyOptions child_opts(options_);
                    child_opts.transformers.clear();
                    child_opts.schemaDescriptors.clear();
                    if (has_next) {
                        child_opts.transformers.push_back(options_.transformers[i + 1]);
                        child_opts.schemaDescriptors.push_back(options_.schemaDescriptors[i + 1]);
                    }
                    child_opts.cf_name = cname;
                    child.opts = std::move(child_opts);
                }
  
                plan.name2idx[cname] = static_cast<int>(plan.nodes.size());
                plan.nodes.push_back(std::move(child));
            }
        }
    }

  return plan;
}

std::vector<ColumnFamilyDescriptor> MymBroker::emitDescriptors(const CFPlan& plan) {
  std::vector<ColumnFamilyDescriptor> descs;
  descs.reserve(plan.nodes.size());
  for (const auto& node : plan.nodes) {
    descs.emplace_back(node.name, node.opts);  // parent already has destination_column_families populated
  }
  return descs;
}

void MymBroker::saveColFamHandlesByName(const CFPlan& plan,
                                        const std::vector<ColumnFamilyDescriptor>& descs,
                                        const std::vector<ColumnFamilyHandle*>& handles,
                                        const std::string& root_cf)
{
    assert(descs.size() == handles.size());

    owned_cf_handles_.insert(owned_cf_handles_.end(), handles.begin(), handles.end());

    // Build name -> handle map from the parallel vectors (Open/Create guarantee order)
    std::unordered_map<std::string, ColumnFamilyHandle*> hmap;
    hmap.reserve(descs.size());
    for (size_t i = 0; i < descs.size(); ++i) {
        auto res = hmap.emplace(descs[i].name, handles[i]);
        if (!res.second) {
            throw std::runtime_error("Duplicate CF name in descriptors: " + descs[i].name);
        }
    }

    // Root must exist
    auto it_root = hmap.find(root_cf);
    assert(it_root != hmap.end());

    // Build the “logical level 0” metadata for the user CF
    std::set<int> all_cols;
    for (int i = 0; i < options_.num_columns; ++i) all_cols.insert(i);

    ColumnFamilyHandle* root_handle = it_root->second;

    // Prefer constructing in-place in the map to avoid redundant copies
    int_cf_meta_.clear();
    int_cf_meta_[0].emplace(root_cf, ColFamMeta(root_cf, /*level=*/0, root_handle, all_cols));
    user_cf_meta_ = int_cf_meta_[0].at(root_cf); // or remove user_cf_meta_ entirely

    // BFS over the plan by name
    std::queue<std::string> q;
    std::unordered_set<std::string> visited;
    visited.reserve(plan.nodes.size());

    q.push(root_cf);
    visited.insert(root_cf);

    int level = 0;
    while (!q.empty()) {
        size_t qs = q.size();
        int next_level = level + 1;

        for (size_t i = 0; i < qs; ++i) {
          std::string src_name = q.front(); 
          q.pop();
    
          const CFNode& src_node = plan.nodes.at(plan.name2idx.at(src_name));
          const auto& src_cols   = int_cf_meta_[level].at(src_name).GetColumns();
    
          const auto& children = src_node.children;
          auto splitColGroups = splitColumns(src_cols, static_cast<int>(children.size()));
    
          for (size_t k = 0; k < children.size(); ++k) {
            const std::string& child_name = children[k];
            auto it_h = hmap.find(child_name);
            assert(it_h != hmap.end() && "Plan references CF name not in descs/handles");

            // If child already exists, decide whether to skip or assert.
            if (!visited.insert(child_name).second) {
                continue; // already processed
            }
    
            int_cf_meta_[next_level].emplace(
                child_name,
                ColFamMeta(child_name, next_level, it_h->second, splitColGroups[k]));
    
            q.push(child_name);
          }
        }
        
        level = next_level;
    }
}

std::vector<std::set<int>> MymBroker::splitColumns(std::set<int> srccols, int splits) {
    std::vector<std::set<int>> result(splits);

    if (srccols.empty() || splits <= 0) {
        return result;
    }

    int total = srccols.size();
    int base_size = total / splits;
    int remainder = total % splits;

    auto it = srccols.begin();
    for (int i = 0; i < splits; ++i) {
        int this_split_size = base_size + (i < remainder ? 1 : 0);  // distribute remainder

        for (int j = 0; j < this_split_size && it != srccols.end(); ++j, ++it) {
            result[i].insert(*it);
        }
    }

    return result;
}

void MymBroker::getColPositions(int divide, int start, int total_cols, std::set<int>& col_pos)
{
    int share = total_cols/divide;
    for (int i = start*share; i < start*share + share; i++) {
        col_pos.insert(i);
    }
}

int MymBroker::checkColumnSearch(ColFamMeta& cfmeta, const std::set<int>* column_positions)
{
    // if colPositions is empty, it is entire row so it covers everything column in 
    // the query
    if (cfmeta.colPositions_.size() == 0) {
        return 2;
    }

    int covered = 0;
    for (auto column_position : *column_positions) {
        if (cfmeta.colPositions_.find(column_position) != cfmeta.colPositions_.end()) {
            covered = 2;
        } else {
            if (covered == 2) {
                return 1;
            }
        }
    }

    return covered;
}

std::vector<std::string> MymBroker::parsePrimaryKeys(const std::string& keystr)
{
    std::vector<std::string> primary_keys;
    std::istringstream stream(keystr);
    std::string key;
    
    while (std::getline(stream, key, ',')) {
        primary_keys.push_back(key);
    }

    return primary_keys;
}

}