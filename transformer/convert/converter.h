#include <memory>
#include "flatbuffers/flatbuffers.h"
#include "rocksdb/transformer.h"
#include "row_generated.h"
#include "data.pb.h"
#include "json2protobuf_schema.h"
#include "protobuf2flatbuffers_schema.h"
namespace ROCKSDB_NAMESPACE {

  class Converter : public Transformer {
    public:
      Converter() {};
      ~Converter() {};

      std::string Name() const override { return "Converter"; }

      void Transform(const std::vector<uint8_t>& input,
                     std::vector<std::vector<uint8_t>>& outputs,
                     const std::shared_ptr<SchemaDescriptor>& data) const override;
    
      TransformerType Supports() const override { return TransformerType::CONVERTER; }
    };
}