#include <iostream>
#include "nooper.h"

namespace ROCKSDB_NAMESPACE {

void Nooper::Transform(std::string input, std::vector<std::string>* outputs, 
                const std::shared_ptr<TransformerData>& data, uint64_t job_id)
{
    outputs->push_back(input);
}

void Nooper::Prepare(uint64_t job_id) {
    return;
}

void Nooper::Retrieve(uint64_t job_id, std::vector<std::pair<std::string, std::string>>& output) {
    return;
}

size_t Nooper::GetStoreSize(uint64_t job_id) {
    return 0;
}

}