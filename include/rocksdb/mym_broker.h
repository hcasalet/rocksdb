#pragma once

#include <set>
#include "rocksdb/db.h"
#include "rocksdb/merge_operator.h"

namespace ROCKSDB_NAMESPACE {

class SecondaryIndexMergeOperator : public AssociativeMergeOperator {
public:
    bool Merge(const rocksdb::Slice& key, 
               const rocksdb::Slice* existing_value,
               const rocksdb::Slice& value,
               std::string* new_value,
               rocksdb::Logger* logger) const override {
        // If there's an existing value, start with it; otherwise, initialize as empty
        std::string merged_value = existing_value ? existing_value->ToString() : "";

        // Convert the new primary key to a string
        std::string new_primary_key = value.ToString();

        // If deduplication is needed
        if (merged_value.find(new_primary_key) == std::string::npos) {
            if (!merged_value.empty()) {
                merged_value += ",";  // Add a separator before appending
            }
            merged_value += new_primary_key;  // Append the new primary key
        }

        // Return the merged result
        *new_value = merged_value;
        return true;
    }

    const char* Name() const override { return "SecondaryIndexMergeOperator"; }
};

class ColFamMeta {
  friend class MymBroker;

  protected:
    std::string cfName_;
    int logical_level_;
    ColumnFamilyHandle* cf_handle_;
    std::set<int>* colPositions_;

  public:
    ColFamMeta() {}
    ColFamMeta(std::string cf_name, int logical_level, ColumnFamilyHandle* cf_handle,
               std::set<int>* col_positions) 
        : cfName_(cf_name), logical_level_(logical_level), 
          cf_handle_(cf_handle), colPositions_(col_positions) {}
};

class MymBroker {
    public :
        MymBroker(const std::string& cfname,
                  bool cf_created,
                  const char *dbfilepath,
                  Options& options,
                  TransformerData& transformer_data);
        int Read(const std::string &key, const std::set<int>* positions, std::string &result);

        int Scan(const std::string &begin_key, int scan_length, const std::set<int> *positions,
                 std::vector<std::string> &result);

        int Insert(const std::string &key, std::string &values);

        int Delete(const std::string &key);

        ~MymBroker() {};
    
    private:
        DB *db_;
        Options options_;
        ColFamMeta user_cf_meta_;
        std::map<int, std::map<std::string, ColFamMeta>> int_cf_meta_;
        TransformerData transformer_data_;

        void genIntColFamDescriptors(const std::string& cfname,
                                     std::vector<ColumnFamilyDescriptor>& column_families,
                                     TransformerData& transformer_data);
        void saveIntColFamHandles(std::vector<ColumnFamilyDescriptor>& column_family_descriptors,
                                  std::vector<ColumnFamilyHandle*> handles,
                                  std::string cfname, int num_splits);
        void getColPositions(int divide, int start, int total_cols, std::set<int> col_pos);
        int checkColumnSearch(ColFamMeta& cfmeta, const std::set<int>* column_positions);
};  

}