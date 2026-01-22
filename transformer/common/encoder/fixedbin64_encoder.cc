#include "fixedbin64_encoder.h"

#include <arrow/result.h>
#include <arrow/status.h>

#include <arrow/util/checked_cast.h>

namespace ROCKSDB_NAMESPACE {

InputOutputDataType FixedBin64Encoder::OutputType() const {
  return InputOutputDataType::FIXEDBIN64;
}

bool ScalarToU64(const arrow::Scalar& s, std::uint64_t* out) {
  if (!s.is_valid) return false;

  switch (s.type->id()) {
    case arrow::Type::UINT8: {
      *out = static_cast<std::uint64_t>(
          arrow::internal::checked_cast<const arrow::UInt8Scalar&>(s).value);
      return true;
    }
    case arrow::Type::UINT16: {
      *out = static_cast<std::uint64_t>(
          arrow::internal::checked_cast<const arrow::UInt16Scalar&>(s).value);
      return true;
    }
    case arrow::Type::UINT32: {
      *out = static_cast<std::uint64_t>(
          arrow::internal::checked_cast<const arrow::UInt32Scalar&>(s).value);
      return true;
    }
    case arrow::Type::UINT64: {
      *out = arrow::internal::checked_cast<const arrow::UInt64Scalar&>(s).value;
      return true;
    }
    case arrow::Type::INT8: {
      const auto v = arrow::internal::checked_cast<const arrow::Int8Scalar&>(s).value;
      if (v < 0) return false;
      *out = static_cast<std::uint64_t>(v);
      return true;
    }
    case arrow::Type::INT16: {
      const auto v = arrow::internal::checked_cast<const arrow::Int16Scalar&>(s).value;
      if (v < 0) return false;
      *out = static_cast<std::uint64_t>(v);
      return true;
    }
    case arrow::Type::INT32: {
      const auto v = arrow::internal::checked_cast<const arrow::Int32Scalar&>(s).value;
      if (v < 0) return false;
      *out = static_cast<std::uint64_t>(v);
      return true;
    }
    case arrow::Type::INT64: {
      const auto v = arrow::internal::checked_cast<const arrow::Int64Scalar&>(s).value;
      if (v < 0) return false;
      *out = static_cast<std::uint64_t>(v);
      return true;
    }
    default:
      return false;
  }
}

void FixedBin64Encoder::AppendFixed64LE(ByteBuffer* out, std::uint64_t v) {
  out->push_back(static_cast<std::uint8_t>( v        & 0xFF));
  out->push_back(static_cast<std::uint8_t>((v >>  8) & 0xFF));
  out->push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
  out->push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
  out->push_back(static_cast<std::uint8_t>((v >> 32) & 0xFF));
  out->push_back(static_cast<std::uint8_t>((v >> 40) & 0xFF));
  out->push_back(static_cast<std::uint8_t>((v >> 48) & 0xFF));
  out->push_back(static_cast<std::uint8_t>((v >> 56) & 0xFF));
}

ByteBuffer FixedBin64Encoder::SerializeFromArrow(const ArrowRecord& rec) const {
  // ArrowRecord is std::shared_ptr<arrow::StructScalar>
  if (!rec) return {};

  // If the whole struct scalar is null, we cannot serialize deterministically.
  // Policy: fail-fast.
  if (!rec->is_valid) return {};

  const auto& dtype = rec->type;
  if (!dtype || dtype->id() != arrow::Type::STRUCT) return {};

  const auto& st = arrow::internal::checked_cast<const arrow::StructType&>(*dtype);
  const int32_t n = st.num_fields();

  ByteBuffer out;
  out.reserve(static_cast<size_t>(n) * 8);

  for (int32_t i = 0; i < n; ++i) {
    auto maybe_child = rec->field(static_cast<int>(i));
    if (!maybe_child.ok()) {
      return {};  // or throw/log depending on your policy
    }
    std::shared_ptr<arrow::Scalar> child = *maybe_child;
    if (!child) return {};

    std::uint64_t v = 0;
    if (!ScalarToU64(*child, &v)) {
      // Non-integer field, null field, or negative signed integer => reject.
      return {};
    }
    AppendFixed64LE(&out, v);
  }

  return out;
}

}