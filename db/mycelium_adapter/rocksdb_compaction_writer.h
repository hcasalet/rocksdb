#pragma once
// db/mycelium_adapter/rocksdb_compaction_writer.h
//
// RocksDB concrete implementation of mycelium::CompactionWriter.
//
// Each instance wraps a CompactionOutputs object that is already set up with
// the correct number of SST builders (one per destination tree).  When
// libmycelium's transformation core calls EmitKV, this adapter parses the
// encoded internal key and delegates to CompactionOutputs::AddKV which in turn
// calls the internal EmitOne path.

#include <cstddef>
#include <string_view>

#include "db/compaction/compaction_outputs.h"
#include "mycelium/compaction_hook.h"

namespace ROCKSDB_NAMESPACE {

class RocksDBCompactionWriter final : public mycelium::CompactionWriter {
 public:
  // outputs  — non-owning pointer; must outlive this writer.
  // dest_count — number of destination trees (= builders already open in
  //              outputs, i.e. outputs.GetOutputsSize()).
  RocksDBCompactionWriter(CompactionOutputs* outputs, size_t dest_count);

  // Write one KV pair to destination tree [dest_index].
  // key and value are encoded as in the RocksDB internal representation:
  //   key   = InternalKey (user_key + 8-byte tag)
  //   value = raw value bytes
  mycelium::Status EmitKV(size_t          dest_index,
                           std::string_view key,
                           std::string_view value) override;

  // Flush is a no-op at the writer level; CompactionOutputs handles flushing
  // when output files are closed by FinishCompactionOutputFile.
  mycelium::Status Flush() override { return mycelium::Status::OK(); }

  size_t DestCount() const override { return dest_count_; }

 private:
  CompactionOutputs* outputs_;   // non-owning
  size_t             dest_count_;
};

}  // namespace ROCKSDB_NAMESPACE
