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
        flatbuffers::FlatBufferBuilder builder;
        std::vector<uint64_t> numericCols;
        int half = (group_size+1)/2;
        for (int j = 0; j < group_size; j++) {
            uint64_t num = std::stoull(row.columns(i*group_size+j).value());
            numericCols.push_back(num);
        }
        auto numericVec = builder.CreateVector(numericCols);

        auto fbRow = CreateFbRow(builder, numericVec);

        builder.Finish(fbRow);

        uint8_t *buf = builder.GetBufferPointer();
        int size = builder.GetSize();
        std::string s(reinterpret_cast<char*>(buf), size);

        outputs->push_back(s);
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