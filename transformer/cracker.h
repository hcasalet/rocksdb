#include "transformer.h"

namespace ROCKSDB_NAMESPACE {

class Cracker : public Transformer
{
  public:
    Cracker() {};
  private:
    void Transform(InternalIterator* input_iter, std::vector<VectorIterator*> output_iters, int splits) override;
};

}