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

std::unique_ptr<ParsedObject> FixedBin64Parser::Parse(const ByteBuffer& data) const {
  if (!Validate(data)) return nullptr;

  auto row = std::make_unique<FixedBin64RowPayload>();
  row->values.reserve(num_cols_);

  const std::uint8_t* p = data.data();
  for (int i = 0; i < num_cols_; ++i) {
    row->values.push_back(ReadFixed64LE(p + (static_cast<size_t>(i) * 8)));
  }

  auto out = std::make_unique<ParsedObject>();
  out->payload = ParsedPayload::Make<FixedBin64RowPayload>(
      InputOutputDataType::FIXEDBIN64, std::move(row));
  return out;
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