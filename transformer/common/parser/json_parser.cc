#include "json_parser.h"

#include <cstring>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Arrow
#include <arrow/api.h>
#include <arrow/buffer.h>
#include <arrow/result.h>
#include <arrow/status.h>

namespace ROCKSDB_NAMESPACE {

JsonColsParser::JsonColsParser(size_t num_cols, size_t expected_value_len)
    : num_cols_(num_cols), expected_value_len_(expected_value_len) {
  input_schema_.reserve(num_cols_);
  for (size_t i = 0; i < num_cols_; ++i) {
    FieldSchema fs;
    fs.name = "col" + std::to_string(i);
    fs.type = "bytes";  // "bytes" logically; still stored as raw bytes
    fs.field_number = static_cast<int>(i + 1);
    input_schema_.push_back(std::move(fs));
  }
}

bool JsonColsParser::Validate(const ByteBuffer& input_data) const {
  if (input_data.empty()) return false;
  const char* s = reinterpret_cast<const char*>(input_data.data());
  const size_t n = input_data.size();
  // Minimal checks for shape; full correctness is deferred to Parse.
  return n >= 2 && s[0] == '{' && s[n - 1] == '}';
}

bool JsonColsParser::ExtractStringValueForKey(const char* json,
                                             size_t n,
                                             const std::string& key,
                                             const char** out_begin,
                                             const char** out_end) {
  // Search for: "key"
  // Then expect : "VALUE"
  // POC assumption: no escapes inside VALUE.
  const std::string needle = "\"" + key + "\"";
  const char* hay = json;
  const char* end = json + n;

  const char* p = nullptr;
  for (const char* cur = hay; cur + needle.size() <= end; ++cur) {
    if (std::memcmp(cur, needle.data(), needle.size()) == 0) {
      p = cur + needle.size();
      break;
    }
  }
  if (!p) return false;

  // Skip whitespace
  while (p < end && (*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r')) ++p;
  if (p >= end || *p != ':') return false;
  ++p;
  while (p < end && (*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r')) ++p;
  if (p >= end || *p != '"') return false;
  ++p;

  const char* val_begin = p;
  while (p < end && *p != '"') ++p;
  if (p >= end) return false;

  const char* val_end = p;  // points at closing quote
  *out_begin = val_begin;
  *out_end = val_end;
  return true;
}

arrow::Result<ArrowRecord> JsonColsParser::ParseToArrow(const ByteBuffer& data) const {
  if (!Validate(data)) {
    return arrow::Status::Invalid("JsonColsParser::ParseToArrow: invalid JSON envelope");
  }

  const char* s = reinterpret_cast<const char*>(data.data());
  const size_t n = data.size();

  // Build struct type: struct<col0: binary, col1: binary, ...>
  std::vector<std::shared_ptr<arrow::Field>> fields;
  fields.reserve(num_cols_);
  for (size_t i = 0; i < num_cols_; ++i) {
    fields.push_back(arrow::field("col" + std::to_string(i), arrow::binary(),
                                  /*nullable=*/false));
  }
  auto struct_type = arrow::struct_(std::move(fields));

  arrow::ScalarVector values;
  values.reserve(num_cols_);

  for (size_t i = 0; i < num_cols_; ++i) {
    const std::string key = "col" + std::to_string(i);
    const char* vb = nullptr;
    const char* ve = nullptr;

    if (!ExtractStringValueForKey(s, n, key, &vb, &ve)) {
      return arrow::Status::Invalid("JsonColsParser::ParseToArrow: missing key: ", key);
    }

    const size_t len = static_cast<size_t>(ve - vb);
    if (expected_value_len_ != 0 && len != expected_value_len_) {
      return arrow::Status::Invalid(
          "JsonColsParser::ParseToArrow: value length mismatch for key: ", key,
          " got=", static_cast<int64_t>(len),
          " expected=", static_cast<int64_t>(expected_value_len_));
    }

    // Copy bytes into an Arrow Buffer (BinaryScalar expects a Buffer).
    ARROW_ASSIGN_OR_RAISE(auto buf, arrow::AllocateBuffer(static_cast<int64_t>(len)));
    std::memcpy(buf->mutable_data(), vb, len);
    values.push_back(std::make_shared<arrow::BinaryScalar>(std::move(buf)));
  }

  return std::make_shared<arrow::StructScalar>(std::move(values), std::move(struct_type));
}

}  // namespace ROCKSDB_NAMESPACE
