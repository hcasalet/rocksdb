#pragma once

#include <memory>
#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

class DistributorData : public TransformerData {
  public:
    int splits;
    bool keepOriginal;
    InputOutputDataType vtype;
    DistributorData(int num_splits, bool keep_original, InputOutputDataType v_type) : 
      splits(num_splits), keepOriginal(keep_original), vtype(v_type) {}
};

class Distributor : public Transformer {
public:
    Distributor() {};
    ~Distributor() {};

    void Transform(const std::vector<uint8_t>& input,
                   std::vector<std::vector<uint8_t>>& outputs,
                   const std::shared_ptr<TransformerData>& data) const override;
    
    TransformerType Supports() const override;
private:
  std::vector<std::map<std::string, std::vector<std::string>>> stores_;
};

} // namespace ROCKSDB_NAMESPACE