#include "flatbuffers/flatbuffers.h"
#include "transformer.h"
#include "flat/data_generated.h"

namespace ROCKSDB_NAMESPACE {
    Class Bytecoder : public Transformer {
        public:
          Bytecoder() {};  
        private:
          void Transform(std::string input, std::vector<std::string>* outputs, int splits, bool extra_write) override;
          void Store(std::vector<VersionEdit*> edits, std::vector<ColumnFamilyData*> cfds, std::vector<std::string>* outputs) override;
          void Delete(VersionEdit* edit, std::vector<CompactionInputFiles*>) override;
    };
}