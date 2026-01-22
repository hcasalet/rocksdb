#include "flatbuffers_parser.h"

#include <cstddef>
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

FlatbuffersParser::FlatbuffersParser(std::string root_type)
    : root_type_(std::move(root_type)) {}

InputOutputDataType FlatbuffersParser::InputType() const {
  return InputOutputDataType::FLATBUFFERS;
}

bool FlatbuffersParser::Validate(const ByteBuffer& input_data) const {
  // Minimal check: non-empty. Replace with flatbuffers::Verifier if you can.
  return !input_data.empty();
}

arrow::Result<ArrowRecord> FlatbuffersParser::ParseToArrow(const ByteBuffer& data) const {
  if (!Validate(data)) {
    return arrow::Status::Invalid("FlatbuffersParser::ParseToArrow: empty input");
  }

  // Build struct type: struct<root_type: string, bytes: binary>
  auto struct_type = arrow::struct_({
      arrow::field("root_type", arrow::utf8(), /*nullable=*/false),
      arrow::field("bytes", arrow::binary(),  /*nullable=*/false),
  });

  // root_type scalar
  auto root_scalar = std::make_shared<arrow::StringScalar>(root_type_);

  // bytes scalar (copy into an Arrow Buffer)
  ARROW_ASSIGN_OR_RAISE(
      std::unique_ptr<arrow::Buffer> tmp,
      arrow::AllocateBuffer(static_cast<int64_t>(data.size())));

  if (!data.empty()) {
    std::memcpy(tmp->mutable_data(),
                static_cast<const void*>(data.data()),
                data.size());
  }

  auto buf = std::shared_ptr<arrow::Buffer>(std::move(tmp));
  auto bytes_scalar = std::make_shared<arrow::BinaryScalar>(std::move(buf));

  arrow::ScalarVector values;
  values.reserve(2);
  values.push_back(std::move(root_scalar));
  values.push_back(std::move(bytes_scalar));

  return std::make_shared<arrow::StructScalar>(std::move(values), std::move(struct_type));
}

}  // namespace ROCKSDB_NAMESPACE
