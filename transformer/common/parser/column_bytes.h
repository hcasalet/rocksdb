#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "rocksdb/rocksdb_namespace.h"
#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

// Add this to InputOutputDataType in transformer.h:
//   COLUMNBYTES      = 1 << 7,
//
// The IR: schema-ordered columns, each as raw bytes (no typing).
struct ColumnBytesRow {
  std::vector<ByteBuffer> cols;
};

// Helper: fetch ColumnBytesRow from a ParsedObject.
inline const ColumnBytesRow* AsColumnBytesRow(const ParsedObject& obj) {
  return obj.payload.As<ColumnBytesRow>();
}

inline ColumnBytesRow* AsMutableColumnBytesRow(ParsedObject& obj) {
  return obj.payload.As<ColumnBytesRow>();
}

}  // namespace ROCKSDB_NAMESPACE