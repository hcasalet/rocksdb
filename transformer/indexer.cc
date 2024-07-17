#include "indexer.h"

namespace ROCKSDB_NAMESPACE {
    void Indexer::Transform(std::string input, std::vector<std::string>* outputs, int splits, bool extra_write) {

    }

    void Indexer::Store(std::vector<VersionEdit*> edits, std::vector<ColumnFamilyData*> cfds, std::vector<std::string>* outputs) {
        return;
    }

    void Indexer::Delete(VersionEdit* edit, std::vector<CompactionInputFiles*>) {

    }

}