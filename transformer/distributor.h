#pragma once

#include <memory>
#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

class DistributorData : public TransformerData {
  public:
    int splits;
    DistributorData(int num_splits) : splits(num_splits) {}
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
private:
  std::vector<std::map<std::string, std::vector<std::string>>> stores_;
};

} // namespace ROCKSDB_NAMESPACE