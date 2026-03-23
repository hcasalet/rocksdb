#include <utility>
#include <vector>

#include "converter.h"

namespace ROCKSDB_NAMESPACE {

std::vector<ArrowRecord> Converter::Transform(
    std::string_view key,
    const ArrowRecord& input) const {
  (void)key;
  return {input};
}

}
