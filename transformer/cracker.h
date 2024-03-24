#include "transformer.h"

namespace ROCKSDB_NAMESPACE {

class Cracker : public Transformer
{
  public:
    Cracker() {};
  private:
    void Transform(std::string input, std::vector<std::string>* outputs, int splits, bool extra_write) override;
};

}