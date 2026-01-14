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

std::unique_ptr<ParsedObject> Json2ProtobufSchema::Parse(const ByteBuffer& data) const {
  std::string json_str(reinterpret_cast<const char*>(data.data()), data.size());

  return std::make_unique<Json2ProtobufParsedObject>(std::move(json_str));
}

ByteBuffer Json2ProtobufSchema::Serialize(const ParsedObject& obj) const {
  const auto* p = dynamic_cast<const Json2ProtobufParsedObject*>(&obj);
  if (p == nullptr) {
    return {};
  }

  std::unique_ptr<google::protobuf::Message> proto(output_message_template_->New());

  google::protobuf::util::JsonParseOptions options;
  options.ignore_unknown_fields = true;

  auto status = google::protobuf::util::JsonStringToMessage(p->json, proto.get(), options);
  if (!status.ok()) return {};

  std::string out;
  if (!proto->SerializeToString(&out)) return {};
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
  std::vector<FieldSchema> ofs;
  for (int i = 0; i < descriptor->field_count(); ++i) {
    const auto* field = descriptor->field(i);
    ofs.push_back({
      field->name(),
      field->type_name(),
      field->number()
    });
  }
  output_field_schema_.push_back(ofs);
}

}  // namespace ROCKSDB_NAMESPACE