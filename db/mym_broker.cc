#include <queue>
#include <sstream>
#include "rocksdb/mym_broker.h"
#include "transformer/distribute/distributor.h"
#include "transformer/convert/converter.h"
#include "transformer/augment/augmenter.h"
#include "transformer/identity/mynooper.h"
#include "transformer/distribute/protobuf_distributor_schema.h"
#include "transformer/convert/json2protobuf_schema.h"
#include "transformer/convert/protobuf2flatbuffers_schema.h"

namespace ROCKSDB_NAMESPACE {

MymBroker::MymBroker(const std::string& cfname,
                     bool cf_created,
                     const char *dbfilepath,
                     Options& options)
    : options_(options)
{
    if (options.schemaDescriptors.size() > 4) {
        throw std::runtime_error("Having more than 4 transformers is not supported.");
    }

    bool split{false}, convert{false}, augment{false};
    int num_splits = 1;
    if (auto distributor = std::dynamic_pointer_cast<ProtobufDistributorSchema>(options.schemaDescriptors[0])) {
        num_splits = distributor->GetNumSplits();
        split = true;
    } else if (std::dynamic_pointer_cast<Protobuf2FlatbuffersSchema>(options.schemaDescriptors[0]) ||
        std::dynamic_pointer_cast<Json2ProtobufSchema>(options.schemaDescriptors[0])) {
        convert = true;
    } else if (auto augmenter = std::dynamic_pointer_cast<ProtobufAugmenterSchema>(options.schemaDescriptors[0])) {
        augment = true;
    }

    std::vector<ColumnFamilyDescriptor> column_family_descriptors;
    genIntColFamDescriptors(cfname, column_family_descriptors);
    std::vector<ColumnFamilyHandle*> cf_handles;
    Status s;

    if (!cf_created) {
        s = DB::Open(options_, dbfilepath, &db_);
        assert(s.ok());

        s = db_->CreateColumnFamilies(column_family_descriptors, &cf_handles);
        assert(s.ok());

        s = db_->AddTransformingDestinationCfds(cfname, split, convert, augment, false, num_splits);
        assert(s.ok());
    } else {
        column_family_descriptors.push_back(ColumnFamilyDescriptor(
                    kDefaultColumnFamilyName, ColumnFamilyOptions(options_)));
        s = DB::Open(options_, dbfilepath, column_family_descriptors, &cf_handles, &db_);
        assert(s.ok());

        s = db_->AddTransformingDestinationCfds(cfname, split, convert, augment, false, num_splits);
        assert(s.ok());
    }

    saveIntColFamHandles(column_family_descriptors, cf_handles, cfname, num_splits);
}

int MymBroker::Read(const std::string &key, const std::set<int>* positions, std::string &result)
{
    Status s = db_->Get(ReadOptions(), user_cf_meta_.cf_handle_, key, &result);
    if (s.ok()) {
        return 0;
    }

    int level = 1;
    while (true) {
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
    }
    
    if (result != "") {
        return 0;
    }
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

    ColumnFamilyHandle* primary_hdl;
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

            s = db_->Get(ReadOptions(), primary_hdl, valkey, &valresult);
            if (valresult != "") {
                result.push_back(valresult);
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
    int levels = int_cf_meta_.size() + 1;

    auto it = db_->NewIterator(ReadOptions(), user_cf_meta_.cf_handle_);
    it->SeekToFirst();
    for (int i = 0; it->Valid() && i < scan_length; i++) {
        result.push_back(it->value().ToString());
        it->Next();
    }
    
    int level = 1;
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

    if (result.size() > 0) {
        return 0;
    }
    return 1;
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
    Status s = db_->Delete(WriteOptions(), user_cf_meta_.cf_handle_, key);
    if (s.ok()) {
        return 0;
    }
    return 1;
}

void MymBroker::genIntColFamDescriptors(const std::string& cfname,
                                   std::vector<ColumnFamilyDescriptor>& column_families)
{
    if (options_.schemaDescriptors.size() != options_.transformers.size()) {
        throw std::runtime_error("Expected the number of transformers to equal the number of SchemaDescriptors.");
    }

    // Generate ColumnFamilyDescriptor for user-facing column family
    ColumnFamilyOptions cf_opts(options_);
    if (options_.schemaDescriptors.size() == 0) {
        column_families.push_back(ColumnFamilyDescriptor(cfname, cf_opts));
        return;
    }

    cf_opts.schemaDescriptors.clear();
    cf_opts.schemaDescriptors.push_back(options_.schemaDescriptors[0]);
    cf_opts.transformers.clear();
    cf_opts.transformers.push_back(options_.transformers[0]);
    column_families.push_back(ColumnFamilyDescriptor(cfname, cf_opts));

    std::queue<std::pair<std::string, ColumnFamilyOptions>> colfamqueue;
    colfamqueue.push(std::make_pair(cfname, cf_opts));

    for (size_t i = 0; i < options_.schemaDescriptors.size(); i++) {
        size_t qsize = colfamqueue.size();

        for (size_t j = 0; j < qsize; j++) {
            auto front = colfamqueue.front();
            auto src_cf_name = front.first;
            auto src_cf_opts = front.second;
            colfamqueue.pop();
            createDestinationColFamDescriptors(colfamqueue, src_cf_name, src_cf_opts, column_families, i);
        }
    }
}

void MymBroker::createDestinationColFamDescriptors(std::queue<std::pair<std::string, ColumnFamilyOptions>>& cfq,
                                                   const std::string& cfname,
                                                   ColumnFamilyOptions& cfopts,
                                                   std::vector<ColumnFamilyDescriptor>& column_families,
                                                   size_t pos) {
    auto makeCfName = [&](const std::string& suffix) {
        return cfname + suffix;
    };

    auto schema = options_.schemaDescriptors[pos];
    
    ColumnFamilyOptions int_cf_opts(options_);
    int_cf_opts.transformers.clear();
    int_cf_opts.schemaDescriptors.clear();
    if (pos + 1 < options_.transformers.size()) {
        int_cf_opts.SetTransformerType(options_.transformers[pos+1]->Supports());
        int_cf_opts.transformers.push_back(options_.transformers[pos+1]);
        int_cf_opts.schemaDescriptors.push_back(options_.schemaDescriptors[pos+1]);
    } else {
        int_cf_opts.SetTransformerType(TransformerType::NOTRANSFORMATION);
    }
    
    if (static_cast<int>(schema->SupportsTransformerType()) & static_cast<int>(TransformerType::DISTRIBUTOR)) {
        int num_splits = schema->GetNumSplits();
        
        for (int k = 0; k < num_splits; k++) {
            std::string cfname_child = cfname + "_split_cf_" + std::to_string(k);
            column_families.push_back(ColumnFamilyDescriptor(cfname_child, int_cf_opts));
            cfq.push(std::make_pair(cfname_child, int_cf_opts));
            cfopts.destination_column_families.push_back(cfname_child);
        }
    } else if (static_cast<int>(schema->SupportsTransformerType()) & static_cast<int>(TransformerType::CONVERTER)) {
        auto converted = makeCfName("_converted_cf");
        column_families.push_back(ColumnFamilyDescriptor(converted, int_cf_opts));
        cfq.push(std::make_pair(converted, int_cf_opts));
        cfopts.destination_column_families.push_back(converted);
    } else if (static_cast<int>(schema->SupportsTransformerType()) & static_cast<int>(TransformerType::AUGMENTER)) {
        auto primaryindex = makeCfName("_indexed_data_cf");
        column_families.push_back(ColumnFamilyDescriptor(primaryindex, int_cf_opts));
        cfq.push(std::make_pair(primaryindex, int_cf_opts));
        cfopts.destination_column_families.push_back(primaryindex);

        for (size_t k=0; k < schema->GetIndexKeys().size(); k++) {
            ColumnFamilyOptions secondary_index_opts(options_);
            secondary_index_opts.SetTransformerType(TransformerType::NOTRANSFORMATION);
            secondary_index_opts.merge_operator = std::make_shared<SecondaryIndexMergeOperator>();
            secondary_index_opts.schemaDescriptors.clear();
            secondary_index_opts.transformers.clear();

            std::string secondaryindex = cfname + "_secondary_index_cf" + std::to_string(k);
            column_families.push_back(rocksdb::ColumnFamilyDescriptor(secondaryindex, secondary_index_opts));
            cfopts.destination_column_families.push_back(secondaryindex);
        }
    } else if (static_cast<int>(schema->SupportsTransformerType()) & static_cast<int>(TransformerType::MYNOOPER)) {
        int_cf_opts.SetTransformerType(TransformerType::NOTRANSFORMATION);
        auto identity = makeCfName("_identity_cf");
        column_families.push_back(ColumnFamilyDescriptor(identity, int_cf_opts));
        cfopts.destination_column_families.push_back(identity);
    } else {
        // handle unknown type
        return; 
    }
}

void MymBroker::saveIntColFamHandles(std::vector<ColumnFamilyDescriptor>& column_family_descriptors,
                                std::vector<ColumnFamilyHandle*> handles,
                                std::string cfname, int num_splits)
{
    assert(column_family_descriptors.size()==handles.size());

    int pre_group = 1;
    int group = num_splits;
    int level = 1;
    int roundcount = 0;
    for (size_t i = 0; i < column_family_descriptors.size(); i++) {
        if (column_family_descriptors[i].name == kDefaultColumnFamilyName) {
            continue;
        }

        if (column_family_descriptors[i].name == cfname) {
            std::set<int> cf_colset;
            user_cf_meta_ = ColFamMeta(cfname, 0, handles[i], std::move(cf_colset));
            continue;
        }

        if (num_splits == 1) {
            std::set<int> cf_colset;
            int_cf_meta_[1][column_family_descriptors[i].name] = ColFamMeta(
                column_family_descriptors[i].name, 1, handles[i], std::move(cf_colset)
            );
        } else {
            std::set<int> colpos;
            getColPositions(group, i - pre_group, options_.num_columns, colpos);
            int_cf_meta_[level][column_family_descriptors[i].name] = ColFamMeta(
                column_family_descriptors[i].name, level, handles[i], std::move(colpos)                    
            );
            roundcount++;

            if (roundcount == group) {
                roundcount = 0;
                pre_group += group;
                group *= num_splits;
                level += 1;
            }
        }
    }
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