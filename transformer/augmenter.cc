#include <sstream>
#include <nlohmann/json.hpp>
#include "augmenter.h"
#include "columns.pb.h"

namespace ROCKSDB_NAMESPACE {

void Augmenter::Transform(const std::vector<uint8_t>& input,
                          std::vector<std::vector<uint8_t>>& outputs,
                          const std::shared_ptr<TransformerData>& data) const {
    auto augmenterData = std::dynamic_pointer_cast<AugmenterData>(data);
    if (!augmenterData) {
        throw std::runtime_error("Invalid TransformerData: Failed to cast to AugmenterData.");
    }

    std::string index_key;
    switch (augmenterData->input_type) {
        case InputOutputDataType::PROTOBUF: {
            data::Row row;
            if (!row.ParseFromArray(input.data(), input.size())) {
                throw std::runtime_error("Failed to parse row from input string.");
            }
            if (row.columns_size() <= 0 || row.columns(0).empty()) {
                throw std::runtime_error("Invalid or empty column in row.");
            }
            index_key = row.columns(0);
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
        index_key += "$$" + augmenterData->row_key;
    }
    outputs.emplace_back(index_key.begin(), index_key.end());
}

TransformerType Augmenter::Supports() const {
    return TransformerType::AUGMENTER;
}

}