#include <cstdint>  // For int32_t

#include <nlohmann/json.hpp>
#include "converter.h"
#include "columns.pb.h"

namespace ROCKSDB_NAMESPACE {

void Converter::Transform(std::string input,
                          std::vector<std::string>& outputs,
                          const std::shared_ptr<TransformerData>& data,
                          uint64_t job_id)
{
    auto converterData = std::dynamic_pointer_cast<ConverterData>(data);

    size_t end = input.find_last_not_of(" \t\n\r\0");
    if (end != std::string::npos) {
        input = input.substr(0, end+1);
    }
 
    flatbuffers::FlatBufferBuilder builder;
    std::vector<int32_t> numvals;
    std::vector<flatbuffers::Offset<flatbuffers::String>> strvals;
    switch (converterData->in_type) {
        case InputOutputDataType::PROTOBUF: {
            data::Row row;
            row.ParseFromString(input);
            if (converterData->column_data_type == "numeric") {
                for (int i = 0; i < row.columns_size(); i++) {
                    numvals.push_back(std::stoi(row.columns(i)));
                } 
            } else if (converterData->column_data_type == "string") {
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
            if (converterData->column_data_type == "numeric") {
                for (const auto& element : parsedJson) {
                    numvals.push_back(element.get<int>());
                }
            } else if (converterData->column_data_type == "string") {
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
    int size = builder.GetSize();
    std::string s(reinterpret_cast<char*>(buf), size);
    outputs.push_back(s);
    
    return;
}

TransformerType Converter::Supports() const {
    return TransformerType::CONVERTER;
}

}
