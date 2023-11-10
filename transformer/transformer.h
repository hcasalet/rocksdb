#pragma once

#include <string>
#include <vector>

#include "db/compaction/compaction_iterator.h"

namespace ROCKSDB_NAMESPACE {

class Transformer {
 public:
  virtual ~Transformer() {}

  // non-virtual interface
  virtual void Transform(CompactionIterator* input_iter, std::vector<CompactionIterator*> output_iters, int splits) = 0;
};

// Create a new Transformer that can be shared among multiple RocksDB instances
extern Transformer* NewTransformer(
    const std::string& conversion_type = "");

}
