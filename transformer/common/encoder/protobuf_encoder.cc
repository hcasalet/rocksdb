#include "protobuf_encoder.h"

#include <cstdint>
#include <memory>

#include <arrow/scalar.h>
#include <arrow/type.h>
#include <arrow/util/checked_cast.h>
#include <arrow/result.h>
#include <arrow/status.h>

namespace ROCKSDB_NAMESPACE {

void ProtobufBytesRowEncoder::AppendVarint(ByteBuffer* out, uint64_t v) {
  while (v >= 0x80) {
    out->push_back(static_cast<uint8_t>((v & 0x7F) | 0x80));
    v >>= 7;
  }
  out->push_back(static_cast<uint8_t>(v));
}

namespace {

// Extract a view of bytes from a scalar.
// Supported: Binary/LargeBinary; optionally String/LargeString (as UTF-8 bytes).
// Returns false on null/unsupported.
bool ScalarToBytesView(const arrow::Scalar& s,
                       const char** data,
                       size_t* size) {
  if (!s.is_valid) return false;

  switch (s.type->id()) {
    case arrow::Type::BINARY: {
      const auto& b = arrow::internal::checked_cast<const arrow::BinaryScalar&>(s);
      const auto view = b.value;
      *data = reinterpret_cast<const char*>(view->data());
      *size = static_cast<size_t>(view->size());
      return true;
    }
    case arrow::Type::LARGE_BINARY: {
      const auto& b = arrow::internal::checked_cast<const arrow::LargeBinaryScalar&>(s);
      const auto view = b.value;
      *data = reinterpret_cast<const char*>(view->data());
      *size = static_cast<size_t>(view->size());
      return true;
    }
    case arrow::Type::STRING: {
      const auto& str = arrow::internal::checked_cast<const arrow::StringScalar&>(s);
      const auto view = str.value;
      *data = reinterpret_cast<const char*>(view->data());
      *size = static_cast<size_t>(view->size());
      return true;
    }
    case arrow::Type::LARGE_STRING: {
      const auto& str = arrow::internal::checked_cast<const arrow::LargeStringScalar&>(s);
      const auto view = str.value;
      *data = reinterpret_cast<const char*>(view->data());
      *size = static_cast<size_t>(view->size());
      return true;
    }
    default:
      return false;
  }
}

}  // namespace

ByteBuffer ProtobufBytesRowEncoder::SerializeFromArrow(const ArrowRecord& rec) const {
  // ArrowRecord is std::shared_ptr<arrow::StructScalar>
  if (!rec) return {};
  if (!rec->is_valid) return {};

  const auto& dtype = rec->type;
  if (!dtype || dtype->id() != arrow::Type::STRUCT) return {};

  const auto& st = arrow::internal::checked_cast<const arrow::StructType&>(*dtype);
  const int32_t n = st.num_fields();
  if (n != static_cast<int32_t>(num_cols_)) return {};

  // Rough reserve: tag + varint(len) + payload for each column.
  // We'll compute from scalar byte sizes.
  size_t total = 0;
  for (size_t i = 0; i < num_cols_; ++i) {
    auto maybe_child = rec->field(static_cast<int>(i));
    if (!maybe_child.ok()) {
      return {};  
    }
    std::shared_ptr<arrow::Scalar> child = *maybe_child;
    if (!child) return {};

    const char* data = nullptr;
    size_t len = 0;
    if (!ScalarToBytesView(*child, &data, &len)) {
      return {};
    }

    // Worst-case varint length for 64-bit is 10 bytes; we reserve ~ (1 + 10 + len).
    total += 1 + 10 + len;
  }

  ByteBuffer out;
  out.reserve(total);

  // field 1, wire type 2 => tag = (1 << 3) | 2 = 0x0A
  constexpr uint8_t kTag = 0x0A;

  for (size_t i = 0; i < num_cols_; ++i) {
    auto maybe_child = rec->field(static_cast<int>(i));
    if (!maybe_child.ok()) {
      return {};  // or throw/log depending on your policy
    }
    std::shared_ptr<arrow::Scalar> child = *maybe_child;
    if (!child) return {};

    const char* data = nullptr;
    size_t len = 0;
    if (!ScalarToBytesView(*child, &data, &len)) {
      return {};
    }

    out.push_back(kTag);
    AppendVarint(&out, static_cast<uint64_t>(len));

    // Append bytes
    const auto* u8 = reinterpret_cast<const uint8_t*>(data);
    out.insert(out.end(), u8, u8 + len);
  }

  return out;
}

}  // namespace ROCKSDB_NAMESPACE
