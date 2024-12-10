#include <memory>
#include <mutex>
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
        : derivers_(derivers) {};
    ~Augmenter() {};

    void Transform(std::string input,
                   std::vector<std::string>* outputs,
                   const std::shared_ptr<TransformerData>& data,
                   uint64_t job_id) override;
    void Prepare(uint64_t job_id) override;
    void Retrieve(uint64_t job_id, std::vector<std::pair<std::string, std::string>>& output) override;
    size_t GetStoreSize(uint64_t job_id) override;

  private:
    std::vector<DeriveFuncData*> derivers_;
    std::unordered_map<int, std::map<std::string, std::vector<std::string>>> stores_;
    std::mutex stores_mutex_; // Mutex to protect access to store_
};

}