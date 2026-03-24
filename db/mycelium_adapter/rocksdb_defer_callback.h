#pragma once
// db/mycelium_adapter/rocksdb_defer_callback.h
//
// RocksDB concrete implementation of mycelium::DeferCallback.
//
// When the admission / scheduling layer defers transforms for a set of column
// families, this adapter looks up each CF by name in the VersionSet and calls
// the provided schedule function (typically DBImpl::SchedulePendingCompaction).
//
// Threading: ScheduleDeferred is called from CompactionJob::InstallCompactionResults
// while db_mutex_ is held.  The schedule_fn must also be called under the mutex.
// The caller is responsible for ensuring the invariant.

#include <functional>
#include <string>
#include <vector>

#include "db/version_set.h"
#include "mycelium/compaction_hook.h"

namespace ROCKSDB_NAMESPACE {

class ColumnFamilyData;

class RocksDBDeferCallback final : public mycelium::DeferCallback {
 public:
  // schedule_fn  — invoked once per deferred CF.  Typically a lambda that
  //                captures DBImpl* and calls SchedulePendingCompaction(cfd).
  //                Called under db_mutex_ — must not re-acquire it.
  // versions     — non-owning; used to resolve CF names → ColumnFamilyData*.
  //                Must outlive this callback.
  using ScheduleFn = std::function<void(ColumnFamilyData*)>;

  RocksDBDeferCallback(VersionSet* versions, ScheduleFn schedule_fn);

  // For each name in cf_names, look up the ColumnFamilyData* and call
  // schedule_fn.  Unknown names are silently skipped (the CF may have been
  // dropped between the compaction and the install step).
  mycelium::Status ScheduleDeferred(
      const std::vector<std::string>& cf_names,
      const std::vector<uint64_t>&    file_numbers) override;

 private:
  VersionSet* versions_;     // non-owning
  ScheduleFn  schedule_fn_;
};

}  // namespace ROCKSDB_NAMESPACE
