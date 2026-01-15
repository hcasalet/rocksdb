#include "flatbuffers_parser.h"

// If you have a generated verifier function, you can include it here.
// Otherwise we do only size checks.
#include <cstddef>

namespace ROCKSDB_NAMESPACE {

FlatbuffersParser::FlatbuffersParser(std::string root_type)
    : root_type_(std::move(root_type)) {}

InputOutputDataType FlatbuffersParser::InputType() const {
  return InputOutputDataType::FLATBUFFERS;
}

bool FlatbuffersParser::Validate(const ByteBuffer& input_data) const {
  // Minimal check: non-empty. Replace with flatbuffers::Verifier if you can.
  return !input_data.empty();
}

std::unique_ptr<ParsedObject> FlatbuffersParser::Parse(const ByteBuffer& data) const {
  if (data.empty()) return nullptr;

  auto fb = std::make_unique<FlatbufPayload>();
  fb->bytes = data;
  fb->root_type = root_type_;

  auto out = std::make_unique<ParsedObject>();
  out->payload = ParsedPayload::Make<FlatbufPayload>(
      InputOutputDataType::FLATBUFFERS, std::move(fb));
  return out;
}

}