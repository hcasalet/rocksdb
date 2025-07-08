#include "json_distributor_schema.h"
#include <nlohmann/json.hpp>

namespace ROCKSDB_NAMESPACE {

using json = nlohmann::json;

std::shared_ptr<void> JsonDistributorSchema::Parse(const ByteBuffer& data) const {
    try {
        auto str = std::string(data.begin(), data.end());
        auto parsed_json = std::make_shared<json>(json::parse(str));
        return parsed_json;
    } catch (...) {
        return nullptr;
    }
}
  
ByteBuffer JsonDistributorSchema::Serialize(const std::shared_ptr<void>& obj) const {
    auto json_ptr = std::static_pointer_cast<json>(obj);
    auto s = json_ptr->dump();
    return ByteBuffer(s.begin(), s.end());
}
  
bool JsonDistributorSchema::Validate(const ByteBuffer& data) const {
    if (data.empty()) {
      return false;
    }

    try {
      nlohmann::json j = nlohmann::json::parse(data.begin(), data.end());
    
      if (!j.is_object() && !j.is_array()) {
        return false;
      }
    
      return true;
    } catch (const std::exception& e) {
      // Invalid JSON
      return false;
    }
}

void JsonDistributorSchema::BuildSchemasFromExampleJson(
        const nlohmann::json& input_example,
        const std::vector<nlohmann::json>& output_examples) {
  input_field_schema_ = ExtractFieldSchemasFromJson(input_example);
  output_field_schemas_.clear();

  for (const auto& out : output_examples) {
    output_field_schemas_.push_back(ExtractFieldSchemasFromJson(out));
  }
}

}