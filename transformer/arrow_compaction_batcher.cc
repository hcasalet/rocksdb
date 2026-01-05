#include "rocksdb/arrow_compaction_batcher.h"

#include <utility>

#include <arrow/api.h>

namespace ROCKSDB_NAMESPACE {

ArrowCompactionBatcher::ArrowCompactionBatcher()
    : ArrowCompactionBatcher(BatcherOptions{}) {}

ArrowCompactionBatcher::ArrowCompactionBatcher(BatcherOptions batopts) : batopts_(batopts) {
  schema_ = arrow::schema({
      arrow::field("internal_key", arrow::binary()),
      arrow::field("user_key", arrow::binary()),
      arrow::field("value", arrow::binary()),
  });
  // Build initial builders
  (void)ResetBuilders();  // ignore status here; builders will be checked on use
}

arrow::Status ArrowCompactionBatcher::ResetBuilders() {
  num_rows_ = 0;
  num_bytes_ = 0;

  internal_key_b_ = std::make_unique<arrow::BinaryBuilder>();
  user_key_b_     = std::make_unique<arrow::BinaryBuilder>();
  value_b_        = std::make_unique<arrow::BinaryBuilder>();

  return arrow::Status::OK();
}

arrow::Status ArrowCompactionBatcher::Add(const Slice& internal_key,
                                         const Slice& user_key,
                                         const Slice& value) {
  // Arrow BinaryBuilder::Append expects (const uint8_t*, int32_t)
  // Guard size conversion; RocksDB values can exceed 2GB in theory, but in
  // practice are far smaller. We fail loudly if someone hits this.
  auto to_i32 = [](size_t n) -> arrow::Result<int32_t> {
    if (n > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
      return arrow::Status::Invalid("Value too large for Arrow binary append");
    }
    return static_cast<int32_t>(n);
  };

  ARROW_ASSIGN_OR_RAISE(int32_t ik_sz, to_i32(internal_key.size()));
  ARROW_ASSIGN_OR_RAISE(int32_t uk_sz, to_i32(user_key.size()));
  ARROW_ASSIGN_OR_RAISE(int32_t v_sz,  to_i32(value.size()));

  ARROW_RETURN_NOT_OK(internal_key_b_->Append(
      reinterpret_cast<const uint8_t*>(internal_key.data()), ik_sz));
  ARROW_RETURN_NOT_OK(user_key_b_->Append(
      reinterpret_cast<const uint8_t*>(user_key.data()), uk_sz));
  ARROW_RETURN_NOT_OK(value_b_->Append(
      reinterpret_cast<const uint8_t*>(value.data()), v_sz));

  num_rows_ += 1;
  num_bytes_ += internal_key.size() + user_key.size() + value.size();
  return arrow::Status::OK();
}

bool ArrowCompactionBatcher::ShouldFlush() const {
  if (num_rows_ == 0) return false;
  return (num_rows_ >= batopts_.max_rows) || (num_bytes_ >= batopts_.max_bytes);
}

arrow::Status ArrowCompactionBatcher::Flush(std::shared_ptr<arrow::RecordBatch>* out) {
  if (!out) return arrow::Status::Invalid("out is null");
  if (num_rows_ == 0) {
    *out = nullptr;
    return arrow::Status::OK();
  }

  std::shared_ptr<arrow::Array> ik_arr;
  std::shared_ptr<arrow::Array> uk_arr;
  std::shared_ptr<arrow::Array> v_arr;

  ARROW_RETURN_NOT_OK(internal_key_b_->Finish(&ik_arr));
  ARROW_RETURN_NOT_OK(user_key_b_->Finish(&uk_arr));
  ARROW_RETURN_NOT_OK(value_b_->Finish(&v_arr));

  *out = arrow::RecordBatch::Make(schema_, static_cast<int64_t>(num_rows_),
                                  {ik_arr, uk_arr, v_arr});

  return ResetBuilders();
}

}  // namespace rocksdb