#pragma once

#include "rocksdb/transformer.h"
#include "../parser/csv_parser.h"  // CsvRowPayload

namespace ROCKSDB_NAMESPACE {

class CsvEncoder final : public Encoder {
 public:
  InputOutputDataType OutputType() const override;
  ByteBuffer SerializeFromArrow(const ArrowRecord& rec) const override;

 private:
  static void AppendField(std::string* out, const std::string& f);
  std::string ScalarToStringForCsv(const arrow::Scalar& s) const;
};

}