#include <cstdint>  // For int32_t

#include <nlohmann/json.hpp>
#include "converter.h"
#include "columns.pb.h"

namespace ROCKSDB_NAMESPACE {

void Converter::Transform(std::string input, std::vector<std::string>* outputs, const std::shared_ptr<TransformerData>& data)
{
    auto converterData = std::dynamic_pointer_cast<ConverterData>(data);

    size_t end = input.find_last_not_of(" \t\n\r\0");
    if (end != std::string::npos) {
        input = input.substr(0, end+1);
    }
 
    switch (converterData->in_type) {
        case InputOutputDataType::PROTOBUF: {
            data::Row row;
            row.ParseFromString(input);

            flatbuffers::FlatBufferBuilder builder;
            if (converterData->column_data_type == 1) {
                std::vector<int32_t> numvals;
                for (int i = 0; i < row.columns_size(); i++) {
                    numvals.push_back(std::stoi(row.columns(i)));
                }

                auto num_vector = builder.CreateVector(numvals);
                auto fb_row_num = rocksdb::CreateFbRowNum(builder, num_vector);

                builder.Finish(fb_row_num);
            } else if (converterData->column_data_type == 2) {
                std::vector<flatbuffers::Offset<flatbuffers::String>> string_vector;
                for (int i = 0; i < row.columns_size(); i++) {
                    string_vector.push_back(builder.CreateString(row.columns(i)));
                }

                auto col_vector = builder.CreateVector(string_vector);
                auto fb_row_str = rocksdb::CreateFbRowStr(builder, col_vector);

                builder.Finish(fb_row_str);
            } else {
                std::vector<int32_t> numvals;
                std::vector<flatbuffers::Offset<flatbuffers::String>> string_vector;
                for (int i = 0; i < row.columns_size(); i++) {
                    if (i < row.columns_size()/2) {
                        numvals.push_back(std::stoi(row.columns(i)));
                    } else {
                        string_vector.push_back(builder.CreateString(row.columns(i)));
                    }
                }

                auto num_vector = builder.CreateVector(numvals);
                auto col_vector = builder.CreateVector(string_vector);
                auto fb_row = rocksdb::CreateFbRow(builder, num_vector, col_vector);

                builder.Finish(fb_row);
            }
            
            uint8_t *buf = builder.GetBufferPointer();
            int size = builder.GetSize();
            std::string s(reinterpret_cast<char*>(buf), size);
            outputs->push_back(s);

            break;
        }
        case InputOutputDataType::JSON: {
            nlohmann::json parsedJson = nlohmann::json::parse(input);
            flatbuffers::FlatBufferBuilder builder;

            std::vector<int32_t> numvals;
            numvals.push_back(parsedJson["field0"].get<int>());
            numvals.push_back(parsedJson["field1"].get<int>());
            numvals.push_back(parsedJson["field2"].get<int>());
            numvals.push_back(parsedJson["field3"].get<int>());
            numvals.push_back(parsedJson["field4"].get<int>());
            numvals.push_back(parsedJson["field5"].get<int>());
            numvals.push_back(parsedJson["field6"].get<int>());
            numvals.push_back(parsedJson["field7"].get<int>());
            auto num_vector = builder.CreateVector(numvals);

            std::vector<flatbuffers::Offset<flatbuffers::String>> strvals;
            strvals.push_back(builder.CreateString(parsedJson["field8"].get<std::string>()));
            strvals.push_back(builder.CreateString(parsedJson["field9"].get<std::string>()));
            strvals.push_back(builder.CreateString(parsedJson["field10"].get<std::string>()));
            strvals.push_back(builder.CreateString(parsedJson["field11"].get<std::string>()));
            strvals.push_back(builder.CreateString(parsedJson["field12"].get<std::string>()));
            strvals.push_back(builder.CreateString(parsedJson["field13"].get<std::string>()));
            strvals.push_back(builder.CreateString(parsedJson["field14"].get<std::string>()));
            strvals.push_back(builder.CreateString(parsedJson["field15"].get<std::string>()));
            auto col_vector = builder.CreateVector(strvals);

            auto fbRow = rocksdb::CreateFbRow(
                builder, num_vector, col_vector
            );
            builder.Finish(fbRow);

            uint8_t *buf = builder.GetBufferPointer();
            int size = builder.GetSize();
            std::string s(reinterpret_cast<char*>(buf), size);
            outputs->push_back(s);

            break;
        }
        default: {
            outputs->push_back(input);
            break;
        }
    }
    
    return;
}

void Converter::Prepare() {
    for (auto store : stores_) {
        store.clear();
    }
}

void Converter::Retrieve(int position, std::vector<std::pair<std::string, std::string>>& output) {
    return;
}

size_t Converter::GetStoreSize() {
    return stores_.size();
}

}
