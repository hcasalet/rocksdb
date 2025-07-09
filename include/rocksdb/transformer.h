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
  MYNOOPER             = 1 << 3,       // 8
};

enum class InputOutputDataType {
  UNKNOWN          = 0,
  JSON             = 1 << 0,
  PROTOBUF         = 1 << 1,
  FLATBUFFERS      = 1 << 2,
  AVRO             = 1 << 3,
  PARQUET          = 1 << 4,
  CSV              = 1 << 5,
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

using ByteBuffer = std::vector<uint8_t>;

struct FieldSchema {
  std::string name;
  std::string type;  // Could use enum for strict typing if preferred
  int field_number;
};

// A schema descriptor defines how to interpret or transform input data.
class SchemaDescriptor {
  public:
   virtual ~SchemaDescriptor() = default;

   // Shows the data format before and after the transformation
   /*virtual InputOutputDataType InputType() const = 0;
   virtual InputOutputDataType OutputType() const = 0;
   virtual bool Validate(const ByteBuffer& input_data) const = 0;*/
   virtual InputOutputDataType InputType() const { return InputOutputDataType::UNKNOWN; } // temporary
   virtual InputOutputDataType OutputType() const { return InputOutputDataType::UNKNOWN; } // temporary
   virtual bool Validate(const ByteBuffer& input_data) const { return true; } // temporary


   virtual std::shared_ptr<void> Parse(const ByteBuffer& data) const = 0;
   virtual ByteBuffer Serialize(const std::shared_ptr<void>& obj) const = 0;

   /*virtual std::vector<FieldSchema> GetInputFieldSchema() const = 0;
   virtual std::vector<std::vector<FieldSchema>> GetOutputFieldSchemas() const = 0;*/
   virtual std::vector<FieldSchema> GetInputFieldSchema() const { return {}; } // temporary
   virtual std::vector<std::vector<FieldSchema>> GetOutputFieldSchemas() const { return {};} // temporary

 };

class Transformer {
 public:
  virtual ~Transformer() = default;

  // Returns transformer name
  virtual std::string Name() const = 0;

  // Transforms a single input record into one or more outputs.
  virtual void Transform(const ByteBuffer& input,
                         std::vector<ByteBuffer>& outputs,
                         const std::shared_ptr<SchemaDescriptor>& schema) const = 0;
  
  // Declares which transformation features this transformer supports
  virtual TransformerType Supports() const = 0;
};

// Create a new Transformer that can be shared among multiple RocksDB instances
// Returns nullptr for TransformerType: NOTRANSFORMATION.
[[nodiscard]] std::shared_ptr<Transformer> CreateTransformer(
    const TransformerType transformer_type = TransformerType::NOTRANSFORMATION);

}