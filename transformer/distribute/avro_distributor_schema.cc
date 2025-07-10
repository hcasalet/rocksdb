#include "avro_distributor_schema.h"
#include <avro/Generic.hh>
#include <avro/Compiler.hh>
#include <avro/Specific.hh>
#include <sstream>

namespace ROCKSDB_NAMESPACE {

AvroDistributorSchema::AvroDistributorSchema(const avro::ValidSchema& input_schema,
                                             const std::vector<avro::ValidSchema>& output_schemas)
          : input_schema_(input_schema), output_schemas_(output_schemas) {
  BuildFieldSchemas();
}

bool AvroDistributorSchema::Validate(const ByteBuffer& input_data) const {
  if (input_data.empty()) return false;
  try {
    auto decoder = avro::binaryDecoder();
    std::unique_ptr<avro::InputStream> in = avro::memoryInputStream(input_data.data(), input_data.size());
    decoder->init(*in);
    avro::GenericDatum datum(input_schema_);
    avro::decode(*decoder, datum);
    return true;
  } catch (...) {
    return false;
  }
}

std::shared_ptr<void> AvroDistributorSchema::Parse(const ByteBuffer& data) const {
  try {
    auto decoder = avro::binaryDecoder();
    std::unique_ptr<avro::InputStream> in = avro::memoryInputStream(data.data(), data.size());
    decoder->init(*in);
    auto datum = std::make_shared<avro::GenericDatum>(input_schema_);
    avro::decode(*decoder, *datum);
    return datum;
  } catch (...) {
    return nullptr;
  }
}

ByteBuffer AvroDistributorSchema::Serialize(const std::shared_ptr<void>& obj) const {
  try {
    auto datum = std::static_pointer_cast<avro::GenericDatum>(obj);
    std::unique_ptr<avro::OutputStream> out = avro::memoryOutputStream();
    auto encoder = avro::binaryEncoder();
    encoder->init(*out);
    avro::encode(*encoder, *datum);

    std::unique_ptr<avro::InputStream> in = avro::memoryInputStream(*out);
    ByteBuffer buffer;
    const uint8_t* buf;
    size_t len;
    while (in->next(&buf, &len)) {
      buffer.insert(buffer.end(), buf, buf + len);
    }
    return buffer;
  } catch (...) {
    return {};
  }
}

void AvroDistributorSchema::BuildFieldSchemas() {
  // Input schema
  input_field_schema_.clear();
  const avro::NodePtr& input_root = input_schema_.root();
  for (size_t i = 0; i < input_root->leaves(); ++i) {
    input_field_schema_.push_back({
      input_root->nameAt(i),
      avro::toString(input_root->leafAt(i)->type()),
      static_cast<int>(i)
    });
  }

  // Output schemas
  output_field_schemas_.clear();
  for (const auto& schema : output_schemas_) {
    const avro::NodePtr& root = schema.root();
    std::vector<FieldSchema> fields;
    for (size_t i = 0; i < root->leaves(); ++i) {
      fields.push_back({
        root->nameAt(i),
        avro::toString(root->leafAt(i)->type()),
        static_cast<int>(i)
      });
    }
    output_field_schemas_.push_back(fields);
  }
}

std::vector<FieldSchema> AvroDistributorSchema::GetInputFieldSchema() const {
  return input_field_schema_;
}

std::vector<std::vector<FieldSchema>> AvroDistributorSchema::GetOutputFieldSchemas() const {
  return output_field_schemas_;
}
   
}