#pragma once
// P2 shim: forwards to mycelium/transform_epoch_tracker.h
#include "rocksdb/rocksdb_namespace.h"
#include "mycelium/transform_epoch_tracker.h"

namespace ROCKSDB_NAMESPACE {
  using mycelium::TransformState;
  using mycelium::TransformEpochTracker;
}  // namespace ROCKSDB_NAMESPACE
