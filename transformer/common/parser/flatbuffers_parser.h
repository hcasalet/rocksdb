#pragma once

#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {
struct FlatbufPayload {
  ByteBuffer bytes;       // owns backing buffer
  std::string root_type;  // optional tag, can be empty
};

// A Parser that validates (optional) and wraps FLATBUFFERS bytes.
class FlatbuffersParser final : public Parser {
 public:
  // root_type is optional; it is a tag for debugging/routing.
  explicit FlatbuffersParser(std::string root_type = "");

  InputOutputDataType InputType() const override;
  bool Validate(const ByteBuffer& input_data) const override;
  std::unique_ptr<ParsedObject> Parse(const ByteBuffer& data) const override;

 private:
  std::string root_type_;
};

}