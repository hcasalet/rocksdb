#pragma once

#include <memory>
#include <string>
#include <vector>
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include "row_generated.h"
#include "flatbuffers/flatbuffers.h"
#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

class Protobuf2FlatbuffersSchema : public SchemaDescriptor {
  public:
    Protobuf2FlatbuffersSchema(std::unique_ptr<google::protobuf::Message> input_proto_template,
                                const flatbuffers::TypeTable* flatbuffers_type_table)
        : input_proto_template_(std::move(input_proto_template)),
          flatbuffers_type_table_(flatbuffers_type_table) {
        BuildSchemas();
    }

    TransformerType SupportsTransformerType() const override { return TransformerType::CONVERTER; }

    InputOutputDataType InputType() const override { return InputOutputDataType::PROTOBUF; }
    InputOutputDataType OutputType() const override { return InputOutputDataType::FLATBUFFERS; }

    bool Validate(const ByteBuffer& input_data) const override;

    std::shared_ptr<void> Parse(const ByteBuffer& data) const override;
    ByteBuffer Serialize(const std::shared_ptr<void>& obj) const override;

    std::vector<FieldSchema> GetInputFieldSchema() const override { return input_field_schema_; }
    std::vector<std::vector<FieldSchema>> GetOutputFieldSchemas() const override { return {output_field_schema_}; }

  private:
    void BuildSchemas();

    std::unique_ptr<google::protobuf::Message> input_proto_template_;
    const flatbuffers::TypeTable* flatbuffers_type_table_;

    std::vector<FieldSchema> input_field_schema_;
    std::vector<FieldSchema> output_field_schema_;
};

} // namespace ROCKSDB_NAMESPACE