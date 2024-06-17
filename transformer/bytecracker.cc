#include "bytecracker.h"
#include "columns.pb.h"

namespace ROCKSDB_NAMESPACE {

void Bytecracker::Transform(std::string input, std::vector<std::string>* outputs, int splits, bool extra_write)
{
    if (splits <= 1) {
        outputs->push_back(input);
        return;
    }

    data::Row row;
    row.ParseFromString(input);
    int group_size = row.columns_size()/splits;
    if (group_size < 1) {
        group_size = 1;
        splits = row.columns_size();
    }
   
    for (int i = 0; i < splits; i++) {
        if (!extra_write) {
            data::Row splittedRow;
            for (int j = 0; j < group_size; j++) {
                data::Column* newColumn = splittedRow.add_columns();
                newColumn->set_name(row.columns(i*group_size+j).name());
                newColumn->set_value(row.columns(i*group_size+j).value());
            }

            if (i == splits - 1) {
                for (int j = 0; j < row.columns_size()-splits*group_size; j++) {
                    data::Column* newColumn = splittedRow.add_columns();
                    newColumn->set_name(row.columns(splits*group_size+j).name());
                    newColumn->set_value(row.columns(splits*group_size+j).value());
                }
            }

            std::string serializedRow;
            splittedRow.SerializeToString(&serializedRow);
            outputs->push_back(serializedRow);
        } else {
            flatbuffers::FlatBufferBuilder builder;
            
            // Add the columns as uint64s
            std::vector<flatbuffers::Offset<NumericColumn>> numericCols;
            for (int j = 0; j < group_size; j++) {
                try {
                    auto col_name = builder.CreateString(row.columns(i*group_size+j).name());
                    auto col = CreateNumericColumn(builder, col_name, std::stoull(row.columns(i*group_size+j).value()));
                    numericCols.push_back(col);
                } catch (const std::invalid_argument& ia) {
                    outputs->push_back(input);
                    return;
                } catch (const std::out_of_range& orr) {
                    outputs->push_back(input);
                    return;
                }
            }
            auto numericVec = builder.CreateVector(numericCols);
            auto fbRow = CreateFbRow(builder, numericVec);

            builder.Finish(fbRow);

            uint8_t *buf = builder.GetBufferPointer();
            int size = builder.GetSize();
            std::string s(reinterpret_cast<char*>(buf), size);

            outputs->push_back(s);
        }
    }

    return;
}

void Bytecracker::Store(std::vector<VersionEdit*> edits, std::vector<ColumnFamilyData*> cfds, std::vector<std::string>* outputs) {
    return;
}

void Bytecracker::Delete(VersionEdit* edit, std::vector<CompactionInputFiles*>) {
    return;
}

}