#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

  class JsonDistributorSchema : public SchemaDescriptor {
    public:
      JsonDistributorSchema(std::vector<FieldSchema> input_schema,
                            std::vector<std::vector<FieldSchema>> output_schemas)
          : input_field_schema_(std::move(input_schema)),
            output_field_schemas_(std::move(output_schemas)) {}

      InputOutputDataType InputType() const override { return InputOutputDataType::JSON; }
      InputOutputDataType OutputType() const override { return InputOutputDataType::JSON; }

      bool Validate(const ByteBuffer& input_data) const override;

      std::shared_ptr<void> Parse(const ByteBuffer& data) const override;
      ByteBuffer Serialize(const std::shared_ptr<void>& obj) const override;

      int GetNumSplits() const { return output_field_schemas_.size(); }

      void BuildSchemasFromExampleJson(const nlohmann::json& input_example,
                                       const std::vector<nlohmann::json>& output_examples);

      std::vector<FieldSchema> GetInputFieldSchema() const override {
        return input_field_schema_;
      }
    
      std::vector<std::vector<FieldSchema>> GetOutputFieldSchemas() const override {
        return output_field_schemas_;
      }

    private:
      std::vector<FieldSchema> input_field_schema_;
      std::vector<std::vector<FieldSchema>> output_field_schemas_;

      static std::vector<FieldSchema> ExtractFieldSchemasFromJson(const nlohmann::json& obj) {
        std::vector<FieldSchema> fields;
        int field_number = 1;
        if (!obj.is_object()) return fields;
      
        for (auto it = obj.begin(); it != obj.end(); ++it) {
          std::string field_type;
          if (it.value().is_string()) {
            field_type = "string";
          } else if (it.value().is_number_integer()) {
            field_type = "int";
          } else if (it.value().is_number_float()) {
            field_type = "float";
          } else if (it.value().is_boolean()) {
            field_type = "bool";
          } else if (it.value().is_array()) {
            field_type = "array";
          } else if (it.value().is_object()) {
            field_type = "object";
          } else {
            field_type = "unknown";
          }
      
          fields.push_back(FieldSchema{it.key(), field_type, field_number++});
        }

        return fields;
      }
      
  };

} // namespace ROCKSDB_NAMESPACE
 