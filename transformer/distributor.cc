#include <iostream>
#include <nlohmann/json.hpp>
#include "distributor.h"
#include "columns.pb.h"

namespace ROCKSDB_NAMESPACE {

void Distributor::Transform(std::string input, std::vector<std::string>* outputs, 
                const std::shared_ptr<TransformerData>& data)
{
    auto distributorData = std::dynamic_pointer_cast<DistributorData>(data);
    int splits = distributorData->splits;
    DistributorValueType vtype = distributorData->vtype;

    switch (vtype) {
        case DistributorValueType::PROTOBUF: {
            data::Row row;
            row.ParseFromString(input);
            int group_size = row.columns_size()/splits;
            if (group_size < 1) {
                group_size = 1;
                splits = row.columns_size();
            }

            for (int i = 0; i < splits; i++) {
                data::Row splittedRow;
              
                for (int j = 0; j < group_size; j++) {
                    data::Column* newColumn = splittedRow.add_columns();
                    newColumn->set_name(row.columns(i*group_size+j).name());
                    newColumn->set_value(row.columns(i*group_size+j).value());
                }

                // any leftovers gets added to the last collection
                for (int j = 0; j < row.columns_size()-splits*group_size; j++) {
                    data::Column* newColumn = splittedRow.add_columns();
                    newColumn->set_name(row.columns(splits*group_size+j).name());
                    newColumn->set_value(row.columns(splits*group_size+j).value());
                }
             
                std::string serializedRow;
                splittedRow.SerializeToString(&serializedRow);
                outputs->push_back(serializedRow);
            }

            break;
        }
        case DistributorValueType::FLATBUFFERS: {
            /**
             * not implemented
             */
            
            break;
        }
        case DistributorValueType::JSON: {
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
                for (int j = 0; j < int(parsedJson.size())-splits*group_size; j++) {
                    jsonData["field"+std::to_string(splits*group_size+j)] = parsedJson["field"+std::to_string(splits*group_size+j)];
                }
                
                outputs->push_back(jsonData.dump());
            }
            
            break;
        }
    }
}

void Distributor::Prepare() {
    for (auto store : stores_) {
        store.clear();
    }
}

void Distributor::Retrieve(int position, std::map<std::string, std::string> output) {
    return;
}

size_t Distributor::GetStoreSize() {
    return stores_.size();
}

}