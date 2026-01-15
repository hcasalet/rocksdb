#pragma once

#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {
class JsonParser final : public Parser {
 public:
  InputOutputDataType InputType() const override;
  bool Validate(const ByteBuffer& input_data) const override;
  std::unique_ptr<ParsedObject> Parse(const ByteBuffer& data) const override;
};

}