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

void RocksDBEpochStore::PreLoad(uint64_t file_id, std::string_view encoded) {
  if (encoded.empty()) return;
  mycelium::TransformEpochTracker tracker;
  auto s = tracker.DecodeFrom(encoded);
  if (!s.ok()) return;          // malformed blob — treat as first-time compaction
  std::lock_guard<std::mutex> lk(mu_);
  // Only seed if not already present (a later Save() from the running job wins).
  epochs_.emplace(file_id, std::move(tracker));
}

}  // namespace ROCKSDB_NAMESPACE
