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
    ARROW,
    FLATBUFFERS
  };

  class ConverterData : public TransformerData {
    public:
      InputOutputDataType in_type;
      InputOutputDataType out_type;
      int column_data_type;
      ConverterData(InputOutputDataType intype, InputOutputDataType outtype,
                    int columndatatype) :
        in_type(intype), out_type(outtype), column_data_type(columndatatype) {}
  };

  class Converter : public Transformer {
    public:
      Converter() {};
      ~Converter() {};

      void Transform(std::string input,
                     std::vector<std::string>* outputs,
                     const std::shared_ptr<TransformerData>& data) override;
      void Prepare() override;
      void Retrieve(int position, std::vector<std::pair<std::string, std::string>>& output) override;
      size_t GetStoreSize() override;

    private:
      std::vector<std::map<std::string, std::vector<std::string>>> stores_;
    };
}