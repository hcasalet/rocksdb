#include <sstream>
#include <nlohmann/json.hpp>
#include "augmenter.h"

namespace ROCKSDB_NAMESPACE {

std::shared_ptr<void> AugmenterSchema::Parse(const ByteBuffer& data) const {
    return nullptr;
}

ByteBuffer AugmenterSchema::Serialize(const std::shared_ptr<void>& obj) const {
    return {};
}

void Augmenter::Transform(const std::vector<uint8_t>& input,
                          std::vector<std::vector<uint8_t>>& outputs,
                          const std::shared_ptr<SchemaDescriptor>& schema) const {
    auto augmenterSchema = std::dynamic_pointer_cast<AugmenterSchema>(schema);
    if (!augmenterSchema) {
        throw std::runtime_error("Invalid SchemaDescriptor: Failed to cast to AugmenterSchema.");
    }

    std::string index_key;
    switch (augmenterSchema->input_type) {
        case InputOutputDataType::PROTOBUF: {
            data::Row row;
            if (!row.ParseFromArray(input.data(), input.size())) {
                throw std::runtime_error("Failed to parse row from input string.");
            }
            if (row.field1() < 0) {
                throw std::runtime_error("Invalid or empty column in row.");
            }
            index_key = row.field1();
            break;
        }
        case InputOutputDataType::JSON: {
            nlohmann::json parsedJson = nlohmann::json::parse(input);
            if (parsedJson.empty()) {
                throw std::runtime_error("Failed to parse row from input string.");
            }
            if (!parsedJson.contains("field0") || parsedJson["field0"].empty()) {
                throw std::runtime_error("Invalid or empty column in row.");
            }
            if (parsedJson["field0"].is_number()) {
                index_key = parsedJson["field0"].get<int>();
            } else {
                index_key = parsedJson["field0"].get<std::string>();
            }
            break;
        }
        default: {
            break;
        }
    }

    if (index_key != "") {
        index_key += "$$" + augmenterSchema->row_key;
    }
    outputs.emplace_back(index_key.begin(), index_key.end());
}

}