#pragma once

#include "rocksdb/transformer.h"
#include "../parser/flatbuffers_parser.h"  // for FlatbufPayload
#include "row_generated.h"

namespace ROCKSDB_NAMESPACE {

class FlatbuffersEncoder final : public Encoder {
 public:
  InputOutputDataType OutputType() const override;
  std::vector<ByteBuffer> SerializeFromArrow(const ArrowRecord& rec) const override;
};

}