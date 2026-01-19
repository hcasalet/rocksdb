#include <cctype>
#include <string>
#include <vector>
#include <utility>
#include <mutex>
#include <optional>

#include <rocksdb/slice.h>

struct TransformConfig {
  int splits;
};

class Transformer {
  public:
    Transformer(TransformConfig& tcfg) : tcfg_(tcfg) {}
    void SplitRecords(const TransformConfig& cfg, const rocksdb::Slice& key,
                    const rocksdb::Slice& value, std::vector<std::pair<std::string, std::string>>* outs);
    std::optional<std::pair<std::string, std::string>> TransformOne(const rocksdb::Slice& key, 
                    const rocksdb::Slice& value);
    void TransformOneMulti(const rocksdb::Slice& key, const rocksdb::Slice& value, 
                    std::vector<std::pair<std::string, std::string>>* outs);
  
  private:
    TransformConfig& tcfg_;
    static inline void SkipWs(const char*& p, const char* e) {
        while (p < e && std::isspace(static_cast<unsigned char>(*p))) ++p;
    }
    bool ParseSimpleJsonObject(const rocksdb::Slice& value, std::vector<std::pair<std::string, std::string>>* kvs);
    int ColumnIndexFromName(const std::string& name);
    void AppendJsonField(std::string* out, const std::string& k, const std::string& v, unsigned char& first);
};