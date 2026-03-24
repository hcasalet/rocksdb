// db/mycelium_adapter/rocksdb_epoch_store.cc

#include "db/mycelium_adapter/rocksdb_epoch_store.h"

namespace ROCKSDB_NAMESPACE {

mycelium::Status RocksDBEpochStore::Load(
    uint64_t                        file_id,
    mycelium::TransformEpochTracker* out) const {
  std::lock_guard<std::mutex> lk(mu_);
  auto it = epochs_.find(file_id);
  if (it == epochs_.end()) {
    return mycelium::Status::Error("not found");
  }
  *out = it->second;
  return mycelium::Status::OK();
}

mycelium::Status RocksDBEpochStore::Save(
    uint64_t                             file_id,
    const mycelium::TransformEpochTracker& tracker) {
  std::lock_guard<std::mutex> lk(mu_);
  epochs_[file_id] = tracker;
  return mycelium::Status::OK();
}

mycelium::Status RocksDBEpochStore::Evict(uint64_t file_id) {
  std::lock_guard<std::mutex> lk(mu_);
  epochs_.erase(file_id);
  return mycelium::Status::OK();
}

}  // namespace ROCKSDB_NAMESPACE
