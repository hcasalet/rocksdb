#include <iostream>
#include "mynooper.h"

namespace ROCKSDB_NAMESPACE {

void Mynooper::Transform(const std::vector<uint8_t>& input,
                          std::vector<std::vector<uint8_t>>& outputs,
                          const std::shared_ptr<TransformerData>& data) const
{
    outputs.push_back(input);
}

TransformerType Mynooper::Supports() const {
    return TransformerType::MYNOOPER;
}

}