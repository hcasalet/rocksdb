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
        data::ByteRow row;
        if (!row.ParseFromArray(input.data(), input.size())) {
            std::cout << "parsing value input into Protobuf schema had an error." << std::endl;
            return;
        }

        int field_count = row.values_size();
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

        for (int g = 0; g < num_outputs; ++g) {
            data::ByteRow groupRow;

            const int start = g * fields_per_output;
            const int end   = std::min(start + fields_per_output, field_count);
            for (int i = start; i < end; ++i) {
                const auto& src = row.values(i);
                auto* c = groupRow.add_values();
                c->set_value(src.value());
            }

            std::string serialized;
            serialized.reserve(groupRow.ByteSizeLong());   // optional, avoids reallocs
            groupRow.SerializeToString(&serialized);

            outputs.emplace_back(ByteBuffer(serialized.begin(), serialized.end()));            
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