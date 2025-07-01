#include <iostream>
#include "mynooper.h"

namespace ROCKSDB_NAMESPACE {

void Mynooper::Transform(std::string input, std::vector<std::string>& outputs, 
                const std::shared_ptr<TransformerData>& data, uint64_t job_id)
{
    outputs.push_back(input);
}

TransformerType Mynooper::Supports() const {
    return TransformerType::MYNOOPER;
}

}