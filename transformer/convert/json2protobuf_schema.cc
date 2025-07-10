#include "json2protobuf_schema.h"
#include <google/protobuf/util/json_util.h>
#include <iostream>

namespace ROCKSDB_NAMESPACE {

bool Json2ProtobufSchema::Validate(const ByteBuffer& input_data) const {
  try {
    std::string json_str(reinterpret_cast<const char*>(input_data.data()), input_data.size());
    nlohmann::json parsed = nlohmann::json::parse(json_str);
    return parsed.is_object();
  } catch (...) {
    return false;
  }
}

std::shared_ptr<void> Json2ProtobufSchema::Parse(const ByteBuffer& data) const {
  auto proto = output_message_template_->New();
  std::string json_str(reinterpret_cast<const char*>(data.data()), data.size());
  google::protobuf::util::JsonParseOptions options;
  options.ignore_unknown_fields = true;

  auto status = google::protobuf::util::JsonStringToMessage(json_str, proto, options);
  if (!status.ok()) {
    delete proto;
    return nullptr;
  }
  return std::shared_ptr<google::protobuf::Message>(proto);
}

ByteBuffer Json2ProtobufSchema::Serialize(const std::shared_ptr<void>& obj) const {
  auto* message = static_cast<google::protobuf::Message*>(obj.get());
  std::string out;
  google::protobuf::util::JsonPrintOptions options;
  options.add_whitespace = false;
  options.always_print_primitive_fields = true;

  auto status = google::protobuf::util::MessageToJsonString(*message, &out, options);
  if (!status.ok()) {
    return {};
  }

  return ByteBuffer(out.begin(), out.end());
}

void Json2ProtobufSchema::BuildSchemas() {
  input_field_schema_.clear();
  output_field_schema_.clear();

  // Build input schema from input_example_json_
  if (input_example_json_.is_object()) {
    int index = 0;
    for (auto it = input_example_json_.begin(); it != input_example_json_.end(); ++it, ++index) {
      input_field_schema_.push_back({
        it.key(),
        it.value().type_name(),
        index
      });
    }
  }

  // Build output schema from protobuf descriptor
  const auto* descriptor = output_message_template_->GetDescriptor();
  for (int i = 0; i < descriptor->field_count(); ++i) {
    const auto* field = descriptor->field(i);
    output_field_schema_.push_back({
      field->name(),
      field->type_name(),
      field->number()
    });
  }
}

}  // namespace ROCKSDB_NAMESPACE