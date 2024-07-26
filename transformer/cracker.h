#include "transformer.h"

namespace ROCKSDB_NAMESPACE {

class Cracker : public Transformer
{
  public:
    Cracker() {};
  private:
    void Transform(std::string input, std::vector<std::string>* outputs, const TransformerData& data) override;
    void Store(std::vector<VersionEdit*> edits, std::vector<ColumnFamilyData*> cfds, std::vector<std::string>* outputs) override;
    void Delete(VersionEdit* edit, std::vector<CompactionInputFiles*>) override;
};

}