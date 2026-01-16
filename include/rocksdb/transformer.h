#pragma once

#include <string>
#include <vector>
#include <map>
#include <type_traits>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <utility>

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
  FIXEDBIN64       = 1 << 6,
  COLUMNBYTES      = 1 << 7,
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

// Type-erased payload with a checked downcast.
struct ParsedPayload {
  InputOutputDataType format;     // e.g., JSON / PROTOBUF / FLATBUFFERS / ...
  std::type_index     type{typeid(void)};
  std::shared_ptr<void> ptr;

  ParsedPayload() : format(InputOutputDataType::UNKNOWN) {}

  template <class T>
  static ParsedPayload Make(InputOutputDataType fmt, std::unique_ptr<T> p) {
    ParsedPayload out;
    out.format = fmt;
    out.type   = std::type_index(typeid(T));
    // Move unique_ptr<T> into shared_ptr<void> with correct deleter.
    out.ptr = std::shared_ptr<T>(p.release());
    return out;
  }

  template <class T>
  T* As() const {
    if (type != std::type_index(typeid(T))) {
      return nullptr;
    }
    return static_cast<T*>(ptr.get());
  }

  explicit operator bool() const { return static_cast<bool>(ptr); }
};

struct ParsedObject {
  virtual ~ParsedObject() = default;
  ParsedPayload payload;
};

class Parser {
 public:
  virtual ~Parser() = default;

  // Which on-the-wire format this parser accepts.
  virtual InputOutputDataType InputType() const = 0;

  // Optional quick validation (cheap checks).
  virtual bool Validate(const ByteBuffer& input_data) const { return true; }

  // Parse bytes into an in-memory representation.
  virtual std::unique_ptr<ParsedObject> Parse(const ByteBuffer& data) const = 0;

  // Optional: expose schema of the parsed representation (if you have it).
  virtual const std::vector<FieldSchema>& GetInputFieldSchema() const {
    static const std::vector<FieldSchema> kEmpty;
    return kEmpty;
  }
};

class Encoder {
 public:
  virtual ~Encoder() = default;

  // Which on-the-wire format this encoder produces.
  virtual InputOutputDataType OutputType() const = 0;

  // Encode an in-memory representation into bytes.
  virtual ByteBuffer Serialize(const ParsedObject& obj) const = 0;

  // Optional: describe outputs (useful for distributor/augmenter cases).
  virtual const std::vector<std::vector<FieldSchema>>& GetOutputFieldSchemas() const {
    static const std::vector<std::vector<FieldSchema>> kEmpty;
    return kEmpty;
  }
};

struct Codec {
  std::shared_ptr<Parser> parser;    // may be null
  std::shared_ptr<Encoder> encoder;  // may be null

  bool HasParser() const { return static_cast<bool>(parser); }
  bool HasEncoder() const { return static_cast<bool>(encoder); }
};

// A schema descriptor defines how to interpret or transform input data.
class SchemaDescriptor {
  public:
   virtual ~SchemaDescriptor() = default;

   virtual TransformerType SupportsTransformerType() const = 0;

   // Shows the data format before and after the transformation
   virtual InputOutputDataType InputType() const {
    const auto& c = InputCodec();
    return c.parser ? c.parser->InputType() : InputOutputDataType::UNKNOWN;
   }
   virtual InputOutputDataType OutputType() const {
    const auto& c = OutputCodec();
    return c.encoder ? c.encoder->OutputType() : InputOutputDataType::UNKNOWN;
   }
   virtual bool Validate(const ByteBuffer& input_data) const {
    const auto& c = InputCodec();
    return c.parser ? c.parser->Validate(input_data) : true;
   } 

   virtual std::unique_ptr<ParsedObject> Parse(const ByteBuffer& data) const {
    const auto& c = InputCodec();
    if (!c.parser) return nullptr;
    if (!c.parser->Validate(data)) return nullptr;
    return c.parser->Parse(data);
   }
   virtual ByteBuffer Serialize(const ParsedObject& obj) const {
    const auto& c = OutputCodec();
    if (!c.encoder) return {};
    return c.encoder->Serialize(obj);
   }

   virtual const Codec& InputCodec() const {
    static const Codec kEmpty;
    return kEmpty;
   }
   virtual const Codec& OutputCodec() const {
    static const Codec kEmpty;
    return kEmpty;
   }

   virtual const std::vector<FieldSchema>& GetInputFieldSchema() const {
    static const std::vector<FieldSchema> kEmpty;
    return kEmpty;
   }
   virtual const std::vector<std::vector<FieldSchema>>& GetOutputFieldSchemas() const {
    static const std::vector<std::vector<FieldSchema>> kEmpty;
    return kEmpty;
   }
   virtual int GetNumSplits() const {return 0;}
   virtual std::vector<std::vector<std::string>> GetIndexKeys() const {return {}; }
   virtual std::vector<std::vector<int>> GetPositionedIndexKeys() const {return {}; }

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