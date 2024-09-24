#pragma once

#include <memory>
#include "rocksdb/transformer.h"
#include "flat/data_generated.h"

namespace ROCKSDB_NAMESPACE {

enum class DistributorValueType {
  JSON,
  PROTOBUF,
  FLATBUFFERS
};

class DistributorData : public TransformerData {
  public:
    int splits;
    DistributorValueType vtype;
    DistributorData(int num_splits, DistributorValueType v_type) : 
      splits(num_splits), vtype(v_type) {}
};

class Distributor : public Transformer {
public:
    Distributor() {};
    ~Distributor() {};

    void Transform(std::string input,
                   std::vector<std::string>* outputs,
                   const std::shared_ptr<TransformerData>& data) override;
    void Prepare() override;
    void Retrieve(int position, std::map<std::string, std::string> output) override;
    size_t GetStoreSize() override;
private:
  std::vector<std::map<std::string, std::vector<std::string>>> stores_;
};

} // namespace ROCKSDB_NAMESPACE