#include <sstream>
#include <nlohmann/json.hpp>
#include "rocksdb/slice.h"
#include "augmenter.h"

namespace ROCKSDB_NAMESPACE {

void Augmenter::Transform(const std::vector<uint8_t>& input,
                          std::vector<std::vector<uint8_t>>& outputs,
                          const std::shared_ptr<SchemaDescriptor>& schema) const {
    const std::string separator = "$$";
    auto it = std::search(input.begin(), input.end(), separator.begin(), separator.end());

    if (it == input.end()) {
        throw std::runtime_error("Separator '$$' not found in input");
    }

    size_t sep_pos = std::distance(input.begin(), it);
    size_t sep_len = separator.size();

    Slice value(reinterpret_cast<const char*>(input.data()), sep_pos);
    Slice key(reinterpret_cast<const char*>(input.data() + sep_pos + sep_len),
                                            input.size() - sep_pos - sep_len);
    const std::string key_field_separator = "%%";
    const std::string original_key_separator = "$$$KEY$$$";
   
    if (auto jsonIndexSchema = std::dynamic_pointer_cast<JsonAugmenterSchema>(schema)) {
        const auto& index_keys = jsonIndexSchema->GetIndexKeys();
        nlohmann::json parsedJson = nlohmann::json::parse(std::string(value.data(), value.size()), nullptr, false);

        if (parsedJson.is_discarded() || parsedJson.empty()) {
            throw std::runtime_error("Failed to parse row from input string.");
        }

        for (const auto& key_fields : index_keys) {
            std::vector<uint8_t> prefixIndexKey;

            for (const auto& keyField : key_fields) {
                if (!prefixIndexKey.empty()) {
                    prefixIndexKey.insert(prefixIndexKey.end(), 
                                          reinterpret_cast<const uint8_t*>(key_field_separator.data()),
                                          reinterpret_cast<const uint8_t*>(key_field_separator.data() + key_field_separator.size()));
                }
                if (!parsedJson.contains(keyField) || parsedJson[keyField].is_null()) {
                    throw std::runtime_error("Missing or null field: " + keyField);
                }
                if (parsedJson[keyField].is_number()) {
                    std::string numstr = std::to_string(parsedJson[keyField].get<int>());
                    prefixIndexKey.insert(prefixIndexKey.end(), numstr.begin(), numstr.end());
                } else {
                    std::string fieldstr = parsedJson[keyField].get<std::string>();
                    prefixIndexKey.insert(prefixIndexKey.end(), fieldstr.begin(), fieldstr.end());
                }
            }

            prefixIndexKey.insert(prefixIndexKey.end(), original_key_separator.begin(), original_key_separator.end());
            prefixIndexKey.insert(prefixIndexKey.end(),
                                  reinterpret_cast<const uint8_t*>(key.data()),
                                  reinterpret_cast<const uint8_t*>(key.data() + key.size()));

            outputs.emplace_back(std::move(prefixIndexKey));
        }
    } else if (auto protoIndexSchema = std::dynamic_pointer_cast<ProtobufAugmenterSchema>(schema)) {
        auto index_keys = protoIndexSchema->GetIndexKeys();
    
        data::ByteRow row;
        if (!row.ParseFromArray(value.data(), value.size())) {
            throw std::runtime_error("Failed to parse row from input string.");
        }
        const auto* descriptor = row.GetDescriptor();
        const auto* reflection = row.GetReflection();

        for (const auto& key_fields : index_keys) {
            std::vector<uint8_t> prefixIndexKey;

            for (auto& field_name : key_fields) {
                if (!prefixIndexKey.empty()) {
                    prefixIndexKey.insert(prefixIndexKey.end(),
                                          reinterpret_cast<const uint8_t*>(key_field_separator.data()),
                                          reinterpret_cast<const uint8_t*>(key_field_separator.data() + key_field_separator.size()));
                }
                const auto* field = descriptor->FindFieldByName(field_name);
                if (!field || !reflection->HasField(row, field)) {
                    throw std::runtime_error("Missing or invalid field: " + field_name);
                }

                switch (field->cpp_type()) {
                    case google::protobuf::FieldDescriptor::CPPTYPE_INT32: {
                        std::string numstr = std::to_string(reflection->GetInt32(row, field));
                        prefixIndexKey.insert(prefixIndexKey.end(), numstr.begin(), numstr.end());
                        break;
                    }
                    case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
                        std::string fieldstr = reflection->GetString(row, field);
                        prefixIndexKey.insert(prefixIndexKey.end(), fieldstr.begin(), fieldstr.end());
                        break;
                    }
                    default: {
                        throw std::runtime_error("Unsupported field type for: " + field_name);
                    }
                }
            }
            prefixIndexKey.insert(prefixIndexKey.end(), original_key_separator.begin(), original_key_separator.end());
            prefixIndexKey.insert(prefixIndexKey.end(),
                                  reinterpret_cast<const uint8_t*>(key.data()),
                                  reinterpret_cast<const uint8_t*>(key.data() + key.size()));
            outputs.emplace_back(std::move(prefixIndexKey));
        }
    } else {
        throw std::runtime_error("Invalid SchemaDescriptor: Failed to cast to AugmenterSchema.");
        return;
    }
}

}