#pragma once

#include <memory>
#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

class MynooperSchema : public SchemaDescriptor {
  public:
    MynooperSchema() {}

    std::shared_ptr<void> Parse(const ByteBuffer& data) const override;

    ByteBuffer Serialize(const std::shared_ptr<void>& obj) const override;
};

class Mynooper : public Transformer {
public:
    Mynooper() {};
    ~Mynooper() {};

    std::string Name() const override { return "Mycelium-NoOp"; }

    void Transform(const std::vector<uint8_t>& input,
                   std::vector<std::vector<uint8_t>>& outputs,
                   const std::shared_ptr<SchemaDescriptor>& schema) const override;
    
    TransformerType Supports() const override { return TransformerType::MYNOOPER; }
};

} // namespace ROCKSDB_NAMESPACE