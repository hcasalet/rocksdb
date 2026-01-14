#include "csv_distributor_schema.h"
#include <sstream>
#include <stdexcept>

namespace ROCKSDB_NAMESPACE {

bool CsvDistributorSchema::Validate(const ByteBuffer& data) const {
  // Basic CSV format check: ensure data is not empty and has at least one comma
  std::string line(data.begin(), data.end());
  return !data.empty() && std::find(data.begin(), data.end(), ',') != data.end() &&
     static_cast<size_t>(std::count(line.begin(), line.end(), ',') + 1) == header_.size();
}

inline void TrimLineEnd(std::string& s) {
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
    s.pop_back();
  }
}

std::vector<std::string> ParseCsvRecord(std::string_view line) {
  std::vector<std::string> out;
  std::string cur;
  cur.reserve(line.size());

  bool in_quotes = false;

  for (size_t i = 0; i < line.size(); ++i) {
    char c = line[i];

    if (in_quotes) {
      if (c == '"') {
        // Either end quote or escaped quote.
        if (i + 1 < line.size() && line[i + 1] == '"') {
          cur.push_back('"');
          ++i;  // consume second quote
        } else {
          in_quotes = false;
        }
      } else {
        cur.push_back(c);
      }
    } else {
      if (c == ',') {
        out.emplace_back(std::move(cur));
        cur.clear();
      } else if (c == '"') {
        in_quotes = true;
      } else {
        cur.push_back(c);
      }
    }
  }

  out.emplace_back(std::move(cur));
  return out;
}

// Escape a field for CSV output if needed.
std::string EscapeCsvField(std::string_view field) {
  bool needs_quotes = false;
  for (char c : field) {
    if (c == ',' || c == '"' || c == '\n' || c == '\r') {
      needs_quotes = true;
      break;
    }
  }
  if (!needs_quotes) return std::string(field);

  std::string out;
  out.reserve(field.size() + 2);
  out.push_back('"');
  for (char c : field) {
    if (c == '"') out.append("\"\"");  // escape quote
    else out.push_back(c);
  }
  out.push_back('"');
  return out;
}

std::unique_ptr<ParsedObject> CsvDistributorSchema::Parse(const ByteBuffer& data) const {
  try {
    std::string line(data.begin(), data.end());
    TrimLineEnd(line);

    auto fields = ParseCsvRecord(line);
    return std::make_unique<CsvParsedObject>(std::move(fields));
  } catch (...) {
    return nullptr;
  }
}
  
ByteBuffer CsvDistributorSchema::Serialize(const ParsedObject& obj) const {
  try {
    const auto* p = dynamic_cast<const CsvParsedObject*>(&obj);
    if (!p) {
      return {};  // wrong ParsedObject type
    }

    std::ostringstream oss;
    for (size_t i = 0; i < p->fields.size(); ++i) {
      if (i > 0) oss << ',';
      oss << EscapeCsvField(p->fields[i]);
    }

    std::string result = oss.str();
    return ByteBuffer(result.begin(), result.end());
  } catch (...) {
    return {};
  }
}

void CsvDistributorSchema::BuildSchemas() {
    input_schema_.clear();
    output_schemas_.clear();
  
    for (size_t i = 0; i < header_.size(); ++i) {
      FieldSchema fs;
      fs.name = header_[i];
      fs.type = types_[i];
      fs.field_number = static_cast<int>(i);
      input_schema_.push_back(fs);
    }
  
    int fieldsplits = std::min(splits_, static_cast<int>(header_.size()));
    int groupsize = std::max(1, static_cast<int>(header_.size()) / fieldsplits);

    for (int i = 0; i < fieldsplits; i++) {
        std::vector<FieldSchema> output_schema;
        for (int j = 0; j < groupsize; j++) {
            int idx = i * groupsize + j;
            if (idx < static_cast<int>(header_.size())) {
                output_schema.push_back(input_schema_[idx]);
            }
        }
        if (i == fieldsplits - 1 && static_cast<size_t>(fieldsplits * groupsize) < header_.size()) {
            int k = fieldsplits * groupsize;
            while (k < static_cast<int>(header_.size())) {
                output_schema.push_back(input_schema_[k]);
                k++;
            }
        } 
        output_schemas_.push_back(output_schema);
    }
}

}