#pragma once

#include <memory>
#include <string>
#include <vector>
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include "row_generated.h"
#include "flatbuffers/flatbuffers.h"
#include "data.pb.h"
#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

struct Protobuf2FlatbuffersParsedObject final : ParsedObject {
  explicit Protobuf2FlatbuffersParsedObject(std::unique_ptr<data::ByteRow> m)
      : message(std::move(m)) {}

  std::unique_ptr<data::ByteRow> message;
};

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

    std::unique_ptr<ParsedObject> Parse(const ByteBuffer& data) const override;
    ByteBuffer Serialize(const ParsedObject& obj) const override;

    const std::vector<FieldSchema>& GetInputFieldSchema() const override { return input_field_schema_; }
    const std::vector<std::vector<FieldSchema>>& GetOutputFieldSchemas() const override { return {output_field_schema_}; }

  private:
    void BuildSchemas();
    static flatbuffers::Offset<flat::Column> BuildFbColumn(flatbuffers::FlatBufferBuilder& fbb, const data::ByteColumn& pc);
    static flatbuffers::Offset<flat::Row> BuildFbRow(flatbuffers::FlatBufferBuilder& fbb, const data::ByteRow& pr);

    std::unique_ptr<google::protobuf::Message> input_proto_template_;
    const flatbuffers::TypeTable* flatbuffers_type_table_;

    std::vector<FieldSchema> input_field_schema_;
    std::vector<std::vector<FieldSchema>> output_field_schema_;
};

} // namespace ROCKSDB_NAMESPACE