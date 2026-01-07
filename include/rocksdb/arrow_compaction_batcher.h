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

    struct FormatOptions {
      // Detection policy:
      // - If set, skip detection and always parse as forced_format.
      std::optional<Format> forced_format;

      // CSV options (if needed)
      char csv_delim = ',';
      // Optional: schema for CSV / JSON (recommended for stability)
      // e.g., vector of {name, datatype}
    };

    explicit ValueParser(FormatOptions opts) : fmt_opts_(std::move(opts)) {}
    arrow::Result<ParsedRow> Parse(const Slice& value,
            const SchemaDescriptor& descriptor) const;

  private:
    FormatOptions fmt_opts_;

    // Detection
    arrow::Result<Format> DetectFormat(const Slice& value) const;

    // Parsers (as private helpers)
    arrow::Result<ParsedRow> ParseJson(const Slice& value) const;
    arrow::Result<ParsedRow> ParseCsv(const Slice& value) const;
    arrow::Result<ParsedRow> ParseProtobuf(const Slice& value,
            std::unique_ptr<google::protobuf::Message>& msg) const;
};

// A small utility that batches compaction K/V pairs into an Arrow RecordBatch.
// This is intentionally "lossless": it stores keys/values as opaque bytes.
// It is NOT wired into compaction yet (Step 2 only).
class ArrowCompactionBatcher {
 public:
  struct BatcherOptions {
    std::size_t max_rows  = 4096;
    std::size_t max_bytes = 16ULL << 20;  // 16 MiB (keys+values) threshold
  };

  ArrowCompactionBatcher();  // default options
  explicit ArrowCompactionBatcher(BatcherOptions batopts, ValueParser::FormatOptions fmtopts);

  // Add a row. Copies bytes into Arrow builders.
  arrow::Status Add(const Slice& internal_key, 
                    const Slice& value,
                    const SchemaDescriptor& schema);

  // Helper function to append 
  arrow::Status AppendScalarToBuilder(arrow::ArrayBuilder* b, const arrow::Scalar& s);

  // Whether we should flush based on thresholds.
  bool ShouldFlush() const;

  // Produce a RecordBatch and reset the builders.
  // If there are no rows, returns an OK status with *out = nullptr.
  arrow::Status Flush(std::shared_ptr<arrow::RecordBatch>* out);

  std::size_t num_rows() const { return num_rows_; }
  std::size_t num_bytes() const { return num_bytes_; }

 private:
  BatcherOptions batopts_;
  std::size_t num_rows_ = 0;
  std::size_t num_bytes_ = 0;

  std::shared_ptr<arrow::Schema> schema_;

  std::unique_ptr<arrow::BinaryBuilder> internal_key_b_;
  std::vector<std::unique_ptr<arrow::ArrayBuilder>> builders_;
  std::vector<std::shared_ptr<arrow::Field>> fields_;
  ValueParser parser_;
  std::unordered_map<std::string, int> col_index_;
  
  arrow::Status ResetBuilders();
};

}  // namespace rocksdb
