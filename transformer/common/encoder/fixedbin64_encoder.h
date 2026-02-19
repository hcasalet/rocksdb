#pragma once

#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

class FixedBin64Encoder final : public Encoder {
 public:
  InputOutputDataType OutputType() const override;
  std::vector<ByteBuffer> SerializeFromArrow(const ArrowRecord& rec) const override;

 private:
  static void AppendFixed64LE(ByteBuffer* out, std::uint64_t v);
};

}