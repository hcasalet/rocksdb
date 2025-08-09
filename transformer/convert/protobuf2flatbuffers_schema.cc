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

std::shared_ptr<void> Protobuf2FlatbuffersSchema::Parse(const ByteBuffer& data) const {
    auto message = input_proto_template_->New();
    if (!message->ParseFromArray(data.data(), data.size())) {
      delete message;
      return nullptr;
    }
    return std::shared_ptr<google::protobuf::Message>(message);
}
  
ByteBuffer Protobuf2FlatbuffersSchema::Serialize(const std::shared_ptr<void>& obj) const {
  auto* pr = static_cast<data::Row*>(obj.get());
  flatbuffers::FlatBufferBuilder fbb;
  auto row_off = BuildFbRow(fbb, *pr);
  fbb.Finish(row_off);  // sets fbdata::Row as root (matches root_type)

  auto* buf  = fbb.GetBufferPointer();
  auto  size = fbb.GetSize();
  return ByteBuffer(buf, buf + size); // copy out
}

void Protobuf2FlatbuffersSchema::BuildSchemas() {
    input_field_schema_.clear();
    output_field_schema_.clear();
  
    const google::protobuf::Descriptor* descriptor = input_proto_template_->GetDescriptor();
    for (int i = 0; i < descriptor->field_count(); ++i) {
      const auto* field = descriptor->field(i);
      FieldSchema schema;
      schema.name = field->name();
      schema.field_number = field->index();
      schema.type = field->cpp_type_name();
      input_field_schema_.push_back(schema);
    }
  
    // For output, hardcoding to numcols and strcols
    output_field_schema_.push_back({"numcols", "repeated int32", 0});
    output_field_schema_.push_back({"strcols", "repeated string", 1});
}

flatbuffers::Offset<flat::Column> Protobuf2FlatbuffersSchema::BuildFbColumn(
      flatbuffers::FlatBufferBuilder& fbb, const data::Column& pc) {
  auto name_off = fbb.CreateString(pc.name());
  const std::string& pv = pc.value();  // bytes -> std::string in C++ API
  auto val_off  = fbb.CreateVector(
      reinterpret_cast<const uint8_t*>(pv.data()), pv.size());
  return flat::CreateColumn(fbb, name_off, val_off);
}

flatbuffers::Offset<flat::Row> Protobuf2FlatbuffersSchema::BuildFbRow(
      flatbuffers::FlatBufferBuilder& fbb, const data::Row& pr) {
  std::vector<flatbuffers::Offset<flat::Column>> cols;
  cols.reserve(pr.columns_size());
  for (const auto& c : pr.columns()) {
    cols.push_back(BuildFbColumn(fbb, c));
  }
  auto cols_vec = fbb.CreateVector(cols);

  return flat::CreateRow(fbb, cols_vec);
}

}