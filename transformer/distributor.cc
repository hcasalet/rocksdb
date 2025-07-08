#include <iostream>
#include <cassert>
#include <nlohmann/json.hpp>
#include "distributor.h"

namespace ROCKSDB_NAMESPACE {

void Distributor::Transform(const ByteBuffer& input,
                          std::vector<ByteBuffer>& outputs,
                          const std::shared_ptr<SchemaDescriptor>& schema) const
{
    using google::protobuf::Message;
    using google::protobuf::Reflection;
    using google::protobuf::Descriptor;
    using google::protobuf::FieldDescriptor;
    
    auto proto_schema = std::static_pointer_cast<const ProtobufDistributorSchema>(schema);
    if (proto_schema) {
        std::shared_ptr<Message> parsed = std::static_pointer_cast<Message>(proto_schema->Parse(input));
        const Reflection* refl = parsed->GetReflection();
        const auto* desc = parsed->GetDescriptor();

        int field_count = desc->field_count();
        int num_outputs = proto_schema->GetNumSplits();
        if (field_count == 0 || num_outputs == 0) {
            outputs.push_back(input);
            return;
        }
        int fields_per_output = (field_count + num_outputs - 1) / num_outputs;

        // Create N output messages
        std::vector<std::unique_ptr<Message>> output_msgs;
        for (int i = 0; i < num_outputs; ++i) {
            output_msgs.push_back(std::unique_ptr<Message>(
                proto_schema->GetOutputSchemaSpecs()[i]->New()));
        }

        for (int i = 0; i < field_count; ++i) {
            const FieldDescriptor* field = desc->field(i);
            int target = i / fields_per_output;
            if (target >= num_outputs) target = num_outputs - 1;

            Message* dst = output_msgs[target].get();

            if (field->is_repeated()) {
                int count = refl->FieldSize(*parsed, field);
                for (int j = 0; j < count; ++j) {
                    switch (field->cpp_type()) {
                        case FieldDescriptor::CPPTYPE_STRING:
                            refl->AddString(dst, field, refl->GetRepeatedString(*parsed, field, j));
                            break;
                        case FieldDescriptor::CPPTYPE_INT32:
                            refl->AddInt32(dst, field, refl->GetRepeatedInt32(*parsed, field, j));
                            break;
                        case FieldDescriptor::CPPTYPE_INT64:
                            refl->AddInt64(dst, field, refl->GetRepeatedInt64(*parsed, field, j));
                            break;
                        case FieldDescriptor::CPPTYPE_BOOL:
                            refl->AddBool(dst, field, refl->GetRepeatedBool(*parsed, field, j));
                            break;
                        case FieldDescriptor::CPPTYPE_FLOAT:
                            refl->AddFloat(dst, field, refl->GetRepeatedFloat(*parsed, field, j));
                            break;
                        case FieldDescriptor::CPPTYPE_DOUBLE:
                            refl->AddDouble(dst, field, refl->GetRepeatedDouble(*parsed, field, j));
                            break;
                        case FieldDescriptor::CPPTYPE_MESSAGE:
                            refl->AddMessage(dst, field)->CopyFrom(refl->GetRepeatedMessage(*parsed, field, j));
                            break;
                        default:
                            break;
                    }
                }
            } else if (refl->HasField(*parsed, field)) {
                switch (field->cpp_type()) {
                    case FieldDescriptor::CPPTYPE_STRING:
                        refl->SetString(dst, field, refl->GetString(*parsed, field));
                        break;
                    case FieldDescriptor::CPPTYPE_INT32:
                        refl->SetInt32(dst, field, refl->GetInt32(*parsed, field));
                        break;
                    case FieldDescriptor::CPPTYPE_INT64:
                        refl->SetInt64(dst, field, refl->GetInt64(*parsed, field));
                        break;
                    case FieldDescriptor::CPPTYPE_BOOL:
                        refl->SetBool(dst, field, refl->GetBool(*parsed, field));
                        break;
                    case FieldDescriptor::CPPTYPE_FLOAT:
                        refl->SetFloat(dst, field, refl->GetFloat(*parsed, field));
                        break;
                    case FieldDescriptor::CPPTYPE_DOUBLE:
                        refl->SetDouble(dst, field, refl->GetDouble(*parsed, field));
                        break;
                    case FieldDescriptor::CPPTYPE_MESSAGE:
                        refl->MutableMessage(dst, field)->CopyFrom(refl->GetMessage(*parsed, field));
                        break;
                    default:
                        break;
                }
            }
        }

        // Serialize each output message
        outputs.clear();
        for (const auto& msg : output_msgs) {
            std::string buf;
            msg->SerializeToString(&buf);
            outputs.emplace_back(ByteBuffer(buf.begin(), buf.end()));
        }

        return;
    }

    // Extend for any other types of schema support
    /*
    switch (vtype) {
        
        case InputOutputDataType::FLATBUFFERS: {
            const data::Row* row = data::GetRow(input.data());
            if (!row || !row->columns()) {
                outputs.push_back(input);  // fallback
                break;
            }

            int total_fields = row->columns()->size();
            int group_size = total_fields / splits;
            if (group_size < 1) {
                group_size = 1;
                splits = total_fields;
            }

            std::vector<flatbuffers::FlatBufferBuilder> builders(splits);
            std::vector<flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>>> column_vectors;

            for (int i = 0; i < splits; ++i) {
                int start = i * group_size;
                int end = (i == splits - 1) ? total_fields : start + group_size;
        
                std::vector<flatbuffers::Offset<flatbuffers::String>> column_offsets;
                for (int j = start; j < end; ++j) {
                    column_offsets.push_back(builders[i].CreateString(row->columns()->Get(j)->str()));
                }
        
                auto cols_vector = builders[i].CreateVector(column_offsets);
                auto row_offset = data::CreateRow(builders[i], cols_vector);
                builders[i].Finish(row_offset);
        
                const uint8_t* buf = builders[i].GetBufferPointer();
                size_t size = builders[i].GetSize();
                outputs.emplace_back(buf, buf + size);
            }

            break;
        }
        case InputOutputDataType::JSON: {
            nlohmann::json parsedJson = nlohmann::json::parse(input);

            int total_fields = parsedJson.size();
            splits = std::min(splits, total_fields);
            int group_size = std::max(1, total_fields / splits);

            std::vector<nlohmann::json> split_jsons(splits);

            int index = 0;
            for (auto it = parsedJson.begin(); it != parsedJson.end(); ++it, ++index) {
                int group_id = std::min(index / group_size, splits - 1);
                split_jsons[group_id][it.key()] = it.value();
            }

            for (const auto& j : split_jsons) {
                std::string jsonStr = j.dump();
                outputs.emplace_back(jsonStr.begin(), jsonStr.end());
            }

            break;
        }
        default: {
            outputs.push_back(input);
            break;
        }
    } */
}

}