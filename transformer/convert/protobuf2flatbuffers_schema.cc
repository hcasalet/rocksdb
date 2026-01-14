#include "protobuf2flatbuffers_schema.h"
#include <google/protobuf/util/json_util.h>
#include <flatbuffers/reflection.h>
#include <flatbuffers/util.h>
#include <sstream>

namespace ROCKSDB_NAMESPACE {

bool Protobuf2FlatbuffersSchema::Validate(const ByteBuffer& input_data) const {
    auto message = input_proto_template_->New();
    if (!message->ParseFromArray(input_data.data(), input_data.size())) {
        delete message;
        return false;
    }
    delete message;
    return true;
}

std::unique_ptr<ParsedObject> Protobuf2FlatbuffersSchema::Parse(const ByteBuffer& data) const {
  if (data.empty()) return nullptr;

  auto msg = std::make_unique<data::ByteRow>();
  if (!msg->ParseFromArray(data.data(), static_cast<int>(data.size()))) {
    return nullptr;
  }
  return std::make_unique<Protobuf2FlatbuffersParsedObject>(std::move(msg));
}
  
ByteBuffer Protobuf2FlatbuffersSchema::Serialize(const ParsedObject& obj) const {
  const auto* p = dynamic_cast<const Protobuf2FlatbuffersParsedObject*>(&obj);
  if (!p || !p->message) return {};

  flatbuffers::FlatBufferBuilder fbb;
  auto row_off = BuildFbRow(fbb, *p->message);  // now matches: const data::ByteRow&
  fbb.Finish(row_off);

  const uint8_t* buf = fbb.GetBufferPointer();
  size_t size = fbb.GetSize();
  return ByteBuffer(buf, buf + size);
}

void Protobuf2FlatbuffersSchema::BuildSchemas() {
    input_field_schema_.clear();
  
    const google::protobuf::Descriptor* descriptor = input_proto_template_->GetDescriptor();
    for (int i = 0; i < descriptor->field_count(); ++i) {
      const auto* field = descriptor->field(i);
      FieldSchema schema;
      schema.name = field->name();
      schema.field_number = field->index();
      schema.type = field->cpp_type_name();
      input_field_schema_.push_back(schema);
    }
}

flatbuffers::Offset<flat::Column> Protobuf2FlatbuffersSchema::BuildFbColumn(
      flatbuffers::FlatBufferBuilder& fbb, const data::ByteColumn& pc) {
  const std::string& pv = pc.value();  // bytes -> std::string in C++ API
  auto val_off  = fbb.CreateVector(
      reinterpret_cast<const uint8_t*>(pv.data()), pv.size());
  return flat::CreateColumn(fbb, val_off);
}

flatbuffers::Offset<flat::Row> Protobuf2FlatbuffersSchema::BuildFbRow(
      flatbuffers::FlatBufferBuilder& fbb, const data::ByteRow& pr) {
  std::vector<flatbuffers::Offset<flat::Column>> cols;
  cols.reserve(pr.values_size());
  for (const auto& c : pr.values()) {
    cols.push_back(BuildFbColumn(fbb, c));
  }
  auto cols_vec = fbb.CreateVector(cols);

  return flat::CreateRow(fbb, cols_vec);
}

}