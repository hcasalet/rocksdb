#include "json_index_schema.h"
#include <stdexcept>

namespace ROCKSDB_NAMESPACE {

void JsonAugmenterSchema::BuildInputSchema() {
    input_field_schema_.clear();
    if (!input_template_.is_object()) {
        throw std::invalid_argument("Input JSON must be an object");
    }

    int field_number = 1;
    for (auto it = input_template_.begin(); it != input_template_.end(); ++it) {
        std::string type_str = it.value().type_name();
        input_field_schema_.push_back(FieldSchema{it.key(), type_str, field_number++});
    }
}

std::shared_ptr<void> JsonAugmenterSchema::Parse(const ByteBuffer& data) const {
    std::string json_str(data.begin(), data.end());
    try {
        auto parsed = std::make_shared<nlohmann::json>(nlohmann::json::parse(json_str));
        return parsed;
    } catch (const std::exception& e) {
        throw std::runtime_error("JSON parse error: " + std::string(e.what()));
    }
}

ByteBuffer JsonAugmenterSchema::Serialize(const std::shared_ptr<void>& obj) const {
    auto json_obj = std::static_pointer_cast<nlohmann::json>(obj);
    std::string json_str = json_obj->dump();
    return ByteBuffer(json_str.begin(), json_str.end());
}

} // namespace ROCKSDB_NAMESPACE