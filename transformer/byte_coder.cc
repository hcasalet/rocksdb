#include "byte_coder.h"

void Bytecoder::Transform(std::string input, std::vector<std::string>* outputs, int splits, bool extra_write)
{
    FlatBuffers.Builder builder;
}

void Bytecoder::Store(std::vector<VersionEdit*> edits, std::vector<ColumnFamilyData*> cfds, std::vector<std::string>* outputs) {
    return;
}

void Bytecoder::Delete(VersionEdit* edit, std::vector<CompactionInputFiles*>) {

}