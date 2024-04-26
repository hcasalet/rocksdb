#include "bytecoder.h"

namespace ROCKSDB_NAMESPACE {

void Bytecoder::Transform(std::string input, std::vector<std::string>* outputs, int splits, bool extra_write)
{
    flatbuffers::FlatBufferBuilder builder(1024);

    int len = input.length();
    int cols = (len-1)/16 + 1;

    std::vector<flatbuffers::Offset<flatbuffers::String>> columns_vec;
    for (int i = 0; i < cols; i++) {
        auto col = builder.CreateString(input.substr(i*16, 16));
        columns_vec.push_back(col);
    }
    auto columns = builder.CreateVector(columns_vec);

    auto row = CreateRow(builder, columns);
    builder.Finish(row);

    //std::string s(reinterpret_cast<char*>(builder.GetBufferPointer(), builder.GetSize()));
    outputs->push_back(input);

    return;
}

void Bytecoder::Store(std::vector<VersionEdit*> edits, std::vector<ColumnFamilyData*> cfds, std::vector<std::string>* outputs) {
    return;
}

void Bytecoder::Delete(VersionEdit* edit, std::vector<CompactionInputFiles*>) {
    return;
}

}