#include "flatbuffers_encoder.h"

namespace ROCKSDB_NAMESPACE {

InputOutputDataType FlatbuffersEncoder::OutputType() const {
  return InputOutputDataType::FLATBUFFERS;
}

ByteBuffer FlatbuffersEncoder::Serialize(const ParsedObject& obj) const {
  if (obj.payload.format != InputOutputDataType::FLATBUFFERS) {
    return {};
  }
  auto* fb = obj.payload.As<FlatbufPayload>();
  if (!fb) return {};

  return fb->bytes;  // copy out
}

}