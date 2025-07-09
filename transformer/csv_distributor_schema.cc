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

std::shared_ptr<void> CsvDistributorSchema::Parse(const ByteBuffer& data) const {
  auto parsed = std::make_shared<std::vector<std::string>>();
  std::string line(data.begin(), data.end());
  std::stringstream ss(line);
  std::string token;

  while (std::getline(ss, token, ',')) {
    parsed->emplace_back(token);
  }

  return parsed;
}
  
ByteBuffer CsvDistributorSchema::Serialize(const std::shared_ptr<void>& obj) const {
  //const auto& fields = *std::static_pointer_cast<std::vector<std::string>>(obj);
  auto* fields = static_cast<std::vector<std::string>*>(obj.get());
  std::ostringstream oss;
  for (size_t i = 0; i < fields->size(); ++i) {
    if (i > 0) oss << ",";
    oss << (*fields)[i];
  }
  std::string result = oss.str();
  return ByteBuffer(result.begin(), result.end());
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