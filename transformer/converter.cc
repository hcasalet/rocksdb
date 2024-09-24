#include "converter.h"
#include "columns.pb.h"
#include "json.hpp"

using json = nlohmann::json;

namespace ROCKSDB_NAMESPACE {

void Converter::Transform(std::string input, std::vector<std::string>* outputs, const std::shared_ptr<TransformerData>& data)
{
    auto converterData = std::dynamic_pointer_cast<ConverterData>(data);

    size_t end = input.find_last_not_of(" \t\n\r\0");
    if (end != std::string::npos) {
        input = input.substr(0, end+1);
    }
 
    switch (converterData->in_type) {
        case ConverterInputType::PROTOBUF: {
            data::Row row;
            row.ParseFromString(input);

            flatbuffers::FlatBufferBuilder builder;

            // Add the columns as uint64s
            std::vector<flatbuffers::Offset<NumericColumn>> numericCols;
            for (int i = 0; i < row.columns_size(); ++i) {
                try {
                    const std::string& col_name_str = row.columns(i).name();
                    const std::string& value_str = row.columns(i).value();
                    if (col_name_str.empty() || value_str.empty()) {
                        //std::cerr << "Empty column name or value encountered at index: " << i << std::endl;
                        continue;  // Skip this column if either is empty
                    }
                    auto col_name = builder.CreateString(col_name_str);
                    auto col = CreateNumericColumn(builder, col_name, std::stoull(value_str));
                    numericCols.push_back(col);
                } catch (const std::invalid_argument& ia) {
                    std::cerr << "Catching invalid argument exception: " << ia.what() << std::endl;
                    return;
                } catch (const std::out_of_range& orr) {
                    std::cerr << "Catching out of range exception: " << orr.what() << std::endl;
                    return;
                }
            }

            // Add vectors to the builder
            auto numericVec = builder.CreateVector(numericCols);

            // Create the FbRow object
            auto fbRow = CreateFbRow(builder, numericVec);

            builder.Finish(fbRow);

            uint8_t *buf = builder.GetBufferPointer();
            int size = builder.GetSize();
            std::string s(reinterpret_cast<char*>(buf), size);
            outputs->push_back(s);

            break;
        }
        case ConverterInputType::JSON: {
            json reader = json::parse(input);
            /**
             * Todo: Need to figure out how to compact data once it is 
             *       converted to Arrow format before we proceed.
             */
            break;
        }
        default: {
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
