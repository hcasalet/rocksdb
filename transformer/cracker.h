#include "transformer.h"

namespace ROCKSDB_NAMESPACE {

class Cracker : public Transformer
{
  public:
    Cracker() {};
  private:
    void Transform(CompactionIterator* input_iter, std::vector<CompactionIterator*> output_iters, int splits) override;
};

}