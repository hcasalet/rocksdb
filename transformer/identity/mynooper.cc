#include <iostream>
#include "mynooper.h"

namespace ROCKSDB_NAMESPACE {

std::unique_ptr<ParsedObject> MynooperSchema::Parse(const ByteBuffer& data) const {
    return nullptr;
}

ByteBuffer MynooperSchema::Serialize(const ParsedObject& obj) const {
    return {};
}

std::vector<ByteBuffer> Mynooper::Transform(
      const ByteBuffer& input_bytes,
      const std::shared_ptr<SchemaDescriptor>& schema) const
{
    std::vector<ByteBuffer> outputs;
    outputs.push_back(input_bytes);
    return outputs;
}

}