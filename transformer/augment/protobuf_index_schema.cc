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

std::unique_ptr<ParsedObject> ProtobufAugmenterSchema::Parse(const ByteBuffer& data) const {
    auto msg = std::unique_ptr<google::protobuf::Message>(input_template_->New());
    if (!msg->ParseFromArray(data.data(), static_cast<int>(data.size()))) {
      return nullptr;
    }
    return std::make_unique<ProtobufIndexParsedObject>(std::move(msg));
}

ByteBuffer ProtobufAugmenterSchema::Serialize(const ParsedObject& obj) const {
  const auto* p = dynamic_cast<const ProtobufIndexParsedObject*>(&obj);
  if (!p) {
    throw std::invalid_argument(
        "ProtobufAugmenterSchema::Serialize: wrong ParsedObject type");
  }
  if (!p->message) {
    throw std::invalid_argument(
        "ProtobufAugmenterSchema::Serialize: null protobuf message");
  }
  
  std::string buffer;
  if (!p->message->SerializeToString(&buffer)) {
    return {};
  }
  return ByteBuffer(buffer.begin(), buffer.end());
}

}