#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "rocksdb/slice.h"

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
  explicit ArrowCompactionBatcher(BatcherOptions batopts);

  // Add a row. Copies bytes into Arrow builders.
  arrow::Status Add(const Slice& internal_key, const Slice& value);

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
  std::unique_ptr<arrow::BinaryBuilder> user_key_b_;
  std::unique_ptr<arrow::BinaryBuilder> value_b_;

  arrow::Status ResetBuilders();
};

}  // namespace rocksdb
