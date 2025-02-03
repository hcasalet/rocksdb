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

    void Transform(std::string input,
                   std::vector<std::string>& outputs,
                   const std::shared_ptr<TransformerData>& data,
                   uint64_t job_id) override;
    void Prepare(uint64_t job_id) override;
    void Retrieve(uint64_t job_id, std::vector<std::pair<std::string, std::string>>& output) override;
    size_t GetStoreSize(uint64_t job_id) override;
private:
  std::vector<std::map<std::string, std::vector<std::string>>> stores_;
};

} // namespace ROCKSDB_NAMESPACE