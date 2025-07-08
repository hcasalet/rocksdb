#include <cstdint>  // For int32_t

#include <nlohmann/json.hpp>
#include "converter.h"

namespace ROCKSDB_NAMESPACE {

std::shared_ptr<void> ConverterSchema::Parse(const ByteBuffer& data) const {
    return nullptr;
}

ByteBuffer ConverterSchema::Serialize(const std::shared_ptr<void>& obj) const {
    return {};
}

void Converter::Transform(const std::vector<uint8_t>& input,
                          std::vector<std::vector<uint8_t>>& outputs,
                          const std::shared_ptr<SchemaDescriptor>& schema) const
{
    auto converterSchema = std::dynamic_pointer_cast<ConverterSchema>(schema);
 
    flatbuffers::FlatBufferBuilder builder;
    std::vector<int32_t> numvals;
    std::vector<flatbuffers::Offset<flatbuffers::String>> strvals;
    switch (converterSchema->in_type) {
        case InputOutputDataType::PROTOBUF: {
            data::Row row;
            row.ParseFromArray(input.data(), input.size());
            if (converterSchema->column_data_type == "numeric") {
                /*numvals.push_back(row.field1());
                numvals.push_back(row.field2());
                numvals.push_back(row.field3());
                numvals.push_back(row.field4());
                numvals.push_back(row.field5());
                numvals.push_back(row.field6());
                numvals.push_back(row.field7());
                numvals.push_back(row.field8());
                numvals.push_back(row.field9());
                numvals.push_back(row.field10());
                numvals.push_back(row.field11());
                numvals.push_back(row.field12()); 
            } else if (converterSchema->column_data_type == "string") {
                strvals.push_back(builder.CreateString(row.field1()));
                strvals.push_back(builder.CreateString(row.field2()));
                strvals.push_back(builder.CreateString(row.field3()));
                strvals.push_back(builder.CreateString(row.field4()));
                strvals.push_back(builder.CreateString(row.field5()));
                strvals.push_back(builder.CreateString(row.field6()));
                strvals.push_back(builder.CreateString(row.field7()));
                strvals.push_back(builder.CreateString(row.field8()));
                strvals.push_back(builder.CreateString(row.field9()));
                strvals.push_back(builder.CreateString(row.field10()));
                strvals.push_back(builder.CreateString(row.field11()));
                strvals.push_back(builder.CreateString(row.field12())); */
            } else {
                numvals.push_back(row.field1());
                numvals.push_back(row.field2());
                numvals.push_back(row.field3());
                numvals.push_back(row.field4());
                numvals.push_back(row.field12());
                strvals.push_back(builder.CreateString(row.field5()));
                strvals.push_back(builder.CreateString(row.field6()));
                strvals.push_back(builder.CreateString(row.field7()));
                strvals.push_back(builder.CreateString(row.field8()));
                strvals.push_back(builder.CreateString(row.field9()));
                strvals.push_back(builder.CreateString(row.field10()));
                strvals.push_back(builder.CreateString(row.field11()));
            }
            break;
        }
        case InputOutputDataType::JSON: {
            nlohmann::json parsedJson = nlohmann::json::parse(input);
            if (converterSchema->column_data_type == "numeric") {
                for (const auto& element : parsedJson) {
                    numvals.push_back(element.get<int>());
                }
            } else if (converterSchema->column_data_type == "string") {
                for (const auto& element : parsedJson) {
                    strvals.push_back(builder.CreateString(element.get<std::string>()));
                }
            } else {
                for  (const auto& element : parsedJson) {
                    if (element.is_number()) {
                        numvals.push_back(element.get<int>());
                    } else if (element.is_string()) {
                        strvals.push_back(builder.CreateString(element.get<std::string>()));
                    }
                }
            }
            break;
        }
        default: {
            outputs.push_back(input);
            break;
        }
    }

    auto num_vector = builder.CreateVector(numvals);
    auto col_vector = builder.CreateVector(strvals);
    auto fb_row = rocksdb::CreateFbRow(builder, num_vector, col_vector);
    builder.Finish(fb_row);
            
    uint8_t *buf = builder.GetBufferPointer();
    int bsize = builder.GetSize();
    outputs.emplace_back(buf, buf + bsize);
    
    return;
}

}
