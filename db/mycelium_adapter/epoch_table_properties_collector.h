// db/mycelium_adapter/epoch_table_properties_collector.h
//
// P4: EpochIntTblPropCollector — writes TransformEpochTracker state into each
// output SST's user-collected table properties under "mycelium.epoch".
//
// Usage (inside CompactionJob::OpenCompactionOutputFile):
//
//   IntTblPropCollectorFactories augmented;
//   for (const auto& f : *cfd->int_tbl_prop_collector_factories())
//     augmented.emplace_back(
//         std::make_unique<DelegatingIntTblPropCollectorFactory>(f.get()));
//   augmented.emplace_back(
//       std::make_unique<EpochIntTblPropCollectorFactory>(epoch_store_, file_number));
//
//   TableBuilderOptions tboptions_epoch(..., &augmented, ...);
//   outputs.NewBuilder(tboptions_epoch, i);
//   // augmented may be destroyed after NewBuilder returns.
//
// The key written to UserCollectedProperties is kEpochPropertyKey.

#pragma once

#include <cstdint>
#include <string>

#include "db/mycelium_adapter/rocksdb_epoch_store.h"
#include "db/table_properties_collector.h"
#include "rocksdb/table_properties.h"

namespace ROCKSDB_NAMESPACE {

// ── Property key ─────────────────────────────────────────────────────────────
inline constexpr const char* kEpochPropertyKey = "mycelium.epoch";

// ── EpochIntTblPropCollector ──────────────────────────────────────────────────
//
// Per-SST collector.  InternalAdd() is a no-op; at Finish() it loads the
// TransformEpochTracker for the output file from the epoch store, encodes it,
// and writes it to the SST's user-collected properties.
class EpochIntTblPropCollector final : public IntTblPropCollector {
 public:
  // @param store        The epoch store for this compaction job (borrowed).
  // @param file_number  RocksDB file number of the output SST being built.
  EpochIntTblPropCollector(RocksDBEpochStore* store, uint64_t file_number)
      : store_(store), file_number_(file_number) {}

  // No per-KV work needed — epoch state is tracked separately by the
  // scheduler and saved via EpochStore::Save().
  Status InternalAdd(const Slice& /*key*/, const Slice& /*value*/,
                     uint64_t /*file_size*/) override {
    return Status::OK();
  }

  void BlockAdd(uint64_t /*block_uncomp_bytes*/,
                uint64_t /*block_compressed_bytes_fast*/,
                uint64_t /*block_compressed_bytes_slow*/) override {}

  // Called once when the SST is fully built.  Reads the tracker from the
  // epoch store and serialises it into the SST's user properties.
  Status Finish(UserCollectedProperties* properties) override;

  UserCollectedProperties GetReadableProperties() const override { return {}; }

  const char* Name() const override { return "EpochIntTblPropCollector"; }

 private:
  RocksDBEpochStore* store_;
  uint64_t           file_number_;
};

// ── EpochIntTblPropCollectorFactory ──────────────────────────────────────────
//
// Created once per output file (with the specific file_number).  Its only job
// is to hand out one EpochIntTblPropCollector; it is safe to destroy after
// NewTableBuilder() returns.
class EpochIntTblPropCollectorFactory final : public IntTblPropCollectorFactory {
 public:
  EpochIntTblPropCollectorFactory(RocksDBEpochStore* store, uint64_t file_number)
      : store_(store), file_number_(file_number) {}

  IntTblPropCollector* CreateIntTblPropCollector(
      uint32_t /*column_family_id*/, int /*level_at_creation*/) override {
    return new EpochIntTblPropCollector(store_, file_number_);
  }

  const char* Name() const override { return "EpochIntTblPropCollectorFactory"; }

 private:
  RocksDBEpochStore* store_;
  uint64_t           file_number_;
};

// ── DelegatingIntTblPropCollectorFactory ─────────────────────────────────────
//
// Non-owning wrapper that lets an existing IntTblPropCollectorFactory* be
// placed in a freshly-allocated IntTblPropCollectorFactories vector without
// transferring ownership.  The delegate must outlive this wrapper — it is
// typically one of the CF's int_tbl_prop_collector_factories_ entries.
class DelegatingIntTblPropCollectorFactory final : public IntTblPropCollectorFactory {
 public:
  explicit DelegatingIntTblPropCollectorFactory(IntTblPropCollectorFactory* delegate)
      : delegate_(delegate) {}

  IntTblPropCollector* CreateIntTblPropCollector(
      uint32_t column_family_id, int level_at_creation) override {
    return delegate_->CreateIntTblPropCollector(column_family_id, level_at_creation);
  }

  const char* Name() const override { return delegate_->Name(); }

 private:
  IntTblPropCollectorFactory* delegate_;  // borrowed, not owned
};

}  // namespace ROCKSDB_NAMESPACE
