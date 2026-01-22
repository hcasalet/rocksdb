#include <iostream>
#include "mynooper.h"

namespace ROCKSDB_NAMESPACE {

std::vector<ArrowRecord> Mynooper::Transform(
      const Slice& key,
      const ArrowRecord& input) const
{
    (void)key;
    return {input};
}

}