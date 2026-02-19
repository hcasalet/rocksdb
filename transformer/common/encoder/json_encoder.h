#pragma once

#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

class JsonEncoder final : public Encoder {
 public:
  InputOutputDataType OutputType() const override;
  std::vector<ByteBuffer> SerializeFromArrow(const ArrowRecord& rec) const override;
};

}