#pragma once

#include <string>
#include <vector>
#include <map>
#include <type_traits>

#include "rocksdb/rocksdb_namespace.h"

namespace ROCKSDB_NAMESPACE {

/*
 * There are basically 3 types of tranformations during compaction: 
 * 1 -- Splitting data from row-wise to column-wise. This results in the split data
 *      getting written into level 0 of different column families, and the 
 *      compacting column faily deleted. This is the distributor type.
 * 2 -- Converting data from one format to another, for instance, from json format
 *      to flat buffers. There is no column family change. The converted data is
 *      written into the compacting column family on a lower level. This is the 
 *      converter type.
 * 3 -- Deriving new data such as creating an index on the orignal data. This results
 *      in the derived data getting written into the augmented column families, and 
 *      the original data into the compacting column family. This is the augmenter type.
 * 
 * In defining TransformerType enum, we assign value 1 to the distributor, 4 to the converter, 
 * and 7 to the augmenter, so that algebraic operations on any of those type values have the 
 * unique result. This allows us to support algebraic operations on the transformer types. 
 * We have the following values for transformation types:
 * 
 *   1  -- distributor only
 *   4  -- converter only
 *   5  -- distributor+converter
 *   7  -- augmenter only
 *   8  -- distributor+augmenter
 *   11 -- converter+augmenter
 *   12 -- distributor+converter+augmenter
 * 
*/

enum class TransformerType {
  NOTRANSFORMATION = 0,
  DISTRIBUTOR = 1 << 0,     // 1
  CONVERTER   = 1 << 1,     // 2
  AUGMENTER   = 1 << 2      // 4
};

constexpr TransformerType operator|(TransformerType lhs, TransformerType rhs) {
    using T = std::underlying_type_t<TransformerType>;
    return static_cast<TransformerType>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

constexpr int to_underlying(TransformerType type) {
    return static_cast<std::underlying_type_t<TransformerType>>(type);
}

class TransformerData {
  public:
    virtual ~TransformerData() = default;
};

class Transformer {
 public:
  virtual ~Transformer() {}

  // non-virtual interface
  virtual void Transform(std::string input,
                         std::vector<std::string>* outputs,
                         const std::shared_ptr<TransformerData>& data) = 0;
  virtual void Prepare() = 0;
  virtual void Retrieve(int position, std::map<std::string, std::string> output) = 0;
  virtual size_t GetStoreSize() = 0;
};

// Create a new Transformer that can be shared among multiple RocksDB instances
extern Transformer* NewTransformer(
    const std::string& conversion_type = "");

}
