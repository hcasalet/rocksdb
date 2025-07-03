#include <cstdint>  // For int32_t

#include <nlohmann/json.hpp>
#include "converter.h"
#include "columns.pb.h"

namespace ROCKSDB_NAMESPACE {

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
                for (int i = 0; i < row.columns_size(); i++) {
                    numvals.push_back(std::stoi(row.columns(i)));
                } 
            } else if (converterSchema->column_data_type == "string") {
                for (int i = 0; i < row.columns_size(); i++) {
                    strvals.push_back(builder.CreateString(row.columns(i)));
                }
            } else {
                for (int i = 0; i < row.columns_size(); i++) {
                    try {
                        numvals.push_back(std::stoi(row.columns(i)));
                    } catch (...) {
                        strvals.push_back(builder.CreateString(row.columns(i)));
                    }
                }
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

TransformerType Converter::Supports() const {
    return TransformerType::CONVERTER;
}

}
