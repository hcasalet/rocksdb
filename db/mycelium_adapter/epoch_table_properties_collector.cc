// db/mycelium_adapter/epoch_table_properties_collector.cc

#include "db/mycelium_adapter/epoch_table_properties_collector.h"

#include "mycelium/transform_epoch_tracker.h"

namespace ROCKSDB_NAMESPACE {

Status EpochIntTblPropCollector::Finish(UserCollectedProperties* properties) {
  // Load the tracker that was built up during this compaction for this file.
  mycelium::TransformEpochTracker tracker;
  auto ms = store_->Load(file_number_, &tracker);
  if (!ms.ok()) {
    // No epoch state recorded for this file (e.g. passthrough compaction with
    // no transforms scheduled).  Write nothing so the next compaction treats
    // it as a first-time file.
    return Status::OK();
  }

  std::string encoded;
  tracker.EncodeTo(&encoded);
  (*properties)[kEpochPropertyKey] = std::move(encoded);
  return Status::OK();
}

}  // namespace ROCKSDB_NAMESPACE
