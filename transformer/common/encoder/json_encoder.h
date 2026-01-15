#pragma once

#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

class JsonEncoder final : public Encoder {
 public:
  InputOutputDataType OutputType() const override;
  ByteBuffer Serialize(const ParsedObject& obj) const override;
};

}