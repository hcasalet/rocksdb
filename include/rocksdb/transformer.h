#pragma once

#include <string>
#include <vector>
#include <map>
#include <type_traits>
#include <memory>

#include "rocksdb/rocksdb_namespace.h"

namespace ROCKSDB_NAMESPACE {

/*
 *
 * TransformerType is a bitmask enum used to specify transformation behavior during compaction:
 * - DISTRIBUTOR: splits a record into multiple outputs written to separate column families
 * - CONVERTER: converts record formats (e.g., JSON → FlatBuffers)
 * - AUGMENTER: derives new records (e.g., index entries) in addition to writing the original
 * 
 * These types may be combined using bitwise OR to express compound transformations.
 * 
*/

enum class TransformerType {
  NOTRANSFORMATION     = 0,
  DISTRIBUTOR          = 1 << 0,       // 1
  CONVERTER            = 1 << 1,       // 2
  AUGMENTER            = 1 << 2,       // 4
  DISTRIBUTORWRITEBOTH = 1 << 5,       // 32
  MYNOOPER             = 1 << 6,       // 64
};

enum class InputOutputDataType {
  UNKNOWN          = 0,
  JSON             = 1 << 0,
  PROTOBUF         = 1 << 1,
  FLATBUFFERS      = 1 << 2
};

constexpr TransformerType operator|(TransformerType lhs, TransformerType rhs) {
    using T = std::underlying_type_t<TransformerType>;
    return static_cast<TransformerType>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

constexpr TransformerType operator&(TransformerType lhs, TransformerType rhs) {
    using T = std::underlying_type_t<TransformerType>;
    return static_cast<TransformerType>(static_cast<T>(lhs) & static_cast<T>(rhs));
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
  virtual ~Transformer() = default;

  // Transforms a single input record into one or more outputs.
  virtual void Transform(const std::vector<uint8_t>& input,
                         std::vector<std::vector<uint8_t>>& outputs,
                         const std::shared_ptr<TransformerData>& data) const = 0;
  
  // Declares which transformation features this transformer supports
  virtual TransformerType Supports() const = 0;
};

// Create a new Transformer that can be shared among multiple RocksDB instances
extern std::shared_ptr<Transformer> NewTransformer(
    const TransformerType transformer_type = TransformerType::NOTRANSFORMATION);

}
