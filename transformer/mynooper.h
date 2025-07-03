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

    void Transform(const std::vector<uint8_t>& input,
                   std::vector<std::vector<uint8_t>>& outputs,
                   const std::shared_ptr<TransformerData>& data) const override;
    
    TransformerType Supports() const override;
};

} // namespace ROCKSDB_NAMESPACE