#pragma once

#include <string>
#include <vector>

#include "table/internal_iterator.h"
#include "util/vector_iterator.h"

namespace ROCKSDB_NAMESPACE {

class Transformer {
 public:
  virtual ~Transformer() {}

  // non-virtual interface
  virtual void Transform(InternalIterator* input_iter, std::vector<VectorIterator*>* output_iters, int splits) = 0;
  virtual void Transform(std::string input, std::vector<std::string>* outputs, int splits, bool extra_write) = 0;
};

// Create a new Transformer that can be shared among multiple RocksDB instances
extern Transformer* NewTransformer(
    const std::string& conversion_type = "");

}
