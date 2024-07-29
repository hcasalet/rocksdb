#pragma once

#include <memory>
#include "rocksdb/transformer.h"
#include "rocksdb/rocksdb_namespace.h"

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
};

} // namespace ROCKSDB_NAMESPACE