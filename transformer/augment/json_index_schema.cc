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

std::unique_ptr<ParsedObject> JsonAugmenterSchema::Parse(const ByteBuffer& data) const {
    std::string json_str(data.begin(), data.end());
    try {
        auto j = std::make_unique<nlohmann::json>(nlohmann::json::parse(json_str));
        return std::make_unique<JsonIndexParsedObject>(std::move(j));
    } catch (const std::exception& e) {
        throw std::runtime_error("JSON parse error: " + std::string(e.what()));
    }
}

ByteBuffer JsonAugmenterSchema::Serialize(const ParsedObject& obj) const {
    const auto* p = dynamic_cast<const JsonIndexParsedObject*>(&obj);
    if (!p) {
      throw std::invalid_argument("JsonAugmenterSchema::Serialize: wrong ParsedObject type");
    }
    if (!p->message) {
      throw std::invalid_argument("JsonAugmenterSchema::Serialize: null JSON message");
    }

    std::string json_str = p->message->dump();
    return ByteBuffer(json_str.begin(), json_str.end());
}

} // namespace ROCKSDB_NAMESPACE