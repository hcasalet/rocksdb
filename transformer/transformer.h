#pragma once

#include <string>
#include <vector>
#include <variant>

#include "table/internal_iterator.h"
#include "util/vector_iterator.h"
#include "db/version_edit.h"
#include "db/compaction/compaction.h"

namespace ROCKSDB_NAMESPACE {

struct IndexerData {
  std::vector<std::vector<int>> indexingPositions;
};

struct CrackerData {
  int splits;
};

using TransformerData = std::variant<IndexerData, CrackerData>;

class Transformer {
 public:
  virtual ~Transformer() {}

  // non-virtual interface
  virtual void Transform(std::string input, std::vector<std::string>* outputs, const TransformerData& data) = 0;
  virtual void Store(std::vector<VersionEdit*> edits, std::vector<ColumnFamilyData*> cfds, std::vector<std::string>* outputs) = 0;
  virtual void Delete(VersionEdit* edit, std::vector<CompactionInputFiles*>) = 0;
};

// Create a new Transformer that can be shared among multiple RocksDB instances
extern Transformer* NewTransformer(
    const std::string& conversion_type = "");

}
