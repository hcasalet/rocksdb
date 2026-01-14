#pragma once

#include <string>
#include <vector>
#include <rapidjson/document.h>
#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

  struct JsonDistParsedObject final : ParsedObject {
    JsonDistParsedObject() = default;
    explicit JsonDistParsedObject(rapidjson::Document d) : doc(std::move(d)) {}

    rapidjson::Document doc;
  };

  class JsonDistributorSchema : public SchemaDescriptor {
    public:
      JsonDistributorSchema(int splits, 
                            std::vector<FieldSchema> input_schema,
                            std::vector<std::vector<FieldSchema>> output_schemas)
          : splits_(splits),
            input_field_schema_(std::move(input_schema)),
            output_field_schemas_(std::move(output_schemas)) {}
      
      TransformerType SupportsTransformerType() const override { return TransformerType::DISTRIBUTOR; }

      InputOutputDataType InputType() const override { return InputOutputDataType::JSON; }
      InputOutputDataType OutputType() const override { return InputOutputDataType::JSON; }

      bool Validate(const ByteBuffer& input_data) const override;

      std::unique_ptr<ParsedObject> Parse(const ByteBuffer& data) const override;
      ByteBuffer Serialize(const ParsedObject& obj) const override;

      int GetNumSplits() const override { return splits_; }

      void BuildSchemasFromExampleJson(const rapidjson::Value& input_example,
                                       const std::vector<rapidjson::Value*>& output_examples);

      const std::vector<FieldSchema>& GetInputFieldSchema() const override {
        return input_field_schema_;
      }
    
      const std::vector<std::vector<FieldSchema>>& GetOutputFieldSchemas() const override {
        return output_field_schemas_;
      }

    private:
      int splits_;
      std::vector<FieldSchema> input_field_schema_;
      std::vector<std::vector<FieldSchema>> output_field_schemas_;

      static std::vector<FieldSchema> ExtractFieldSchemasFromJson(const rapidjson::Value& obj) {
        std::vector<FieldSchema> fields;
        int field_number = 1;
        if (!obj.IsObject()) return fields;
        fields.reserve(obj.MemberCount());
      
        for (auto it = obj.MemberBegin(); it != obj.MemberEnd(); ++it) {
          const rapidjson::Value& v = it->value;
          std::string field_type;
          if (v.IsString()) {
            field_type = "string";
          } else if (v.IsInt64() || v.IsUint64()) {
            field_type = "int";
          } else if (v.IsDouble()) {
            field_type = "float";
          } else if (v.IsBool()) {
            field_type = "bool";
          } else if (v.IsArray()) {
            field_type = "array";
          } else if (v.IsObject()) {
            field_type = "object";
          } else if (v.IsNull()) {
            field_type = "null";
          } else {
            field_type = "unknown";
          }

          const auto& name = it->name;
          std::string key(name.GetString(), name.GetStringLength());

          fields.push_back(FieldSchema{std::move(key), std::move(field_type), field_number++});
        }

        return fields;
      }
      
  };

} // namespace ROCKSDB_NAMESPACE
 