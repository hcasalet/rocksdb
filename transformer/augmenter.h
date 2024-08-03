#include <memory>
#include <functional>
#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

class AugmenterData : public TransformerData {
  public:
    std::string row_key;
    AugmenterData(std::string rowKey) : row_key(rowKey) {}
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
    Augmenter(std::vector<DeriveFuncData*>& derivers)
        : derivers_(derivers) {
          for (size_t i = 0; i < derivers.size(); i++) {
            std::map<std::string, std::vector<std::string>> st;
            stores_.push_back(st);
          }
        };
    ~Augmenter() {};

    void Transform(std::string input,
                   std::vector<std::string>* outputs,
                   const std::shared_ptr<TransformerData>& data) override;
    void Prepare() override;
    void Retrieve(int position, std::map<std::string, std::string> output) override;

  private:
    std::vector<DeriveFuncData*> derivers_;
    std::vector<std::map<std::string, std::vector<std::string>>> stores_;
};

}