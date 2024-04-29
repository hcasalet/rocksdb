#include "bytecoder.h"
#include "columns.pb.h"

namespace ROCKSDB_NAMESPACE {

void Bytecoder::Transform(std::string input, std::vector<std::string>* outputs, int splits, bool extra_write)
{
    data::Row row;
    row.ParseFromString(input);
    int half = row.columns_size()/2;
    flatbuffers::FlatBufferBuilder builder;

    // Add the first half of the columns at uint64s
    std::vector<uint64_t> numericCols;
    for (int i = 0; i < half; ++i) {
        uint64_t num = std::stoull(row.columns(i).value());
        numericCols.push_back(num);
    }

    // Add the remaining strings to another vector
    std::vector<flatbuffers::Offset<flatbuffers::String>> stringCols;
    for (int i = half; i < row.columns_size(); ++i) {
        auto str = builder.CreateString(row.columns(i).value());
        stringCols.push_back(str);
    }

    // Add vectors to the builder
    auto numericVec = builder.CreateVector(numericCols);
    auto stringVec = builder.CreateVector(stringCols);

    // Create the FbRow object
    auto fbRow = CreateFbRow(builder, numericVec, stringVec);

    builder.Finish(fbRow);

    uint8_t *buf = builder.GetBufferPointer();
    int size = builder.GetSize();
    std::string s(reinterpret_cast<char*>(buf), size);

    outputs->push_back(s);
    return;
}

void Bytecoder::Store(std::vector<VersionEdit*> edits, std::vector<ColumnFamilyData*> cfds, std::vector<std::string>* outputs) {
    return;
}

void Bytecoder::Delete(VersionEdit* edit, std::vector<CompactionInputFiles*>) {
    return;
}

}