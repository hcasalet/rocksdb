#include <iostream>
#include "mynooper.h"

namespace ROCKSDB_NAMESPACE {

void Mynooper::Transform(std::string input, std::vector<std::string>* outputs, 
                const std::shared_ptr<TransformerData>& data, uint64_t job_id)
{
    outputs->push_back(input);
}

void Mynooper::Prepare(uint64_t job_id) {
    return;
}

void Mynooper::Retrieve(uint64_t job_id, std::vector<std::pair<std::string, std::string>>& output) {
    return;
}

size_t Mynooper::GetStoreSize(uint64_t job_id) {
    return 0;
}

}