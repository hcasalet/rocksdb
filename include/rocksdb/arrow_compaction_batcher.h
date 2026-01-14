#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <charconv>
#include <system_error>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/reflection.h>

#include "rocksdb/slice.h"
#include "rocksdb/transformer.h"

#ifdef LZ4
  #pragma push_macro("LZ4")
  #undef LZ4
  #define ROCKSDB_RESTORE_LZ4_MACRO
#endif

#ifdef ZSTD
  #pragma push_macro("ZSTD")
  #undef ZSTD
  #define ROCKSDB_RESTORE_ZSTD_MACRO
#endif

#include <arrow/api.h>

#ifdef ROCKSDB_RESTORE_ZSTD_MACRO
  #pragma pop_macro("ZSTD")
  #undef ROCKSDB_RESTORE_ZSTD_MACRO
#endif

#ifdef ROCKSDB_RESTORE_LZ4_MACRO
  #pragma pop_macro("LZ4")
  #undef ROCKSDB_RESTORE_LZ4_MACRO
#endif

namespace ROCKSDB_NAMESPACE {

struct ParsedFields {
    std::string name;
    std::shared_ptr<arrow::Scalar> scalar;
    std::shared_ptr<arrow::DataType> declared_type;
};

struct ParsedRow {
    std::vector<ParsedFields> fields;
};

class ValueParser {
  public:
    enum class Format { kJson, kCsv, kProtobuf };

    ValueParser(const SchemaDescriptor& schema) : schema_(schema) {}
    arrow::Result<ParsedRow> Parse(const Slice& value) const;

  private:
    const SchemaDescriptor& schema_;

    // Parsers (as private helpers)
    arrow::Result<ParsedRow> ParseJson(const Slice& value) const;
    arrow::Result<ParsedRow> ParseCsv(const Slice& value) const;
    arrow::Result<ParsedRow> ParseProtobuf(const Slice& value,
            std::unique_ptr<google::protobuf::Message>& msg) const;
    arrow::Result<ParsedRow> ParseBin64(const rocksdb::Slice& value) const;
};

// A small utility that batches compaction K/V pairs into an Arrow RecordBatch.
// This is intentionally "lossless": it stores keys/values as opaque bytes.
// It is NOT wired into compaction yet (Step 2 only).
class ArrowCompactionBatcher {
 public:
  static arrow::Result<std::unique_ptr<ArrowCompactionBatcher>> Create(
      const SchemaDescriptor& schema);

  // Add a row. Copies bytes into Arrow builders.
  arrow::Status Add(const Slice& internal_key, 
                    const Slice& value);

  // Helper function to append 
  arrow::Status AppendScalarToBuilder(arrow::ArrayBuilder* b, const arrow::Scalar& s);

  // Produce a RecordBatch and reset the builders.
  // If there are no rows, returns an OK status with *out = nullptr.
  arrow::Status Flush(std::shared_ptr<arrow::RecordBatch>* out);

  std::size_t num_rows() const { return num_rows_; }
  std::size_t num_bytes() const { return num_bytes_; }

 private:
  std::size_t num_rows_ = 0;
  std::size_t num_bytes_ = 0;

  ValueParser parser_;
  std::vector<std::shared_ptr<arrow::Field>> fields_;
  std::vector<std::unique_ptr<arrow::ArrayBuilder>> builders_;
  std::shared_ptr<arrow::Schema> schema_;
  
  explicit ArrowCompactionBatcher(const SchemaDescriptor& schema);
  arrow::Status BuildFromSchema(const SchemaDescriptor& schema);
  arrow::Status Clear();
  arrow::Status Reset();
};

}  // namespace rocksdb
