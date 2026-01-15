#include "csv_encoder.h"

#include <string>

namespace ROCKSDB_NAMESPACE {

InputOutputDataType CsvEncoder::OutputType() const {
  return InputOutputDataType::CSV;
}

ByteBuffer CsvEncoder::Serialize(const ParsedObject& obj) const {
  if (obj.payload.format != InputOutputDataType::CSV) {
    return {};
  }
  auto* row = obj.payload.As<CsvRowPayload>();
  if (!row) return {};

  std::string line;
  for (size_t i = 0; i < row->fields.size(); ++i) {
    if (i) line.push_back(',');
    AppendField(&line, row->fields[i]);
  }
  line.push_back('\n');

  return ByteBuffer(line.begin(), line.end());
}

void CsvEncoder::AppendField(std::string* out, const std::string& f) {
  bool needs_quotes = false;
  for (char c : f) {
    if (c == '"' || c == ',' || c == '\n' || c == '\r') {
      needs_quotes = true;
      break;
    }
  }

  if (!needs_quotes) {
    out->append(f);
    return;
  }

  out->push_back('"');
  for (char c : f) {
    if (c == '"') {
      out->push_back('"');
      out->push_back('"');
    } else {
      out->push_back(c);
    }
  }
  out->push_back('"');
}

}