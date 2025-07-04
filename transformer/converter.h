#include <memory>
#include "flatbuffers/flatbuffers.h"
#include "rocksdb/transformer.h"
#include "flat/row_generated.h"
#include "flat/row_num_generated.h"
#include "flat/row_str_generated.h"

namespace ROCKSDB_NAMESPACE {

  enum class ConverterInputType {
    JSON,
    PROTOBUF
  };

  enum class ConverterOutputType {
    FLATBUFFERS
  };

  class ConverterSchema : public SchemaDescriptor {
    public:
      InputOutputDataType in_type;
      InputOutputDataType out_type;
      std::string column_data_type;
      ConverterSchema(InputOutputDataType intype, InputOutputDataType outtype,
                    std::string columndatatype) :
        in_type(intype), out_type(outtype), column_data_type(columndatatype) {}

      std::shared_ptr<void> Parse(const ByteBuffer& data) const override;

      ByteBuffer Serialize(const std::shared_ptr<void>& obj) const override;
  };

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