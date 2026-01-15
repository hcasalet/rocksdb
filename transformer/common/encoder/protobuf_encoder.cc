#include "protobuf_encoder.h"

namespace ROCKSDB_NAMESPACE {

InputOutputDataType ProtobufEncoder::OutputType() const {
  return InputOutputDataType::PROTOBUF;
}

ByteBuffer ProtobufEncoder::Serialize(const ParsedObject& obj) const {
  if (obj.payload.format != InputOutputDataType::PROTOBUF) {
    return {};
  }
  auto* msg = obj.payload.As<google::protobuf::Message>();
  if (!msg) return {};

  ByteBuffer out;
  const size_t size = static_cast<size_t>(msg->ByteSizeLong());
  out.resize(size);

  if (!msg->SerializeToArray(out.data(), static_cast<int>(out.size()))) {
    return {};
  }
  return out;
}

}