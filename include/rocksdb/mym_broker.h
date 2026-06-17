#pragma once

#include <set>
#include <queue>
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
    ColumnFamilyHandle* cf_handle_ = nullptr;
    std::set<int> colPositions_;

  public:
    ColFamMeta() {}
    ColFamMeta(std::string cf_name, int logical_level, ColumnFamilyHandle* cf_handle,
               std::set<int> col_positions) 
        : cfName_(cf_name), logical_level_(logical_level), 
          cf_handle_(cf_handle), colPositions_(std::move(col_positions)) {}
    ~ColFamMeta() = default;
    int GetLogicalLevel() { return logical_level_; }
    std::set<int> GetColumns() { return colPositions_; }
};

struct CFNode {
    std::string name;
    ColumnFamilyOptions opts;
    std::vector<std::string> children;  // names only
    int level = 0;
};
  
struct CFPlan {
    std::string root;
    std::vector<CFNode> nodes;                        // stable emission order
    std::unordered_map<std::string, int> name2idx;    // name -> nodes[] index
};

class MymBroker {
    public :
        MymBroker(const std::string& cfname,
                  bool cf_created,
                  const char *dbfilepath,
                  const Options& options,
                  int num_splits);
        int Read(const std::string &key, const std::set<int>* positions, std::string &result);

        int Scan(const std::string &begin_key, int scan_length, const std::set<int> *positions,
                 std::vector<std::string> &result);

        int Insert(const std::string &key, const std::string &values);

        int Delete(const std::string &key);

        int IndexRead(const std::string &key, const std::set<int>* positions, std::vector<std::string> &result);

        ~MymBroker() {
            for (auto* h : owned_cf_handles_) {
                delete h;
            }
            owned_cf_handles_.clear();
            int_cf_meta_.clear();
            delete db_;
        };

        static inline std::string make_child_name(const std::string& parent, std::string_view suffix) {
            return parent + std::string(suffix);
        }

        // ── Test seam ──────────────────────────────────────────────────────────
        // Expose raw handles so integration tests can call CompactRange and
        // do per-CF Gets without going through MymBroker's routing logic.
        DB* GetDB() const { return db_; }
        ColumnFamilyHandle* GetCFHandle(const std::string& name) const {
            for (const auto& [level, cf_map] : int_cf_meta_) {
                auto it = cf_map.find(name);
                if (it != cf_map.end()) return it->second.cf_handle_;
            }
            return nullptr;
        }

    private:
        DB *db_;
        Options options_;
        WriteOptions write_options_;  // cached; avoids per-call construction
        ColFamMeta user_cf_meta_;
        std::unordered_map<int, std::unordered_map<std::string, ColFamMeta>> int_cf_meta_;
        // to track ColumnFamilyHandles in order to delete
        std::vector<rocksdb::ColumnFamilyHandle*> owned_cf_handles_;
        
        CFPlan buildPlan(const std::string& root_cf);
        std::vector<ColumnFamilyDescriptor> emitDescriptors(const CFPlan& plan);
        void saveColFamHandlesByName(const CFPlan& plan,
                                    const std::vector<ColumnFamilyDescriptor>& descs,
                                    const std::vector<ColumnFamilyHandle*>& handles,
                                    const std::string& root_cf);
        std::vector<std::set<int>> splitColumns(std::set<int> srccols, int splits);
        void getColPositions(int divide, int start, int total_cols, std::set<int>& col_pos);
        int checkColumnSearch(ColFamMeta& cfmeta, const std::set<int>* column_positions);
        std::vector<std::string> parsePrimaryKeys(const std::string& keystr);
};  

}