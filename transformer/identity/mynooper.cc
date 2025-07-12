#include <iostream>
#include "mynooper.h"

namespace ROCKSDB_NAMESPACE {

std::shared_ptr<void> MynooperSchema::Parse(const ByteBuffer& data) const {
    return nullptr;
}

ByteBuffer MynooperSchema::Serialize(const std::shared_ptr<void>& obj) const {
    return {};
}

void Mynooper::Transform(const std::vector<uint8_t>& input,
                          std::vector<std::vector<uint8_t>>& outputs,
                          const std::shared_ptr<SchemaDescriptor>& schema) const
{
    outputs.push_back(input);
}

}