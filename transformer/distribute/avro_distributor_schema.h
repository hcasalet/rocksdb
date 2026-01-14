#pragma once

#include "rocksdb/transformer.h"
#include <memory>
#include <string>
#include <vector>
#include <avro/ValidSchema.hh>
#include <avro/Decoder.hh>
#include <avro/Encoder.hh>
#include <avro/Generic.hh>

namespace ROCKSDB_NAMESPACE {

struct AvroParsedObject final : ParsedObject {
  explicit AvroParsedObject(const avro::ValidSchema& schema)
      : datum(schema) {}

  avro::GenericDatum datum;
};

class AvroDistributorSchema : public SchemaDescriptor {
  public:
    AvroDistributorSchema(int splits, 
                          const avro::ValidSchema& input_schema,
                          const std::vector<avro::ValidSchema>& output_schemas);

    TransformerType SupportsTransformerType() const override { return TransformerType::DISTRIBUTOR; }
  
    InputOutputDataType InputType() const override { return InputOutputDataType::AVRO; }
    InputOutputDataType OutputType() const override { return InputOutputDataType::AVRO; }
    bool Validate(const ByteBuffer& input_data) const override;
  
    std::unique_ptr<ParsedObject> Parse(const ByteBuffer& data) const override;
    ByteBuffer Serialize(const ParsedObject& obj) const override;
  
    const std::vector<FieldSchema>& GetInputFieldSchema() const override;
    const std::vector<std::vector<FieldSchema>>& GetOutputFieldSchemas() const override;

    int GetNumSplits() const override { return splits_; }
  
  private:
    void BuildFieldSchemas();
  
    int splits_;
    avro::ValidSchema input_schema_;
    std::vector<avro::ValidSchema> output_schemas_;
    std::vector<FieldSchema> input_field_schema_;
    std::vector<std::vector<FieldSchema>> output_field_schemas_;
};

}