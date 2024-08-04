#include <sstream>
#include "augmenter.h"
#include "columns.pb.h"

namespace ROCKSDB_NAMESPACE {

void Augmenter::Transform(std::string input, std::vector<std::string>* outputs, const std::shared_ptr<TransformerData>& data) {
    auto augmenterData = std::dynamic_pointer_cast<AugmenterData>(data);

    data::Row row;
    row.ParseFromString(input);

    for (size_t i = 0; i < derivers_.size(); i++) {
        std::vector<std::string> inputs;
        for (auto pos : derivers_[i]->positions) {
            assert(pos < row.columns_size());
            inputs.push_back(row.columns(pos).value());
        }

        std::string derived = derivers_[i]->derive(inputs);

        if (derivers_[i]->is_index) {
            stores_[i][derived].push_back(augmenterData->row_key);
        } else {
            stores_[i][augmenterData->row_key].push_back(derived);
        }
    }
}

void Augmenter::Prepare() {
    for (auto store : stores_) {
        store.clear();
    }
}

void Augmenter::Retrieve(int position, std::map<std::string, std::string> output) {
    assert(size_t(position) < stores_.size());

    for (const auto& pair : stores_[position]) {
        std::ostringstream oss;

        size_t sz = pair.second.size();
        oss.write(reinterpret_cast<const char*>(&sz), sizeof(sz));

        for (const auto& row_num : pair.second) {
            size_t row_num_len = row_num.size();
            oss.write(reinterpret_cast<const char*>(&row_num_len), sizeof(row_num_len));
            oss.write(row_num.c_str(), row_num_len);
        }

        output[pair.first] = oss.str();
    }
}

size_t Augmenter::GetStoreSize() {
    return stores_.size();
}

}