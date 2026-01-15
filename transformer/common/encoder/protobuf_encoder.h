#pragma once

#include "rocksdb/transformer.h"

#include <google/protobuf/message.h>

namespace ROCKSDB_NAMESPACE {

class ProtobufEncoder final : public Encoder {
 public:
  InputOutputDataType OutputType() const override;
  ByteBuffer Serialize(const ParsedObject& obj) const override;
};

}