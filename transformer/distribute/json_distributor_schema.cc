#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "json_distributor_schema.h"

namespace ROCKSDB_NAMESPACE {

std::unique_ptr<ParsedObject> JsonDistributorSchema::Parse(const ByteBuffer& data) const {
    try {
      std::string json(reinterpret_cast<const char*>(data.data()), data.size());

      auto parsed = std::make_unique<JsonDistParsedObject>();
      // Parse in-situ would mutate the buffer; use normal Parse on std::string data.
      parsed->doc.Parse(json.c_str());

      if (parsed->doc.HasParseError()) {
        return nullptr;
      }
      return parsed;  
    } catch (...) {
        return nullptr;
    }
}
  
ByteBuffer JsonDistributorSchema::Serialize(const ParsedObject& obj) const {
  try {
    const auto* p = dynamic_cast<const JsonDistParsedObject*>(&obj);
    if (!p) {
      return {};  // wrong ParsedObject type
    }

    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    p->doc.Accept(writer);

    const char* s = sb.GetString();
    const size_t n = sb.GetSize();
    return ByteBuffer(reinterpret_cast<const std::uint8_t*>(s),
                      reinterpret_cast<const std::uint8_t*>(s) + n);
  } catch (...) {
    return {};
  }
}
  
bool JsonDistributorSchema::Validate(const ByteBuffer& data) const {
    if (data.empty()) {
      return false;
    }

    try {
      rapidjson::Document doc;

      // Ensure null-termination by parsing from a std::string.
      std::string json(reinterpret_cast<const char*>(data.data()), data.size());
      doc.Parse(json.c_str());

      if (doc.HasParseError()) {
        return false;
      }

      if (!doc.IsObject() && !doc.IsArray()) {
        return false;
      }

      return true;
    } catch (const std::exception& e) {
      // Invalid JSON
      return false;
    }
}

void JsonDistributorSchema::BuildSchemasFromExampleJson(
        const rapidjson::Value& input_example,
        const std::vector<rapidjson::Value*>& output_examples) {
  input_field_schema_ = ExtractFieldSchemasFromJson(input_example);
  output_field_schemas_.clear();
  output_field_schemas_.reserve(output_examples.size());

  for (const rapidjson::Value* out : output_examples) {
    if (out == nullptr) {
      // If you prefer to hard-fail, you could throw or return here instead.
      output_field_schemas_.push_back({});
      continue;
    }
    output_field_schemas_.push_back(ExtractFieldSchemasFromJson(*out));
  }
}

}