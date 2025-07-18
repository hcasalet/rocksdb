#pragma once

#include "rocksdb/transformer.h"
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

namespace ROCKSDB_NAMESPACE {

class CsvDistributorSchema : public SchemaDescriptor {
  public:
    CsvDistributorSchema(std::vector<std::string> header,
                         std::vector<std::string> types, int splits)
          : header_(std::move(header)), types_(std::move(types)), splits_(splits) {
      BuildSchemas();
    }

    TransformerType SupportsTransformerType() const override { return TransformerType::DISTRIBUTOR; }
    
    InputOutputDataType InputType() const override { return InputOutputDataType::CSV; }
    InputOutputDataType OutputType() const override { return InputOutputDataType::CSV; }
    bool Validate(const ByteBuffer& data) const override;

    std::shared_ptr<void> Parse(const ByteBuffer& data) const override;
    ByteBuffer Serialize(const std::shared_ptr<void>& obj) const override;

    int GetNumSplits() const override { return splits_; }
    
    std::vector<FieldSchema> GetInputFieldSchema() const override { return input_schema_; }
    std::vector<std::vector<FieldSchema>> GetOutputFieldSchemas() const override { return output_schemas_; }
    
 private:
    std::vector<std::string> header_;
    std::vector<std::string> types_;
    int splits_;
    std::vector<FieldSchema> input_schema_;
    std::vector<std::vector<FieldSchema>> output_schemas_; // defaults to a single copy of input_schema_
    
    void BuildSchemas();
};
    
}  // namespace ROCKSDB_NAMESPACE
    