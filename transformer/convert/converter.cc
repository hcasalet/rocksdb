#include <cstdint>  // For int32_t

#include <nlohmann/json.hpp>
#include "converter.h"

namespace ROCKSDB_NAMESPACE {

void Converter::Transform(const std::vector<uint8_t>& input,
                          std::vector<std::vector<uint8_t>>& outputs,
                          const std::shared_ptr<SchemaDescriptor>& schema) const
{
    if (!schema) {
        throw std::invalid_argument("Schema cannot be null.");
    }

    // Case 1: JSON to Protobuf
    if (auto json2pb = std::dynamic_pointer_cast<Json2ProtobufSchema>(schema)) {
        auto parsed = json2pb->Parse(input);
        ByteBuffer serialized = json2pb->Serialize(parsed);
        outputs.push_back(serialized);
        return;
    }

    // Case 2: Protobuf to FlatBuffers
    if (auto pb2fb = std::dynamic_pointer_cast<Protobuf2FlatbuffersSchema>(schema)) {
        auto parsed = pb2fb->Parse(input);
        ByteBuffer serialized = pb2fb->Serialize(parsed);
        outputs.push_back(serialized);
        return;
    }

    throw std::runtime_error("Unsupported schema type for Converter::Transform.");
}

}
