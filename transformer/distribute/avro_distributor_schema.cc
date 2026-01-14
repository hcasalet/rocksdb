#include "avro_distributor_schema.h"
#include <avro/Generic.hh>
#include <avro/Compiler.hh>
#include <avro/Specific.hh>
#include <sstream>

namespace ROCKSDB_NAMESPACE {

AvroDistributorSchema::AvroDistributorSchema(int splits, 
                                             const avro::ValidSchema& input_schema,
                                             const std::vector<avro::ValidSchema>& output_schemas)
          : splits_(splits), input_schema_(input_schema), output_schemas_(output_schemas) {
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

std::unique_ptr<ParsedObject> AvroDistributorSchema::Parse(const ByteBuffer& data) const {
  try {
    auto decoder = avro::binaryDecoder();
    auto in = avro::memoryInputStream(data.data(), data.size());
    decoder->init(*in);

    auto parsed = std::make_unique<AvroParsedObject>(input_schema_);
    avro::decode(*decoder, parsed->datum);
    return parsed;
  } catch (...) {
    return nullptr;
  }
}

ByteBuffer AvroDistributorSchema::Serialize(const ParsedObject& obj) const {
  try {
    const auto* p = dynamic_cast<const AvroParsedObject*>(&obj);
    if (!p) {
      return {};  // wrong ParsedObject type
    }

    auto out = avro::memoryOutputStream();
    auto encoder = avro::binaryEncoder();
    encoder->init(*out);

    avro::encode(*encoder, p->datum);
    encoder->flush();  // important: finalize encoder output into the stream

    auto in = avro::memoryInputStream(*out);
    ByteBuffer buffer;
    const uint8_t* buf = nullptr;
    size_t len = 0;
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

const std::vector<FieldSchema>& AvroDistributorSchema::GetInputFieldSchema() const {
  return input_field_schema_;
}

const std::vector<std::vector<FieldSchema>>& AvroDistributorSchema::GetOutputFieldSchemas() const {
  return output_field_schemas_;
}
   
}