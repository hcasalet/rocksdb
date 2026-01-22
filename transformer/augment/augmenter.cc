#include <arrow/buffer.h>
#include <arrow/result.h>
#include <arrow/status.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "augmenter.h"

namespace ROCKSDB_NAMESPACE {

// Assumes inside a class that has: std::vector<std::vector<int>> index_positions_;

namespace {

// Convert a scalar into a byte string used for composing secondary index keys.
// Policy: reject nulls; render numerics as base-10 ASCII; pass through string/binary as bytes.
arrow::Result<std::string> ScalarToKeyBytes(const std::shared_ptr<arrow::Scalar>& s) {
  if (!s) {
    return arrow::Status::Invalid("ScalarToKeyBytes: null scalar pointer");
  }
  if (!s->is_valid) {
    return arrow::Status::Invalid("ScalarToKeyBytes: null (invalid) scalar value");
  }

  switch (s->type->id()) {
    case arrow::Type::BINARY: {
      const auto& bs = static_cast<const arrow::BinaryScalar&>(*s);
      if (!bs.value) return std::string{};
      return std::string(reinterpret_cast<const char*>(bs.value->data()),
                         static_cast<size_t>(bs.value->size()));
    }
    case arrow::Type::LARGE_BINARY: {
      const auto& bs = static_cast<const arrow::LargeBinaryScalar&>(*s);
      if (!bs.value) return std::string{};
      return std::string(reinterpret_cast<const char*>(bs.value->data()),
                         static_cast<size_t>(bs.value->size()));
    }
    case arrow::Type::STRING: {
      const auto& ss = static_cast<const arrow::StringScalar&>(*s);
      // StringScalar stores a Buffer containing UTF-8 bytes.
      if (!ss.value) return std::string{};
      return std::string(reinterpret_cast<const char*>(ss.value->data()),
                         static_cast<size_t>(ss.value->size()));
    }
    case arrow::Type::LARGE_STRING: {
      const auto& ss = static_cast<const arrow::LargeStringScalar&>(*s);
      if (!ss.value) return std::string{};
      return std::string(reinterpret_cast<const char*>(ss.value->data()),
                         static_cast<size_t>(ss.value->size()));
    }
    case arrow::Type::UINT64: {
      const auto& ns = static_cast<const arrow::UInt64Scalar&>(*s);
      return std::to_string(ns.value);
    }
    case arrow::Type::INT64: {
      const auto& ns = static_cast<const arrow::Int64Scalar&>(*s);
      return std::to_string(ns.value);
    }
    case arrow::Type::UINT32: {
      const auto& ns = static_cast<const arrow::UInt32Scalar&>(*s);
      return std::to_string(ns.value);
    }
    case arrow::Type::INT32: {
      const auto& ns = static_cast<const arrow::Int32Scalar&>(*s);
      return std::to_string(ns.value);
    }
    case arrow::Type::UINT16: {
      const auto& ns = static_cast<const arrow::UInt16Scalar&>(*s);
      return std::to_string(ns.value);
    }
    case arrow::Type::INT16: {
      const auto& ns = static_cast<const arrow::Int16Scalar&>(*s);
      return std::to_string(ns.value);
    }
    case arrow::Type::UINT8: {
      const auto& ns = static_cast<const arrow::UInt8Scalar&>(*s);
      return std::to_string(ns.value);
    }
    case arrow::Type::INT8: {
      const auto& ns = static_cast<const arrow::Int8Scalar&>(*s);
      return std::to_string(ns.value);
    }
    case arrow::Type::BOOL: {
      const auto& bs = static_cast<const arrow::BooleanScalar&>(*s);
      return bs.value ? "1" : "0";
    }
    default:
      return arrow::Status::Invalid("ScalarToKeyBytes: unsupported scalar type: ",
                                    s->type->ToString());
  }
}

arrow::Result<std::shared_ptr<arrow::Buffer>> BufferFromBytes(std::string_view sv) {
  // Arrow's Buffer::FromString copies.
  auto buf = arrow::Buffer::FromString(std::string(sv));
  if (!buf) {
    return arrow::Status::OutOfMemory("BufferFromBytes: Buffer::FromString returned null");
  }
  return buf;
}

}  // namespace

std::vector<ArrowRecord> Augmenter::Transform(
    const Slice& key,
    const ArrowRecord& input) const {
  std::vector<ArrowRecord> outputs;

  if (!input) {
    // In your codebase you may prefer to throw; here we return empty.
    return outputs;
  }

  // Access the struct fields (Arrow StructScalar stores a vector of child scalars).
  // This is the common Arrow C++ layout for StructScalar.
  const auto& row_values = input->value;

  const std::string key_field_separator = "%%";
  const std::string original_key_separator = "$$$KEY$$$";

  // Copy primary key bytes once.
  const std::string_view pk_sv(key.data(), key.size());

  // Predefine output "schema" (struct type) for index records.
  // (StructScalar carries a DataType; consumers can inspect it.)
  auto out_type = arrow::struct_({
      arrow::field("index_no", arrow::int32(),  /*nullable=*/false),
      arrow::field("index_key", arrow::binary(), /*nullable=*/false),
      arrow::field("primary_key", arrow::binary(), /*nullable=*/false),
  });

  outputs.reserve(index_positions_.size());

  for (int idx = 0; idx < static_cast<int>(index_positions_.size()); ++idx) {
    const auto& positions = index_positions_[static_cast<size_t>(idx)];

    // Build prefixIndexKey bytes = f0%%f1%%...$$$KEY$$$<primary_key>
    std::string prefix;
    prefix.reserve(128);  // heuristic

    for (size_t j = 0; j < positions.size(); ++j) {
      const int pos = positions[j];
      if (pos < 0 || static_cast<size_t>(pos) >= row_values.size()) {
        // If you prefer "skip this index record", change to `continue;`
        // Here we fail “hard” by skipping output for this idx.
        // (No arrow::Result return type here, so choose policy.)
        return {};  // safest: do not emit partial indexes
      }

      if (j > 0) prefix.append(key_field_separator);

      // Convert field scalar to bytes.
      auto maybe_bytes = ScalarToKeyBytes(row_values[static_cast<size_t>(pos)]);
      if (!maybe_bytes.ok()) {
        return {};  // policy: if any field missing/unsupported/null -> emit nothing
      }
      prefix.append(*maybe_bytes);
    }

    prefix.append(original_key_separator);
    prefix.append(pk_sv.data(), pk_sv.size());

    // Materialize Arrow scalars for the output record.
    auto index_no_scalar = std::make_shared<arrow::Int32Scalar>(static_cast<int32_t>(idx));

    auto maybe_index_buf = BufferFromBytes(std::string_view(prefix.data(), prefix.size()));
    if (!maybe_index_buf.ok()) return {};
    auto index_key_scalar = std::make_shared<arrow::BinaryScalar>(*maybe_index_buf);

    auto maybe_pk_buf = BufferFromBytes(pk_sv);
    if (!maybe_pk_buf.ok()) return {};
    auto pk_scalar = std::make_shared<arrow::BinaryScalar>(*maybe_pk_buf);

    arrow::ScalarVector out_values;
    out_values.reserve(3);
    out_values.push_back(std::move(index_no_scalar));
    out_values.push_back(std::move(index_key_scalar));
    out_values.push_back(std::move(pk_scalar));

    outputs.push_back(std::make_shared<arrow::StructScalar>(std::move(out_values), out_type));
  }

  return outputs;
}

}  // namespace ROCKSDB_NAMESPACE
