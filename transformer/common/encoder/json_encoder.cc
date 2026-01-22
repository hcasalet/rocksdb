#include "json_encoder.h"

#include <cstdint>
#include <memory>
#include <string>

#include <arrow/scalar.h>
#include <arrow/type.h>
#include <arrow/util/checked_cast.h>
#include <arrow/result.h>
#include <arrow/status.h>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace ROCKSDB_NAMESPACE {

InputOutputDataType JsonEncoder::OutputType() const {
  return InputOutputDataType::JSON;
}

namespace {

template <typename WriterT>
void WriteScalarAsJson(WriterT& w, const arrow::Scalar& s) {
  if (!s.is_valid) {
    w.Null();
    return;
  }

  switch (s.type->id()) {
    case arrow::Type::BOOL:
      w.Bool(arrow::internal::checked_cast<const arrow::BooleanScalar&>(s).value);
      return;

    case arrow::Type::INT8:
      w.Int(arrow::internal::checked_cast<const arrow::Int8Scalar&>(s).value);
      return;
    case arrow::Type::INT16:
      w.Int(arrow::internal::checked_cast<const arrow::Int16Scalar&>(s).value);
      return;
    case arrow::Type::INT32:
      w.Int(arrow::internal::checked_cast<const arrow::Int32Scalar&>(s).value);
      return;
    case arrow::Type::INT64:
      w.Int64(arrow::internal::checked_cast<const arrow::Int64Scalar&>(s).value);
      return;

    case arrow::Type::UINT8:
      w.Uint(arrow::internal::checked_cast<const arrow::UInt8Scalar&>(s).value);
      return;
    case arrow::Type::UINT16:
      w.Uint(arrow::internal::checked_cast<const arrow::UInt16Scalar&>(s).value);
      return;
    case arrow::Type::UINT32:
      w.Uint(arrow::internal::checked_cast<const arrow::UInt32Scalar&>(s).value);
      return;
    case arrow::Type::UINT64:
      w.Uint64(arrow::internal::checked_cast<const arrow::UInt64Scalar&>(s).value);
      return;

    case arrow::Type::FLOAT:
      w.Double(static_cast<double>(arrow::internal::checked_cast<const arrow::FloatScalar&>(s).value));
      return;
    case arrow::Type::DOUBLE:
      w.Double(arrow::internal::checked_cast<const arrow::DoubleScalar&>(s).value);
      return;

    case arrow::Type::STRING: {
      const auto& ss = arrow::internal::checked_cast<const arrow::StringScalar&>(s);
      const auto view = ss.value;  // string_view
      w.String(reinterpret_cast<const char*>(view->data()), static_cast<rapidjson::SizeType>(view->size()));
      return;
    }
    case arrow::Type::LARGE_STRING: {
      const auto& ss = arrow::internal::checked_cast<const arrow::LargeStringScalar&>(s);
      const auto view = ss.value;
      w.String(reinterpret_cast<const char*>(view->data()), static_cast<rapidjson::SizeType>(view->size()));
      return;
    }

    default: {
      // Safe fallback: represent as JSON string.
      const std::string repr = s.ToString();
      w.String(repr.data(), static_cast<rapidjson::SizeType>(repr.size()));
      return;
    }
  }
}

}  // namespace

ByteBuffer JsonEncoder::SerializeFromArrow(const ArrowRecord& rec) const {
  // ArrowRecord is std::shared_ptr<arrow::StructScalar>
  if (!rec) return {};

  const auto& dtype = rec->type;
  if (!dtype || dtype->id() != arrow::Type::STRUCT) {
    return {};
  }

  const auto& st = arrow::internal::checked_cast<const arrow::StructType&>(*dtype);
  const int32_t n = st.num_fields();

  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

  writer.StartObject();

  // If the whole struct is null, we still emit an empty object {} (policy).
  // If you prefer "null" instead, replace this block accordingly.
  if (rec->is_valid) {
    for (int32_t i = 0; i < n; ++i) {
      const auto& field = st.field(i);

      auto child_res = rec->field(i);  // arrow::Result<std::shared_ptr<arrow::Scalar>>
      if (!child_res.ok()) {
        return {};
      }
      std::shared_ptr<arrow::Scalar> child = std::move(*child_res);
      if (!field || !child) return {};

      const std::string& name = field->name();
      writer.Key(name.data(), static_cast<rapidjson::SizeType>(name.size()));
      WriteScalarAsJson(writer, *child);
    }
  }

  writer.EndObject();

  const char* s = sb.GetString();
  const size_t len = sb.GetSize();
  return ByteBuffer(reinterpret_cast<const std::uint8_t*>(s),
                    reinterpret_cast<const std::uint8_t*>(s) + len);
}

}  // namespace ROCKSDB_NAMESPACE
