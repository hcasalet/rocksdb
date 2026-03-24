// db/mycelium_adapter/rocksdb_defer_callback.cc

#include "db/mycelium_adapter/rocksdb_defer_callback.h"

#include "db/column_family.h"

namespace ROCKSDB_NAMESPACE {

RocksDBDeferCallback::RocksDBDeferCallback(VersionSet* versions,
                                           ScheduleFn  schedule_fn)
    : versions_(versions), schedule_fn_(std::move(schedule_fn)) {}

mycelium::Status RocksDBDeferCallback::ScheduleDeferred(
    const std::vector<std::string>& cf_names,
    const std::vector<uint64_t>&    /*file_numbers*/) {
  ColumnFamilySet* cfs = versions_->GetColumnFamilySet();
  for (const std::string& name : cf_names) {
    ColumnFamilyData* cfd = cfs->GetColumnFamily(name);
    if (cfd == nullptr || cfd->IsDropped()) {
      // CF may have been dropped between the compaction and install step.
      continue;
    }
    schedule_fn_(cfd);
  }
  return mycelium::Status::OK();
}

}  // namespace ROCKSDB_NAMESPACE
