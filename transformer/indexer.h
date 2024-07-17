#include "transformer.h"

namespace ROCKSDB_NAMESPACE {

class Indexer : public Transformer
{
  public:
    Indexer() {};
  private:
    void Transform(std::string input, std::vector<std::string>* outputs, int splits, bool extra_write) override;
    void Store(std::vector<VersionEdit*> edits, std::vector<ColumnFamilyData*> cfds, std::vector<std::string>* outputs) override;
    void Delete(VersionEdit* edit, std::vector<CompactionInputFiles*>) override;
};

}