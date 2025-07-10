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

class AvroDistributorSchema : public SchemaDescriptor {
  public:
    AvroDistributorSchema(const avro::ValidSchema& input_schema,
                          const std::vector<avro::ValidSchema>& output_schemas);
  
    InputOutputDataType InputType() const override { return InputOutputDataType::AVRO; }
    InputOutputDataType OutputType() const override { return InputOutputDataType::AVRO; }
    bool Validate(const ByteBuffer& input_data) const override;
  
    std::shared_ptr<void> Parse(const ByteBuffer& data) const override;
    ByteBuffer Serialize(const std::shared_ptr<void>& obj) const override;
  
    std::vector<FieldSchema> GetInputFieldSchema() const override;
    std::vector<std::vector<FieldSchema>> GetOutputFieldSchemas() const override;
  
  private:
    void BuildFieldSchemas();
  
    avro::ValidSchema input_schema_;
    std::vector<avro::ValidSchema> output_schemas_;
    std::vector<FieldSchema> input_field_schema_;
    std::vector<std::vector<FieldSchema>> output_field_schemas_;
};

}