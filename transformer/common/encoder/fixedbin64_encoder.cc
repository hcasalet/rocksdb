#include "fixedbin64_encoder.h"

namespace ROCKSDB_NAMESPACE {

InputOutputDataType FixedBin64Encoder::OutputType() const {
  return InputOutputDataType::FIXEDBIN64;
}

ByteBuffer FixedBin64Encoder::Serialize(const ParsedObject& obj) const {
  if (obj.payload.format != InputOutputDataType::FIXEDBIN64) {
    return {};
  }
  auto* row = obj.payload.As<FixedBin64RowPayload>();
  if (!row) return {};

  ByteBuffer out;
  out.reserve(row->values.size() * 8);
  for (std::uint64_t v : row->values) {
    AppendFixed64LE(&out, v);
  }
  return out;
}

void FixedBin64Encoder::AppendFixed64LE(ByteBuffer* out, std::uint64_t v) {
  out->push_back(static_cast<std::uint8_t>( v        & 0xFF));
  out->push_back(static_cast<std::uint8_t>((v >>  8) & 0xFF));
  out->push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
  out->push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
  out->push_back(static_cast<std::uint8_t>((v >> 32) & 0xFF));
  out->push_back(static_cast<std::uint8_t>((v >> 40) & 0xFF));
  out->push_back(static_cast<std::uint8_t>((v >> 48) & 0xFF));
  out->push_back(static_cast<std::uint8_t>((v >> 56) & 0xFF));
}

}