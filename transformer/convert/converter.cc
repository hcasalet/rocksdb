#include <utility>
#include <vector>

#include "converter.h"

namespace ROCKSDB_NAMESPACE {

std::vector<ArrowRecord> Converter::Transform(
    const Slice& key,
    const ArrowRecord& input) const {
  (void)key;
  return {input};
}

}
