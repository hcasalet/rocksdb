#pragma once

#include <memory>
#include <string>
#include <nlohmann/json.hpp>
#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

class JsonAugmenterSchema : public SchemaDescriptor {
  public:
    JsonAugmenterSchema(std::vector<std::vector<std::string>> index_keys,
                        nlohmann::json input_example_json)
            : index_keys_(std::move(index_keys)),
              input_template_(input_example_json) {
        BuildInputSchema();
    }

    TransformerType SupportsTransformerType() const override { return TransformerType::AUGMENTER; }

    InputOutputDataType InputType() const override { return InputOutputDataType::JSON; }
    InputOutputDataType OutputType() const override { return InputOutputDataType::JSON; }

    std::shared_ptr<void> Parse(const ByteBuffer& data) const override;
    ByteBuffer Serialize(const std::shared_ptr<void>& obj) const override;

    std::vector<std::vector<std::string>> GetIndexKeys() const override { return index_keys_; }

    std::vector<FieldSchema> GetInputFieldSchema() const override { return input_field_schema_; }

  private:
    void BuildInputSchema();
    std::vector<std::vector<std::string>> index_keys_;
    nlohmann::json input_template_;
    std::vector<FieldSchema> input_field_schema_;
};

}