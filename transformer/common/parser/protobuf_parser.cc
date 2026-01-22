#include "protobuf_parser.h"

#include <google/protobuf/message.h>

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

ProtobufParser::ProtobufParser(std::unique_ptr<google::protobuf::Message> template_message)
    : template_(std::move(template_message)) {}

InputOutputDataType ProtobufParser::InputType() const {
  return InputOutputDataType::PROTOBUF;
}

bool ProtobufParser::Validate(const rocksdb::ByteBuffer& input_data) const {
  if (!template_) return false;
  auto msg = std::unique_ptr<google::protobuf::Message>(template_->New());
  return msg->ParseFromArray(input_data.data(), static_cast<int>(input_data.size()));
}

arrow::Result<ArrowRecord> ProtobufParser::ParseToArrow(const rocksdb::ByteBuffer& data) const {
  if (!template_) {
    return arrow::Status::Invalid("ProtobufParser::ParseToArrow: template_ is null");
  }

  auto msg = std::unique_ptr<google::protobuf::Message>(template_->New());
  if (!msg->ParseFromArray(data.data(), static_cast<int>(data.size()))) {
    return arrow::Status::Invalid("ProtobufParser::ParseToArrow: ParseFromArray failed");
  }

  // Describe message type (best-effort).
  std::string type_name = "protobuf";
  if (msg->GetDescriptor() && !msg->GetDescriptor()->full_name().empty()) {
    type_name = msg->GetDescriptor()->full_name();
  }

  // Build struct type: struct<type: string, bytes: binary>
  auto struct_type = arrow::struct_({
      arrow::field("type", arrow::utf8(),   /*nullable=*/false),
      arrow::field("bytes", arrow::binary(), /*nullable=*/false),
  });

  auto type_scalar = std::make_shared<arrow::StringScalar>(type_name);

  // Copy input bytes into an Arrow buffer for the BinaryScalar.
  auto buf = arrow::Buffer::FromString(std::string(
      reinterpret_cast<const char*>(data.data()), data.size()));
  if (!buf) {
    return arrow::Status::OutOfMemory("ProtobufParser::ParseToArrow: Buffer::FromString returned null");
  }

  auto bytes_scalar = std::make_shared<arrow::BinaryScalar>(std::move(buf));

  arrow::ScalarVector values;
  values.reserve(2);
  values.push_back(std::move(type_scalar));
  values.push_back(std::move(bytes_scalar));

  return std::make_shared<arrow::StructScalar>(std::move(values), std::move(struct_type));
}

}  // namespace ROCKSDB_NAMESPACE
