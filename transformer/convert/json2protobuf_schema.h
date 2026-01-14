#pragma once

#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

struct Json2ProtobufParsedObject final : ParsedObject {
  explicit Json2ProtobufParsedObject(std::string s) : json(std::move(s)) {}
  std::string json;
};

class Json2ProtobufSchema : public SchemaDescriptor {
  public:
    Json2ProtobufSchema(nlohmann::json input_example_json,
        std::unique_ptr<google::protobuf::Message> output_message_template)
            : input_example_json_(input_example_json),
              output_message_template_(std::move(output_message_template)) {
        BuildSchemas();
    }

    TransformerType SupportsTransformerType() const override { return TransformerType::CONVERTER; }

    InputOutputDataType InputType() const override { return InputOutputDataType::JSON; }
    InputOutputDataType OutputType() const override { return InputOutputDataType::PROTOBUF; }
    bool Validate(const ByteBuffer& input_data) const override;

    std::unique_ptr<ParsedObject> Parse(const ByteBuffer& data) const override;
    ByteBuffer Serialize(const ParsedObject& obj) const override;

    const std::vector<FieldSchema>& GetInputFieldSchema() const override { return input_field_schema_; }
    const std::vector<std::vector<FieldSchema>>& GetOutputFieldSchemas() const override { return output_field_schema_; }

  private:
    void BuildSchemas();
    nlohmann::json input_example_json_;
    std::unique_ptr<google::protobuf::Message> output_message_template_;
    std::vector<FieldSchema> input_field_schema_;
    std::vector<std::vector<FieldSchema>> output_field_schema_;
};

} // namespace ROCKSDB_NAMESPACE