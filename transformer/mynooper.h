#pragma once

#include <memory>
#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

class MynooperData : public TransformerData {
  public:
    MynooperData() {}
};

class Mynooper : public Transformer {
public:
    Mynooper() {};
    ~Mynooper() {};

    void Transform(std::string input,
                   std::vector<std::string>* outputs,
                   const std::shared_ptr<TransformerData>& data,
                   uint64_t job_id) override;
    void Prepare(uint64_t job_id) override;
    void Retrieve(uint64_t job_id, std::vector<std::pair<std::string, std::string>>& output) override;
    size_t GetStoreSize(uint64_t job_id) override;
};

} // namespace ROCKSDB_NAMESPACE