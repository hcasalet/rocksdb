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

            auto field8 = builder.CreateString(row.columns(8).value());
            auto field9 = builder.CreateString(row.columns(9).value());
            auto field10 = builder.CreateString(row.columns(10).value());
            auto field11 = builder.CreateString(row.columns(11).value());
            auto field12 = builder.CreateString(row.columns(12).value());
            auto field13 = builder.CreateString(row.columns(13).value());
            auto field14 = builder.CreateString(row.columns(14).value());
            auto field15 = builder.CreateString(row.columns(15).value());

            auto fbRow = rocksdb::CreateFbRow(
                builder,
                std::stoi(row.columns(0).value()),
                std::stoi(row.columns(1).value()),
                std::stoi(row.columns(2).value()),
                std::stoi(row.columns(3).value()),
                std::stoi(row.columns(4).value()),
                std::stoi(row.columns(5).value()),
                std::stoi(row.columns(6).value()),
                std::stoi(row.columns(7).value()),
                field8, field9, field10, field11, field12, field13, field14, field15
            );

            builder.Finish(fbRow);

            uint8_t *buf = builder.GetBufferPointer();
            int size = builder.GetSize();
            std::string s(reinterpret_cast<char*>(buf), size);
            outputs->push_back(s);

            break;
        }
        case InputOutputDataType::JSON: {
            nlohmann::json parsedJson = nlohmann::json::parse(input);
            flatbuffers::FlatBufferBuilder builder;

            auto field8 = builder.CreateString(parsedJson["field8"].get<std::string>());
            auto field9 = builder.CreateString(parsedJson["field9"].get<std::string>());
            auto field10 = builder.CreateString(parsedJson["field10"].get<std::string>());
            auto field11 = builder.CreateString(parsedJson["field11"].get<std::string>());
            auto field12 = builder.CreateString(parsedJson["field12"].get<std::string>());
            auto field13 = builder.CreateString(parsedJson["field13"].get<std::string>());
            auto field14 = builder.CreateString(parsedJson["field14"].get<std::string>());
            auto field15 = builder.CreateString(parsedJson["field15"].get<std::string>());

            auto fbRow = rocksdb::CreateFbRow(
                builder,
                parsedJson["field0"].get<int>(),
                parsedJson["field1"].get<int>(),
                parsedJson["field2"].get<int>(),
                parsedJson["field3"].get<int>(),
                parsedJson["field4"].get<int>(),
                parsedJson["field5"].get<int>(),
                parsedJson["field6"].get<int>(),
                parsedJson["field7"].get<int>(),
                field8, field9, field10, field11, field12, field13, field14, field15
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

void Converter::Retrieve(int position, std::map<std::string, std::string> output) {
    return;
}

size_t Converter::GetStoreSize() {
    return stores_.size();
}

}
