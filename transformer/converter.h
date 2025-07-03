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

  class ConverterData : public TransformerData {
    public:
      InputOutputDataType in_type;
      InputOutputDataType out_type;
      std::string column_data_type;
      ConverterData(InputOutputDataType intype, InputOutputDataType outtype,
                    std::string columndatatype) :
        in_type(intype), out_type(outtype), column_data_type(columndatatype) {}
  };

  class Converter : public Transformer {
    public:
      Converter() {};
      ~Converter() {};

      void Transform(const std::vector<uint8_t>& input,
                     std::vector<std::vector<uint8_t>>& outputs,
                     const std::shared_ptr<TransformerData>& data) const override;
    
      TransformerType Supports() const override;
    private:
      std::vector<std::map<std::string, std::vector<std::string>>> stores_;
    };
}