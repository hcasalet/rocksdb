#include "fixedbin64_parser.h"

#include <stdexcept>


namespace ROCKSDB_NAMESPACE {

FixedBin64Parser::FixedBin64Parser(int num_cols)
    : num_cols_(num_cols) {
  schema_.reserve(num_cols_);
  for (int i = 0; i < num_cols_; ++i) {
    FieldSchema fs;
    fs.name = "col" + std::to_string(i);
    fs.type = "uint64";          // or "fixed64"
    fs.field_number = i;
    schema_.push_back(std::move(fs));
  }
}

InputOutputDataType FixedBin64Parser::InputType() const {
  return InputOutputDataType::FIXEDBIN64;
}

const std::vector<FieldSchema>& FixedBin64Parser::GetInputFieldSchema() const {
  return schema_;
}

bool FixedBin64Parser::Validate(const ByteBuffer& input_data) const {
  if (num_cols_ <= 0) return false;
  const size_t need = static_cast<size_t>(num_cols_) * 8;
  return input_data.size() == need;
}

arrow::Result<ArrowRecord> FixedBin64Parser::ParseToArrow(const ByteBuffer& data) const {
  if (!Validate(data)) {
    return arrow::Status::Invalid(
        "FixedBin64Parser::ParseToArrow: invalid FIXEDBIN64 payload size; got=",
        static_cast<int64_t>(data.size()),
        " expected=",
        static_cast<int64_t>(static_cast<size_t>(num_cols_) * 8));
  }

  // Build Arrow struct type: struct<col0: uint64, col1: uint64, ...>
  std::vector<std::shared_ptr<arrow::Field>> fields;
  fields.reserve(static_cast<size_t>(num_cols_));
  for (int i = 0; i < num_cols_; ++i) {
    fields.push_back(arrow::field(schema_[i].name, arrow::uint64(), /*nullable=*/false));
  }
  auto struct_type = arrow::struct_(std::move(fields));

  // Build field scalars (one "row"): [UInt64Scalar(v0), UInt64Scalar(v1), ...]
  arrow::ScalarVector values;
  values.reserve(static_cast<size_t>(num_cols_));

  const std::uint8_t* p = data.data();
  for (int i = 0; i < num_cols_; ++i) {
    const std::uint64_t v = ReadFixed64LE(p + (static_cast<size_t>(i) * 8));
    values.push_back(std::make_shared<arrow::UInt64Scalar>(v));
  }

  // Construct StructScalar from values + struct type.
  // Note: StructScalar's ctor takes (ScalarVector values, DataType type).
  auto rec = std::make_shared<arrow::StructScalar>(std::move(values), std::move(struct_type));
  return rec;
}

std::uint64_t FixedBin64Parser::ReadFixed64LE(const std::uint8_t* p) {
  return (static_cast<std::uint64_t>(p[0])      ) |
         (static_cast<std::uint64_t>(p[1]) <<  8) |
         (static_cast<std::uint64_t>(p[2]) << 16) |
         (static_cast<std::uint64_t>(p[3]) << 24) |
         (static_cast<std::uint64_t>(p[4]) << 32) |
         (static_cast<std::uint64_t>(p[5]) << 40) |
         (static_cast<std::uint64_t>(p[6]) << 48) |
         (static_cast<std::uint64_t>(p[7]) << 56);
}

}