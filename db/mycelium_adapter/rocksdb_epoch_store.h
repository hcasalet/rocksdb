#pragma once
// db/mycelium_adapter/rocksdb_epoch_store.h
//
// RocksDB concrete implementation of mycelium::EpochStore.
//
// P3 implementation: in-memory map guarded by a std::mutex.  This is
// sufficient for the admission-control use-case (the epoch state guides
// deferral decisions but is not crash-critical).
//
// P4 upgrade path: replace the in-memory map with a TablePropertiesCollector
// that serialises TransformEpochTracker into SST user properties under the
// key "mycelium.epoch".  Load() would then read via
// DB::GetPropertiesOfAllTables() or the TableProperties embedded in the
// SST metadata.  The public interface (Load/Save/Evict) stays identical.

#include <cstdint>
#include <mutex>
#include <unordered_map>

#include "db/compaction/transform_epoch_tracker.h"
#include "mycelium/compaction_hook.h"

namespace ROCKSDB_NAMESPACE {

class RocksDBEpochStore final : public mycelium::EpochStore {
 public:
  // Load the epoch tracker for [file_id] into [*out].
  // Returns Status::OK() if a record exists.
  // Returns Status::Error("not found") on the first compaction for this file.
  mycelium::Status Load(uint64_t               file_id,
                        mycelium::TransformEpochTracker* out) const override;

  // Persist (in memory) the epoch tracker for [file_id].
  mycelium::Status Save(uint64_t                          file_id,
                        const mycelium::TransformEpochTracker& tracker) override;

  // Remove the epoch record when the SST is deleted / compacted away.
  mycelium::Status Evict(uint64_t file_id) override;

 private:
  mutable std::mutex mu_;
  std::unordered_map<uint64_t, mycelium::TransformEpochTracker> epochs_;
};

}  // namespace ROCKSDB_NAMESPACE
