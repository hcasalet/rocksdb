#include "protobuf_distributor_schema.h"

#include <google/protobuf/message.h>
#include <google/protobuf/dynamic_message.h>
#include <cassert>

namespace ROCKSDB_NAMESPACE {

std::shared_ptr<void> ProtobufDistributorSchema::Parse(const ByteBuffer& data) const {
    if (!input_proto_msgtype_ || data.empty()) return nullptr;

    std::unique_ptr<google::protobuf::Message> msg(input_proto_msgtype_->New());
    if (!msg->ParseFromArray(data.data(), static_cast<int>(data.size()))) {
        return nullptr;
    }
    return std::shared_ptr<void>(msg.release()); // hand over ownership
}

ByteBuffer ProtobufDistributorSchema::Serialize(const std::shared_ptr<void>& obj) const {
    auto* msg = static_cast<google::protobuf::Message*>(obj.get());
    ByteBuffer buffer;
    size_t size = msg->ByteSizeLong();
    buffer.resize(size);
    msg->SerializeToArray(buffer.data(), static_cast<int>(size));
    return buffer;
}

bool ProtobufDistributorSchema::Validate(const ByteBuffer& input_data) const {
    if (input_data.empty()) return false;

    std::unique_ptr<google::protobuf::Message> msg(input_proto_msgtype_->New());

    return msg->ParseFromArray(input_data.data(), static_cast<int>(input_data.size()));
}

}