#pragma once

#include "rocksdb/transformer.h"

#include <string>
#include <vector>

namespace ROCKSDB_NAMESPACE {
struct CsvRowPayload {
  std::vector<std::string> fields;
};

// Simple CSV parser: comma-separated, supports quoted fields ("...") and escaped quotes ("").
// Does not do multiline fields.
class CsvParser final : public Parser {
 public:
  InputOutputDataType InputType() const override;
  bool Validate(const ByteBuffer& input_data) const override;
  std::unique_ptr<ParsedObject> Parse(const ByteBuffer& data) const override;

 private:
  static bool ParseLine(const char* s, size_t n, std::vector<std::string>* out_fields);
};

}