// db/mycelium_adapter/rocksdb_compaction_writer.cc

#include "db/mycelium_adapter/rocksdb_compaction_writer.h"

namespace ROCKSDB_NAMESPACE {

RocksDBCompactionWriter::RocksDBCompactionWriter(CompactionOutputs* outputs,
                                                 size_t             dest_count)
    : outputs_(outputs), dest_count_(dest_count) {}

mycelium::Status RocksDBCompactionWriter::EmitKV(size_t          dest_index,
                                                  std::string_view key,
                                                  std::string_view value) {
  Status s = outputs_->AddKV(dest_index, key, value);
  if (!s.ok()) {
    return mycelium::Status::Error(s.ToString());
  }
  return mycelium::Status::OK();
}

}  // namespace ROCKSDB_NAMESPACE
