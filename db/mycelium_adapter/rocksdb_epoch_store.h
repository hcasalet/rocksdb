#pragma once
// db/mycelium_adapter/rocksdb_epoch_store.h
//
// RocksDB concrete implementation of mycelium::EpochStore.
//
// P3 implementation: in-memory map guarded by a std::mutex.
//
// P4: PreLoad() seeds the map from the encoded bytes stored in an SST's
// user-collected table properties ("mycelium.epoch").  The write-back
// side uses EpochIntTblPropCollector (see epoch_table_properties_collector.h),
// which is injected per-output-file in CompactionJob::OpenCompactionOutputFile
// and calls Load() → EncodeTo() → Finish() when the SST is closed.

#include <cstdint>
#include <mutex>
#include <string_view>
#include <unordered_map>

#include "mycelium/transform_epoch_tracker.h"
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

  // Seed the map from the encoded bytes stored in the SST's
  // "mycelium.epoch" user-collected property.  Called once per input
  // file at the start of CompactionJob::Run().  A malformed or empty
  // blob is silently ignored (first-time compaction for this SST).
  void PreLoad(uint64_t file_id, std::string_view encoded);

 private:
  mutable std::mutex mu_;
  std::unordered_map<uint64_t, mycelium::TransformEpochTracker> epochs_;
};

}  // namespace ROCKSDB_NAMESPACE
