#include <memory>
#include "flatbuffers/flatbuffers.h"
#include "rocksdb/transformer.h"
#include "flat/data_generated.h"

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
      ConverterInputType in_type;
      ConverterOutputType out_type;
      ConverterData(ConverterInputType intype, ConverterOutputType outtype) :
        in_type(intype), out_type(outtype) {}
  };

  class Converter : public Transformer {
    public:
      Converter() {};
      ~Converter() {};

      void Transform(std::string input,
                     std::vector<std::string>* outputs,
                     const std::shared_ptr<TransformerData>& data) override;
      void Prepare() override;
      void Retrieve(int position, std::map<std::string, std::string> output) override;
      size_t GetStoreSize() override;

    private:
      std::vector<std::map<std::string, std::vector<std::string>>> stores_;
    };
}