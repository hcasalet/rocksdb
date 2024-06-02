#include "bytecoder.h"
#include "columns.pb.h"

namespace ROCKSDB_NAMESPACE {

void Bytecoder::Transform(std::string input, std::vector<std::string>* outputs, int splits, bool extra_write)
{
    // cracking and flat buffers transformation only both occurs for level 0 -> level 1
    data::Row row;
    row.ParseFromString(input);
    int len = row.columns_size();
    flatbuffers::FlatBufferBuilder builder;

    // Add the columns as uint64s
    std::vector<uint64_t> numericCols;
    for (int i = 0; i < len; ++i) {
        try {
            uint64_t num = std::stoull(row.columns(i).value());
            numericCols.push_back(num);
        } catch (const std::invalid_argument& ia) {
            outputs->push_back(input);
            return;
        } catch (const std::out_of_range& orr) {
            outputs->push_back(input);
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
    return;
}

void Bytecoder::Store(std::vector<VersionEdit*> edits, std::vector<ColumnFamilyData*> cfds, std::vector<std::string>* outputs) {
    return;
}

void Bytecoder::Delete(VersionEdit* edit, std::vector<CompactionInputFiles*>) {
    return;
}

}
