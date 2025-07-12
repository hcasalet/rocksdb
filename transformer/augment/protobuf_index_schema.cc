#include "protobuf_index_schema.h"
#include <google/protobuf/util/json_util.h>

namespace ROCKSDB_NAMESPACE {

void ProtobufAugmenterSchema::BuildInputSchema() {
    input_field_schema_.clear();
    
    const auto* descriptor = input_template_->GetDescriptor();
    if (!descriptor) return;
    
    for (int i = 0; i < descriptor->field_count(); ++i) {
        const auto* field = descriptor->field(i);
        FieldSchema schema;
        schema.name = field->name();
        schema.type = field->type_name();  // returns a string like "TYPE_INT32"
        schema.field_number = field->number();
        input_field_schema_.push_back(std::move(schema));
    }
}

std::shared_ptr<void> ProtobufAugmenterSchema::Parse(const ByteBuffer& data) const {
    auto message = std::unique_ptr<google::protobuf::Message>(input_template_->New());
    if (!message->ParseFromArray(data.data(), static_cast<int>(data.size()))) {
      return nullptr;
    }
    return std::shared_ptr<void>(message.release());
}

ByteBuffer ProtobufAugmenterSchema::Serialize(const std::shared_ptr<void>& obj) const {
    auto* message = static_cast<google::protobuf::Message*>(obj.get());
    std::string buffer;
    if (!message->SerializeToString(&buffer)) {
      return {};
    }
    return ByteBuffer(buffer.begin(), buffer.end());
}

}