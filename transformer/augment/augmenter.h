#include <memory>
#include <mutex>
#include <functional>
#include "rocksdb/transformer.h"
#include "data.pb.h"
#include "json_index_schema.h"
#include "protobuf_index_schema.h"

namespace ROCKSDB_NAMESPACE {

class DeriveFuncData {
  public:
    std::vector<int> positions;
    std::function<std::string(std::vector<std::string>&)> func;
    bool is_index;

    DeriveFuncData(std::vector<int> ps, std::function<std::string(std::vector<std::string>&)> f) 
        : positions(ps), func(f) {}

    std::string derive(std::vector<std::string>& columnVals) {
      if (func) {
        return func(columnVals);
      }
      return "";
    }
};

class Augmenter : public Transformer
{
  public:
    Augmenter() {};
    ~Augmenter() {};

    std::string Name() const override { return "Augmenter"; }

    void Transform(const std::vector<uint8_t>& input,
                   std::vector<std::vector<uint8_t>>& outputs,
                   const std::shared_ptr<SchemaDescriptor>& data) const override;
  
    TransformerType Supports() const override { return TransformerType::AUGMENTER; }
};

}