#include "protobuf_distributor_schema.h"

#include <google/protobuf/message.h>
#include <google/protobuf/dynamic_message.h>
#include <cassert>

namespace ROCKSDB_NAMESPACE {

std::unique_ptr<ParsedObject> ProtobufDistributorSchema::Parse(const ByteBuffer& data) const {
  if (!input_proto_msgtype_ || data.empty()) return nullptr;

  auto msg = std::unique_ptr<google::protobuf::Message>(input_proto_msgtype_->New());
  if (!msg->ParseFromArray(data.data(), static_cast<int>(data.size()))) {
    return nullptr;
  }

  return std::make_unique<ProtobufDistributorParsedObject>(std::move(msg));
}

ByteBuffer ProtobufDistributorSchema::Serialize(const ParsedObject& obj) const {
  const auto* p = dynamic_cast<const ProtobufDistributorParsedObject*>(&obj);
  if (!p || !p->message) {
    return {};
  }

  const size_t size = p->message->ByteSizeLong();
  if (size > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return {};  // defensive: SerializeToArray takes int
  }

  ByteBuffer buffer(size);
  if (!p->message->SerializeToArray(buffer.data(), static_cast<int>(size))) {
    return {};
  }
  return buffer;
}

bool ProtobufDistributorSchema::Validate(const ByteBuffer& input_data) const {
    if (input_data.empty()) return false;

    std::unique_ptr<google::protobuf::Message> msg(input_proto_msgtype_->New());

    return msg->ParseFromArray(input_data.data(), static_cast<int>(input_data.size()));
}

}