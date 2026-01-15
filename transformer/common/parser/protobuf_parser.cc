#include "protobuf_parser.h"

#include <google/protobuf/message.h>

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

std::unique_ptr<ParsedObject> ProtobufParser::Parse(const rocksdb::ByteBuffer& data) const {
  if (!template_) return nullptr;

  auto msg = std::unique_ptr<google::protobuf::Message>(template_->New());
  if (!msg->ParseFromArray(data.data(), static_cast<int>(data.size()))) {
    return nullptr;
  }

  auto out = std::make_unique<ParsedObject>();
  out->payload = ParsedPayload::Make<google::protobuf::Message>(
      InputOutputDataType::PROTOBUF, std::move(msg));
  return out;
}

}