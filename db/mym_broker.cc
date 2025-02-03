#include <queue>
#include <sstream>
#include "rocksdb/mym_broker.h"
#include "transformer/distributor.h"
#include "transformer/converter.h"
#include "transformer/augmenter.h"
#include "transformer/mynooper.h"

namespace ROCKSDB_NAMESPACE {

MymBroker::MymBroker(const std::string& cfname,
                     bool cf_created,
                     const char *dbfilepath,
                     Options& options,
                     TransformerData& transformer_data)
    : options_(options)
{
    bool split{false}, convert{false}, augment{false};
    int num_splits = 1;
    if (auto distributor = dynamic_cast<DistributorData*>(&transformer_data)) {
        num_splits = distributor->splits;
        split = true;
    } else if (auto converter = dynamic_cast<ConverterData*>(&transformer_data)) {
        convert = true;
    } else if (auto augmenter = dynamic_cast<AugmenterData*>(&transformer_data)) {
        augment = true;
    }

    std::vector<ColumnFamilyDescriptor> column_family_descriptors;
    genIntColFamDescriptors(cfname, column_family_descriptors, transformer_data);
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
                                   std::vector<ColumnFamilyDescriptor>& column_families,
                                   TransformerData& transformer_data)
{
    column_families.push_back(ColumnFamilyDescriptor(cfname, ColumnFamilyOptions(options_)));

    if (auto distributor = dynamic_cast<DistributorData*>(&transformer_data)) {
        bool lastSplitLevel = false;
        int num_splits = distributor->splits;
        std::string prefix = cfname + "_sys_cf";
        std::queue<int> parents;
        parents.push(options_.num_columns);

        int total_levels = options_.num_levels;
        for (int level = 1; level < total_levels - 2; level++) {
            int queueLen = parents.size();

            options_.num_levels = options_.num_levels - level;
            if (level == total_levels - 3) {
                lastSplitLevel = true;
                options_.SetTransformerType(TransformerType::NOTRANSFORMATION);
                options_.level0_file_num_compaction_trigger = 4;
                options_.compaction_pri = kByCompensatedSize;
            }
            for (int j = 0; j < queueLen; j++) {
                int parent_cols = parents.front();
                parents.pop();
                if (parent_cols < 2) {
                    continue;
                }

                if (!lastSplitLevel && parent_cols <= num_splits) {
                    lastSplitLevel = true;
                    options_.SetTransformerType(TransformerType::NOTRANSFORMATION);
                }
                for (int k = 0; k < num_splits; k++) {
                    int child = parent_cols/(num_splits-k);
                    if (child == 0) {
                        child = 1;
                    }

                    if (child < num_splits) {
                        lastSplitLevel = true;
                        options_.SetTransformerType(TransformerType::NOTRANSFORMATION);
                    }
                    std::string cfname_child = prefix + "_L" + std::to_string(level) + "_G" + std::to_string(j*num_splits+k);
                    column_families.push_back(ColumnFamilyDescriptor(cfname_child, ColumnFamilyOptions(options_)));

                    options_.SetTransformerType(TransformerType::DISTRIBUTOR);

                    if (!lastSplitLevel && child >= num_splits) {
                        parents.push(child);
                    }
                    if (k < num_splits-1) {
                        parent_cols -= child;
                        if (parent_cols == 0) {
                            break;
                        }
                    }
                }
            }
        }
    } else if (auto converter = dynamic_cast<ConverterData*>(&transformer_data)) {
        options_.SetTransformerType(TransformerType::NOTRANSFORMATION);
        column_families.push_back(ColumnFamilyDescriptor(
                    cfname+"_converted_cf", ColumnFamilyOptions(options_)));

    } else if (auto augmenter = dynamic_cast<AugmenterData*>(&transformer_data)) {
        options_.SetTransformerType(TransformerType::NOTRANSFORMATION);
        column_families.push_back(ColumnFamilyDescriptor(
                    cfname+"_indexed_data_cf", ColumnFamilyOptions(options_)));
        options_.merge_operator = std::make_shared<SecondaryIndexMergeOperator>();
        column_families.push_back(rocksdb::ColumnFamilyDescriptor(
                    cfname+"_secondary_index_cf", ColumnFamilyOptions(options_)));

    } else if (auto mynooper = dynamic_cast<MynooperData*>(&transformer_data)) {
        options_.SetTransformerType(TransformerType::NOTRANSFORMATION);
        column_families.push_back(ColumnFamilyDescriptor(
                    cfname+"_identity_cf", ColumnFamilyOptions(options_)));
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