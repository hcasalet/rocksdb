#include <sstream>
#include <nlohmann/json.hpp>
#include "augmenter.h"

namespace ROCKSDB_NAMESPACE {

void Augmenter::Transform(const std::vector<uint8_t>& input,
                          std::vector<std::vector<uint8_t>>& outputs,
                          const std::shared_ptr<SchemaDescriptor>& schema) const {
    if (outputs.empty()) {
        throw std::runtime_error("Expected at least one output slot for augmented result.");
    }
   
    if (auto jsonIndexSchema = std::dynamic_pointer_cast<JsonAugmenterSchema>(schema)) {
        auto index_keys = jsonIndexSchema->GetIndexKeys();
        if (index_keys.size() != outputs.size()) {
            throw std::runtime_error("Expected outputs to have the same size as the number of indexes to be created.");
        }
        nlohmann::json parsedJson = nlohmann::json::parse(input, nullptr, false);
        if (parsedJson.is_discarded() || parsedJson.empty()) {
            throw std::runtime_error("Failed to parse row from input string.");
        }

        for (size_t i = 0; i < index_keys.size(); ++i) {

            std::string prefixIndexKey;

            for (const auto& keyField : index_keys[i]) {
                if (!prefixIndexKey.empty()) {
                    prefixIndexKey += "%%";
                }
                if (!parsedJson.contains(keyField) || parsedJson[keyField].is_null()) {
                    throw std::runtime_error("Missing or null field: " + keyField);
                }
                if (parsedJson[keyField].is_number()) {
                    prefixIndexKey += std::to_string(parsedJson[keyField].get<int>());
                } else {
                    prefixIndexKey += parsedJson[keyField].get<std::string>();
                }
            }
            std::string combined = prefixIndexKey + "$$$" + std::string(outputs[i].begin(), outputs[i].end());
            outputs[i] = std::vector<uint8_t>(combined.begin(), combined.end());
        }
    } else if (auto protoIndexSchema = std::dynamic_pointer_cast<ProtobufAugmenterSchema>(schema)) {
        auto index_keys = protoIndexSchema->GetIndexKeys();
        if (index_keys.size() != outputs.size()) {
            throw std::runtime_error("Expected outputs to have the same size as the number of indexes to be created.");
        }
        data::Row row;
        if (!row.ParseFromArray(input.data(), input.size())) {
            throw std::runtime_error("Failed to parse row from input string.");
        }
        const auto* descriptor = row.GetDescriptor();
        const auto* reflection = row.GetReflection();

        for (size_t i = 0; i < index_keys.size(); ++i) {

            std::string prefixIndexKey;

            for (auto& field_name : index_keys[i]) {
                if (!prefixIndexKey.empty()) {
                    prefixIndexKey += "%%";
                }
                const auto* field = descriptor->FindFieldByName(field_name);
                if (!field || !reflection->HasField(row, field)) {
                    throw std::runtime_error("Missing or invalid field: " + field_name);
                }

                switch (field->cpp_type()) {
                    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
                        prefixIndexKey += std::to_string(reflection->GetInt32(row, field));
                        break;
                    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
                        prefixIndexKey += reflection->GetString(row, field);
                        break;
                    default:
                        throw std::runtime_error("Unsupported field type for: " + field_name);
                }
            }
            std::string combined = prefixIndexKey + "$$$" + std::string(outputs[i].begin(), outputs[i].end());
            outputs[i] = std::vector<uint8_t>(combined.begin(), combined.end());
        }
    } else {
        throw std::runtime_error("Invalid SchemaDescriptor: Failed to cast to AugmenterSchema.");
        return;
    }
}

}