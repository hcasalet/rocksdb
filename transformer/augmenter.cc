#include <sstream>
#include <nlohmann/json.hpp>
#include "augmenter.h"
#include "columns.pb.h"

namespace ROCKSDB_NAMESPACE {

void Augmenter::Transform(std::string input,
                          std::vector<std::string>& outputs,
                          const std::shared_ptr<TransformerData>& data,
                          uint64_t job_id) {
    auto augmenterData = std::dynamic_pointer_cast<AugmenterData>(data);
    if (!augmenterData) {
        throw std::runtime_error("Invalid TransformerData: Failed to cast to AugmenterData.");
    }

    std::string index_key;
    switch (augmenterData->input_type) {
        case InputOutputDataType::PROTOBUF: {
            data::Row row;
            if (!row.ParseFromString(input)) {
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
            index_key = input;
            break;
        }
    }
    
    // Lock the mutex before accessing store_
    {
        std::lock_guard<std::mutex> lock(stores_mutex_);
        auto& store = stores_[job_id];
        store[index_key].push_back(augmenterData->row_key);
    }
}

void Augmenter::Prepare(uint64_t job_id) {
    std::lock_guard<std::mutex> lock(stores_mutex_);
    if (stores_.find(job_id) == stores_.end()) {
        stores_[job_id] = std::map<std::string, std::vector<std::string>>();
    } else {
        stores_[job_id].clear();
    }
}

void Augmenter::Retrieve(uint64_t job_id, std::vector<std::pair<std::string, std::string>>& output) {
    std::lock_guard<std::mutex> lock(stores_mutex_);
    auto it = stores_.find(job_id);
    if (it != stores_.end()) {
        const auto& store = it->second;
        for (const auto& entry : store) {
            std::ostringstream oss;
            for (size_t i = 0; i < entry.second.size(); ++i) {
                if (i > 0) oss << ",";  // Add comma before each element except the first
                oss << entry.second[i];
            }
            output.emplace_back(entry.first, oss.str());
        }
    } else {
        throw std::runtime_error("No store found for job_id " + std::to_string(job_id));
    }
    stores_.erase(it);
}

size_t Augmenter::GetStoreSize(uint64_t job_id) {
    std::lock_guard<std::mutex> lock(stores_mutex_);
    auto it = stores_.find(job_id);
    if (it != stores_.end()) {
        return it->second.size();
    } else {
        return 0;
    }
}

}