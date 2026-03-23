#pragma once
// P2 shim: forwards to mycelium/compaction_slack_estimator.h
#include "rocksdb/rocksdb_namespace.h"
#include "mycelium/compaction_slack_estimator.h"

namespace ROCKSDB_NAMESPACE {
  using mycelium::CpuTimer;
  using mycelium::CompactionSlackEstimator;
}  // namespace ROCKSDB_NAMESPACE
