#include <iostream>
#include "mynooper.h"

namespace ROCKSDB_NAMESPACE {

std::vector<ArrowRecord> Mynooper::Transform(
      std::string_view key,
      const ArrowRecord& input) const
{
    (void)key;
    return {input};
}

}