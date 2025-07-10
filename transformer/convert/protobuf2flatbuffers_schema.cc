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
    auto* proto_msg = static_cast<google::protobuf::Message*>(obj.get());
  
    flatbuffers::FlatBufferBuilder builder;
  
    // Assuming flatbuffers table is FbRow with fields: numcols:[int32], strcols:[string]
    std::vector<int32_t> numcols;
    std::vector<flatbuffers::Offset<flatbuffers::String>> strcols;
  
    const google::protobuf::Descriptor* descriptor = proto_msg->GetDescriptor();
    const google::protobuf::Reflection* reflection = proto_msg->GetReflection();
  
    for (int i = 0; i < descriptor->field_count(); ++i) {
      const auto* field = descriptor->field(i);
      if (field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_INT32) {
        if (field->is_repeated()) {
          int count = reflection->FieldSize(*proto_msg, field);
          for (int j = 0; j < count; ++j) {
            numcols.push_back(reflection->GetRepeatedInt32(*proto_msg, field, j));
          }
        } else {
          numcols.push_back(reflection->GetInt32(*proto_msg, field));
        }
      } else if (field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_STRING) {
        if (field->is_repeated()) {
          int count = reflection->FieldSize(*proto_msg, field);
          for (int j = 0; j < count; ++j) {
            auto str = reflection->GetRepeatedString(*proto_msg, field, j);
            strcols.push_back(builder.CreateString(str));
          }
        } else {
          auto str = reflection->GetString(*proto_msg, field);
          strcols.push_back(builder.CreateString(str));
        }
      }
    }
  
    auto numcols_vec = builder.CreateVector(numcols);
    auto strcols_vec = builder.CreateVector(strcols);
  
    auto fb_row = flat::CreateFbRow(builder, numcols_vec, strcols_vec);
    builder.Finish(fb_row);
  
    return ByteBuffer(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
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

}