#pragma once

#include "rocksdb/transformer.h"
#include "../parser/flatbuffers_parser.h"  // for FlatbufPayload

namespace ROCKSDB_NAMESPACE {

class FlatbuffersEncoder final : public Encoder {
 public:
  InputOutputDataType OutputType() const override;
  ByteBuffer SerializeFromArrow(const ArrowRecord& rec) const override;
};

}