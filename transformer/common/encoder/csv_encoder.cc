#include "csv_encoder.h"

#include <arrow/util/checked_cast.h>

#include <string>
#include <memory>
#include <utility>

#include <arrow/scalar.h>
#include <arrow/type.h>
#include <arrow/util/checked_cast.h>
#include <arrow/result.h>
#include <arrow/status.h>

namespace ROCKSDB_NAMESPACE {

InputOutputDataType CsvEncoder::OutputType() const {
  return InputOutputDataType::CSV;
}

std::string CsvEncoder::ScalarToStringForCsv(const arrow::Scalar& s) const {
  if (!s.is_valid) {
    return "";
  }

  switch (s.type->id()) {
    case arrow::Type::BOOL:
      return arrow::internal::checked_cast<const arrow::BooleanScalar&>(s).value ? "true" : "false";

    case arrow::Type::INT8:
      return std::to_string(arrow::internal::checked_cast<const arrow::Int8Scalar&>(s).value);
    case arrow::Type::INT16:
      return std::to_string(arrow::internal::checked_cast<const arrow::Int16Scalar&>(s).value);
    case arrow::Type::INT32:
      return std::to_string(arrow::internal::checked_cast<const arrow::Int32Scalar&>(s).value);
    case arrow::Type::INT64:
      return std::to_string(arrow::internal::checked_cast<const arrow::Int64Scalar&>(s).value);

    case arrow::Type::UINT8:
      return std::to_string(arrow::internal::checked_cast<const arrow::UInt8Scalar&>(s).value);
    case arrow::Type::UINT16:
      return std::to_string(arrow::internal::checked_cast<const arrow::UInt16Scalar&>(s).value);
    case arrow::Type::UINT32:
      return std::to_string(arrow::internal::checked_cast<const arrow::UInt32Scalar&>(s).value);
    case arrow::Type::UINT64:
      return std::to_string(arrow::internal::checked_cast<const arrow::UInt64Scalar&>(s).value);

    case arrow::Type::FLOAT:
      return std::to_string(arrow::internal::checked_cast<const arrow::FloatScalar&>(s).value);
    case arrow::Type::DOUBLE:
      return std::to_string(arrow::internal::checked_cast<const arrow::DoubleScalar&>(s).value);

    case arrow::Type::STRING:
      return arrow::internal::checked_cast<const arrow::StringScalar&>(s).ToString();
    case arrow::Type::LARGE_STRING:
      return arrow::internal::checked_cast<const arrow::LargeStringScalar&>(s).ToString();

    case arrow::Type::BINARY:
      // ToString() prints a readable representation; if you need base64/hex,
      // change this policy explicitly.
      return arrow::internal::checked_cast<const arrow::BinaryScalar&>(s).ToString();
    case arrow::Type::LARGE_BINARY:
      return arrow::internal::checked_cast<const arrow::LargeBinaryScalar&>(s).ToString();

    default:
      // Fallback: Arrow's Scalar::ToString() for types like decimal/date/timestamp, etc.
      // If you want to reject unsupported types instead, return "" and handle it in caller.
      return s.ToString();
  }
}

void CsvEncoder::AppendField(std::string* out, const std::string& f) {
  bool needs_quotes = false;
  for (char c : f) {
    if (c == '"' || c == ',' || c == '\n' || c == '\r') {
      needs_quotes = true;
      break;
    }
  }

  if (!needs_quotes) {
    out->append(f);
    return;
  }

  out->push_back('"');
  for (char c : f) {
    if (c == '"') {
      out->push_back('"');
      out->push_back('"');
    } else {
      out->push_back(c);
    }
  }
  out->push_back('"');
}

ByteBuffer CsvEncoder::SerializeFromArrow(const ArrowRecord& rec) const {
  // ArrowRecord is std::shared_ptr<arrow::StructScalar>
  if (!rec) return {};

  const auto& dtype = rec->type;
  if (!dtype || dtype->id() != arrow::Type::STRUCT) {
    return {};
  }

  const auto& st = arrow::internal::checked_cast<const arrow::StructType&>(*dtype);
  const int32_t n = st.num_fields();

  std::string line;
  // Rough reserve: assume ~8 chars per field plus commas/newline (tune if you want)
  line.reserve(static_cast<size_t>(n) * 10 + 1);

  // If the entire struct is null, policy: emit empty fields for the right arity.
  // This preserves the CSV column count.
  for (int32_t i = 0; i < n; ++i) {
    if (i) line.push_back(',');

    std::string field_str;
    if (rec->is_valid) {
      auto maybe_child = rec->field(i);
      if (!maybe_child.ok()) {
        return {}; 
      }
      std::shared_ptr<arrow::Scalar> child = *maybe_child;
      if (!child) return {};
      field_str = ScalarToStringForCsv(*child);
    } else {
      field_str = "";
    }

    AppendField(&line, field_str);
  }

  line.push_back('\n');
  return ByteBuffer(line.begin(), line.end());
}

}