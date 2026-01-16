#include "protobuf_encoder.h"

namespace ROCKSDB_NAMESPACE {

void ProtobufBytesRowEncoder::AppendVarint(ByteBuffer* out, uint64_t v) {
  while (v >= 0x80) {
    out->push_back(static_cast<uint8_t>((v & 0x7F) | 0x80));
    v >>= 7;
  }
  out->push_back(static_cast<uint8_t>(v));
}

ByteBuffer ProtobufBytesRowEncoder::Serialize(const ParsedObject& obj) const {
  const ColumnBytesRow* row = AsColumnBytesRow(obj);
  if (!row) return {};
  if (row->cols.size() != num_cols_) return {};

  ByteBuffer out;
  // Rough reserve: tag+len varint + bytes each.
  size_t total = 0;
  for (const auto& c : row->cols) total += 2 + c.size();
  out.reserve(total);

  // field 1, wire type 2 => tag = 0x0A
  const uint8_t kTag = 0x0A;

  for (size_t i = 0; i < num_cols_; ++i) {
    const auto& col = row->cols[i];
    out.push_back(kTag);
    AppendVarint(&out, static_cast<uint64_t>(col.size()));
    out.insert(out.end(), col.begin(), col.end());
  }
  return out;
}

}  // namespace ROCKSDB_NAMESPACE