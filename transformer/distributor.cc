#include <iostream>
#include <nlohmann/json.hpp>
#include "distributor.h"
#include "columns.pb.h"

namespace ROCKSDB_NAMESPACE {

void Distributor::Transform(const std::vector<uint8_t>& input,
                          std::vector<std::vector<uint8_t>>& outputs,
                          const std::shared_ptr<SchemaDescriptor>& schema) const
{
    auto distributorSchema = std::dynamic_pointer_cast<DistributorSchema>(schema);
    if (distributorSchema->keepOriginal) {
        outputs.push_back(input);
    }

    int splits = distributorSchema->splits;
    InputOutputDataType vtype = distributorSchema->vtype;

    switch (vtype) {
        case InputOutputDataType::PROTOBUF: {
            data::Row row;
            row.ParseFromArray(input.data(), input.size());
            int group_size = row.columns_size()/splits;
            if (group_size < 1) {
                group_size = 1;
                splits = row.columns_size();
            }

            for (int i = 0; i < splits; i++) {
                data::Row splittedRow;
              
                for (int j = 0; j < group_size; j++) {
                    splittedRow.add_columns(row.columns(i*group_size+j));
                }

                // any leftovers gets added to the last collection
                if (i == splits-1) {
                    for (int j = 0; j < row.columns_size()-splits*group_size; j++) {
                        splittedRow.add_columns(row.columns(splits*group_size+j));
                    }
                }
             
                std::vector<uint8_t> serializedRow;
                size_t output_len = splittedRow.ByteSizeLong();
                serializedRow.resize(output_len);
                splittedRow.SerializeToArray(serializedRow.data(), output_len);
                outputs.push_back(serializedRow);
            }

            break;
        }
        case InputOutputDataType::FLATBUFFERS: {
            /**
             * not implemented
             */
            
            break;
        }
        case InputOutputDataType::JSON: {
            nlohmann::json parsedJson = nlohmann::json::parse(input);
            int group_size = parsedJson.size()/splits;
            if (group_size < 1) {
                group_size = 1;
                splits = parsedJson.size();
            }

            for (int i = 0; i < splits; i++) {
                nlohmann::json jsonData;
                
                for (int j = 0; j < group_size; j++) {
                    jsonData["field"+std::to_string(i*group_size+j)] = parsedJson["field"+std::to_string(i*group_size+j)];
                }

                // any leftovers gets added to the last collection
                if (i == splits-1) {
                    for (int j = 0; j < int(parsedJson.size())-splits*group_size; j++) {
                        jsonData["field"+std::to_string(splits*group_size+j)] = parsedJson["field"+std::to_string(splits*group_size+j)];
                    }
                }
                
                std::string jsonStr = jsonData.dump();
                outputs.push_back(std::vector<uint8_t>(jsonStr.begin(), jsonStr.end()));
            }
            
            break;
        }
        default: {
            outputs.push_back(input);
            break;
        }
    }
}

TransformerType Distributor::Supports() const {
    return TransformerType::DISTRIBUTOR;
}

}