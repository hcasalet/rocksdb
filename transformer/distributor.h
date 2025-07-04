#pragma once

#include <memory>
#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

class DistributorSchema : public SchemaDescriptor {
  public:
    int splits;
    bool keepOriginal;
    InputOutputDataType vtype;
    DistributorSchema(int num_splits, bool keep_original, InputOutputDataType v_type) : 
      splits(num_splits), keepOriginal(keep_original), vtype(v_type) {}

    std::shared_ptr<void> Parse(const ByteBuffer& data) const override;

    ByteBuffer Serialize(const std::shared_ptr<void>& obj) const override;
};

class Distributor : public Transformer {
  public:
    Distributor() {};
    ~Distributor() {};

    std::string Name() const override { return "Distributor"; }

    void Transform(const std::vector<uint8_t>& input,
                   std::vector<std::vector<uint8_t>>& outputs,
                   const std::shared_ptr<SchemaDescriptor>& data) const override;
    
    TransformerType Supports() const override { return TransformerType::DISTRIBUTOR; }
};

} // namespace ROCKSDB_NAMESPACE