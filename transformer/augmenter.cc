#include "augmenter.h"
#include "columns.pb.h"

namespace ROCKSDB_NAMESPACE {

void Augmenter::Transform(std::string input, std::vector<std::string>* outputs, const std::shared_ptr<TransformerData>& data) {
    data::Row row;
    row.ParseFromString(input);
}

}