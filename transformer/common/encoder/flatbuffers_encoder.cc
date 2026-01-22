#include "flatbuffers_encoder.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <arrow/scalar.h>
#include <arrow/type.h>
#include <arrow/util/checked_cast.h>
#include <arrow/result.h>
#include <arrow/status.h>

namespace ROCKSDB_NAMESPACE {

InputOutputDataType FlatbuffersEncoder::OutputType() const {
  return InputOutputDataType::FLATBUFFERS;
}

namespace {

int FindFieldIndexByName(const arrow::StructType& st, const std::string& name) {
  for (int i = 0; i < st.num_fields(); ++i) {
    if (st.field(i)->name() == name) return i;
  }
  return -1;
}

arrow::Result<rocksdb::ByteBuffer> BytesFromScalar(const arrow::Scalar& s) {
  if (!s.is_valid) return arrow::Status::Invalid("FlatbuffersRowEncoder: invalid input record");

  switch (s.type->id()) {
    case arrow::Type::BINARY: {
      const auto& b = arrow::internal::checked_cast<const arrow::BinaryScalar&>(s);
      const auto view = b.value;  // arrow::util::string_view
      if (!view) return rocksdb::ByteBuffer{}; 
      return ByteBuffer(view->data(), view->data() + view->size());
    }
    case arrow::Type::LARGE_BINARY: {
      const auto& b = arrow::internal::checked_cast<const arrow::LargeBinaryScalar&>(s);
      const auto view = b.value;
      if (!view) return rocksdb::ByteBuffer{}; 
      return ByteBuffer(view->data(), view->data() + view->size());
    }
    case arrow::Type::STRING: {
      const auto& str = arrow::internal::checked_cast<const arrow::StringScalar&>(s);
      const auto view = str.value;
      if (!view) return rocksdb::ByteBuffer{}; 
      return ByteBuffer(view->data(), view->data() + view->size());
    }
    case arrow::Type::LARGE_STRING: {
      const auto& str = arrow::internal::checked_cast<const arrow::LargeStringScalar&>(s);
      const auto view = str.value;
      if (!view) return rocksdb::ByteBuffer{}; 
      return ByteBuffer(view->data(), view->data() + view->size());
    }
    default:
      return arrow::Status::Invalid("FlatbuffersRowEncoder: invalid scalar type");
    }
  }

}  // namespace

ByteBuffer FlatbuffersEncoder::SerializeFromArrow(const ArrowRecord& rec) const {
  // ArrowRecord is std::shared_ptr<arrow::StructScalar>
  if (!rec) return {};
  if (!rec->is_valid) return {};

  const auto& dtype = rec->type;
  if (!dtype || dtype->id() != arrow::Type::STRUCT) return {};

  const auto& st = arrow::internal::checked_cast<const arrow::StructType&>(*dtype);
  if (st.num_fields() <= 0) return {};

  // Prefer a field literally named "bytes" if it exists; otherwise use field 0.
  int idx = FindFieldIndexByName(st, "bytes");
  if (idx < 0) idx = 0;

  auto maybe_child = rec->field(static_cast<int>(idx));
  if (!maybe_child.ok()) {
    return {};  // or throw/log depending on your policy
  }
  std::shared_ptr<arrow::Scalar> child = *maybe_child;
  if (!child) return {};

  auto r = BytesFromScalar(*child);
  return std::move(*r);
}

}  // namespace ROCKSDB_NAMESPACE
