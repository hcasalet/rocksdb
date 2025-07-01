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
                   std::vector<std::string>& outputs,
                   const std::shared_ptr<TransformerData>& data,
                   uint64_t job_id) override;
    
    TransformerType Supports() const override;
};

} // namespace ROCKSDB_NAMESPACE