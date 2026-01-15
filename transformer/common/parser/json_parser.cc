#include "json_parser.h"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

namespace ROCKSDB_NAMESPACE {

InputOutputDataType JsonParser::InputType() const {
  return InputOutputDataType::JSON;
}

bool JsonParser::Validate(const ByteBuffer& input_data) const {
  rapidjson::Document d;
  d.Parse(reinterpret_cast<const char*>(input_data.data()), input_data.size());
  return !d.HasParseError() && d.IsObject();
}

std::unique_ptr<ParsedObject> JsonParser::Parse(const ByteBuffer& data) const {
  auto doc = std::make_unique<rapidjson::Document>();
  doc->Parse(reinterpret_cast<const char*>(data.data()), data.size());

  if (doc->HasParseError() || !doc->IsObject()) {
    return nullptr;
  }

  auto out = std::make_unique<ParsedObject>();
  out->payload = ParsedPayload::Make<rapidjson::Document>(
      InputOutputDataType::JSON, std::move(doc));
  return out;
}

}