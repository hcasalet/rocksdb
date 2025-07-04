#include <memory>
#include <mutex>
#include <functional>
#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

class AugmenterSchema : public SchemaDescriptor {
  public:
    std::string row_key;
    InputOutputDataType input_type;
    AugmenterSchema(std::string rowKey, InputOutputDataType inType) :
              row_key(rowKey), input_type(inType) {}

    std::shared_ptr<void> Parse(const ByteBuffer& data) const override;

    ByteBuffer Serialize(const std::shared_ptr<void>& obj) const override;
};

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