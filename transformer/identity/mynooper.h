#pragma once

#include <memory>
#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

class MynooperSchema : public SchemaDescriptor {
  public:
    MynooperSchema(InputOutputDataType inT, std::vector<FieldSchema> inSchema) 
        : input_type_(inT), input_schema_(inSchema) {}

    std::unique_ptr<ParsedObject> Parse(const ByteBuffer& data) const override;

    ByteBuffer Serialize(const ParsedObject& obj) const override;

    TransformerType SupportsTransformerType() const override { return TransformerType::MYNOOPER; }

    InputOutputDataType InputType() const override { return input_type_; }

    const std::vector<FieldSchema>& GetInputFieldSchema() const override { return input_schema_; }
  private:
    InputOutputDataType input_type_;
    std::vector<FieldSchema> input_schema_;
    
};

class Mynooper : public Transformer {
public:
    Mynooper() {};
    ~Mynooper() {};

    std::string Name() const override { return "Mycelium-NoOp"; }

    virtual std::vector<ByteBuffer> Transform(
      const ByteBuffer& input_bytes,
      const std::shared_ptr<SchemaDescriptor>& schema) const override;
    
    TransformerType Supports() const override { return TransformerType::MYNOOPER; }
};

} // namespace ROCKSDB_NAMESPACE