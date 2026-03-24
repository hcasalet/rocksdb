// db/mycelium_adapter/rocksdb_grove_manager.cc

#include "db/mycelium_adapter/rocksdb_grove_manager.h"

#include <cstdio>

#include "rocksdb/write_batch.h"

namespace ROCKSDB_NAMESPACE {

RocksDBGroveManager::RocksDBGroveManager(
    DB*                              db,
    std::vector<ColumnFamilyHandle*> derived_handles)
    : db_(db), derived_handles_(std::move(derived_handles)) {}

mycelium::Status RocksDBGroveManager::PropagateDelete(std::string_view key) {
  Slice key_slice(key.data(), key.size());
  mycelium::Status first_error = mycelium::Status::OK();

  for (ColumnFamilyHandle* handle : derived_handles_) {
    Status s = db_->Delete(WriteOptions(), handle, key_slice);
    if (!s.ok()) {
      // Log and continue: partial propagation is better than aborting.
      // Grove consistency can be restored via catch-up compaction.
      fprintf(stderr,
              "[RocksDBGroveManager] PropagateDelete: failed for CF '%s': %s\n",
              handle->GetName().c_str(), s.ToString().c_str());
      if (first_error.ok()) {
        first_error = mycelium::Status::Error(s.ToString());
      }
    }
  }
  return first_error;
}

std::vector<std::string> RocksDBGroveManager::DerivedCFNames() const {
  std::vector<std::string> names;
  names.reserve(derived_handles_.size());
  for (const ColumnFamilyHandle* h : derived_handles_) {
    names.push_back(h->GetName());
  }
  return names;
}

}  // namespace ROCKSDB_NAMESPACE
