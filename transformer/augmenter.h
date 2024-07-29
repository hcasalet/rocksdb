#include <memory>
#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

enum class AugmenterDeriveType {
  INDEX,
  MATERIALIZED_VIEW
};

class AugmenterData : public TransformerData {
  public:
    AugmenterDeriveType derive_type;
    std::vector<std::vector<int>> derivingPositions;
    AugmenterData(AugmenterDeriveType deriveType, std::vector<std::vector<int>> derivingInfo)
            : derive_type(deriveType),
              derivingPositions(derivingInfo) {}
};

class Augmenter : public Transformer
{
  public:
    Augmenter() {};
    ~Augmenter() {};
  private:
    void Transform(std::string input,
                   std::vector<std::string>* outputs,
                   const std::shared_ptr<TransformerData>& data) override;
};

}