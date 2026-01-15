#pragma once

#include "rocksdb/transformer.h"
#include "../parser/fixedbin64_parser.h"  // FixedBin64RowPayload

namespace ROCKSDB_NAMESPACE {

class FixedBin64Encoder final : public Encoder {
 public:
  InputOutputDataType OutputType() const override;
  ByteBuffer Serialize(const ParsedObject& obj) const override;

 private:
  static void AppendFixed64LE(ByteBuffer* out, std::uint64_t v);
};

}