#include <sstream>
#include <nlohmann/json.hpp>
#include "augmenter.h"
#include "columns.pb.h"

namespace ROCKSDB_NAMESPACE {

void Augmenter::Transform(std::string input, std::vector<std::string>* outputs, const std::shared_ptr<TransformerData>& data) {
    auto augmenterData = std::dynamic_pointer_cast<AugmenterData>(data);
    //nlohmann::json parsedJson = nlohmann::json::parse(input);
    data::Row row;
    row.ParseFromString(input);
    store_[row.columns(0)].push_back(augmenterData->row_key);

    //for (size_t i = 0; i < derivers_.size(); i++) {
    /*std::vector<std::string> inputs;
    for (auto pos : derivers_[i]->positions) {
        assert(pos < row.columns_size());
        //assert(pos < int(parsedJson.size()));
        inputs.push_back(row.columns(pos).value());
        //nputs.push_back(std::to_string(parsedJson["field"+std::to_string(pos)].get<int>()));
    }

    std::string derived = derivers_[i]->derive(inputs);

    if (derivers_[i]->is_index) {
        stores_[i][derived].push_back(augmenterData->row_key);
    } else {
        stores_[i][augmenterData->row_key].push_back(derived);
    }*/
    //}
}

void Augmenter::Prepare() {
    //for (auto store : stores_) {
    store_.clear();
    //}
}

void Augmenter::Retrieve(int position, std::vector<std::pair<std::string, std::string>>& output) {
    //assert(size_t(position) < stores_.size());

    for (auto kv : store_) {
        std::ostringstream oss;
        for (size_t i = 0; i < kv.second.size(); ++i) {
            if (i > 0) oss << ",";  // Add comma before each element except the first
            oss << kv.second[i];
        }
        output.emplace_back(kv.first, oss.str());
    }
}

size_t Augmenter::GetStoreSize() {
    return store_.size();
}

}