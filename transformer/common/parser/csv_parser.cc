#include "csv_parser.h"


namespace ROCKSDB_NAMESPACE {

InputOutputDataType CsvParser::InputType() const {
  return InputOutputDataType::CSV;
}

bool CsvParser::Validate(const ByteBuffer& input_data) const {
  std::vector<std::string> tmp;
  return ParseLine(reinterpret_cast<const char*>(input_data.data()), input_data.size(), &tmp);
}

std::unique_ptr<ParsedObject> CsvParser::Parse(const ByteBuffer& data) const {
  auto payload = std::make_unique<CsvRowPayload>();
  if (!ParseLine(reinterpret_cast<const char*>(data.data()), data.size(), &payload->fields)) {
    return nullptr;
  }

  auto out = std::make_unique<ParsedObject>();
  out->payload = ParsedPayload::Make<CsvRowPayload>(InputOutputDataType::CSV, std::move(payload));
  return out;
}

bool CsvParser::ParseLine(const char* s, size_t n, std::vector<std::string>* out_fields) {
  out_fields->clear();
  std::string cur;
  bool in_quotes = false;

  for (size_t i = 0; i < n; ++i) {
    char c = s[i];

    if (in_quotes) {
      if (c == '"') {
        // escaped quote?
        if (i + 1 < n && s[i + 1] == '"') {
          cur.push_back('"');
          ++i;
        } else {
          in_quotes = false;
        }
      } else {
        cur.push_back(c);
      }
    } else {
      if (c == '"') {
        in_quotes = true;
      } else if (c == ',') {
        out_fields->push_back(std::move(cur));
        cur.clear();
      } else if (c == '\r' || c == '\n') {
        // stop at end-of-line
        break;
      } else {
        cur.push_back(c);
      }
    }
  }

  if (in_quotes) {
    // unterminated quote
    return false;
  }

  out_fields->push_back(std::move(cur));
  return true;
}

}