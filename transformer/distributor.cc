#include <iostream>
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
            const uint8_t* buf = reinterpret_cast<const uint8_t*>(input.c_str());

            flatbuffers::Verifier verifier(buf, input.size());
            if (!VerifyFbRowBuffer(verifier)) {
                std::cerr << "Invalid FlatBuffer detected, possible data corruption: " << std::endl;
                return;  // Early exit on corruption
            }
            const FbRow* fbRow = GetFbRow(buf);
            auto numcols = fbRow->numcols();

            int total_size = numcols->size();
            int remainder = total_size % splits;  // Handle remainder columns
            int group_size = total_size / splits;

            for (int i = 0; i < splits; i++) {
                flatbuffers::FlatBufferBuilder builder;
                std::vector<flatbuffers::Offset<NumericColumn>> numericCols;

                int current_group_size = group_size;
                if (i < remainder) {
                    current_group_size++;  // Distribute the remainder
                }

                for (int j = 0; j < current_group_size; j++) {
                    const NumericColumn* num_col = numcols->Get(i * group_size + std::min(i, remainder) + j);
                    try {
                        auto col_name = builder.CreateString(num_col->name());
                        auto col = CreateNumericColumn(builder, col_name, num_col->value());
                        numericCols.push_back(col);
                    } catch (const std::invalid_argument& ia) {
                        std::cerr << "Invalid argument in column: " << ia.what() << std::endl;
                    } catch (const std::out_of_range& orr) {
                        std::cerr << "Out of range error in column: " << orr.what() << std::endl;
                    }
                }

                auto numericVecPiece = builder.CreateVector(numericCols);
                auto fbRowPiece = CreateFbRow(builder, numericVecPiece);
                builder.Finish(fbRowPiece);
                uint8_t *bufPiece = builder.GetBufferPointer();
                int bufPieceSize = builder.GetSize();
                std::string s(reinterpret_cast<char*>(bufPiece), bufPieceSize);
                outputs->push_back(s);

                flatbuffers::Verifier split_verifier(reinterpret_cast<const uint8_t*>(s.c_str()), s.size());
                if (!VerifyFbRowBuffer(split_verifier)) {
                    std::cerr << "Verification failed for split FbRow at index: " << i << std::endl;
                }
            }
            /*const uint8_t* buf = reinterpret_cast<const uint8_t*>(input.c_str());

            flatbuffers::Verifier verifier(buf, input.size());
            if (!VerifyFbRowBuffer(verifier)) {
                std::cerr << "Invalid FlatBuffer detected, possible data corruption: " << std::endl;
                return;  // Early exit on corruption
            } else {
                std::cout << "Yay data is legit!!!!!!!" << std::endl;
            }

            const FbRow* fbRow = GetFbRow(buf);
            auto numcols = fbRow->numcols();

            int group_size = numcols->size()/splits;
            if (group_size < 1) {
                group_size = 1;
                splits = numcols->size();
            }

            for (int i = 0; i < splits; i++) {
                flatbuffers::FlatBufferBuilder builder;
                std::vector<flatbuffers::Offset<NumericColumn>> numericCols;

                for (int j = 0; j < group_size; j++) {
                    const NumericColumn* num_col = numcols->Get(i*group_size+j);
                    try {
                        auto col_name = builder.CreateString(num_col->name());
                        auto col = CreateNumericColumn(builder, col_name, num_col->value());
                        numericCols.push_back(col);
                    } catch (const std::invalid_argument& ia) {
                    } catch (const std::out_of_range& orr) {
                    }
                }
                
                for (size_t j = 0; j < numcols->size() - group_size*splits; j++) {
                    const NumericColumn* num_col = numcols->Get(splits*group_size+j);
                    try {
                        auto col_name = builder.CreateString(num_col->name());
                        auto col = CreateNumericColumn(builder, col_name, num_col->value());
                        numericCols.push_back(col);
                    } catch (const std::invalid_argument& ia) {
                    } catch (const std::out_of_range& orr) {
                    }
                }
                
                auto numericVecPiece = builder.CreateVector(numericCols);
                auto fbRowPiece = CreateFbRow(builder, numericVecPiece);
                builder.Finish(fbRowPiece);
                uint8_t *bufPiece = builder.GetBufferPointer();
                int bufPieceSize = builder.GetSize();
                std::string s(reinterpret_cast<char*>(bufPiece), bufPieceSize);
                outputs->push_back(s);
            }*/

            break;
        }
        case DistributorValueType::JSON: {
            /** to be implemented */
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