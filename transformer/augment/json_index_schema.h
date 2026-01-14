#pragma once

#include <memory>
#include <string>
#include <nlohmann/json.hpp>
#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

struct JsonIndexParsedObject final : ParsedObject {
  explicit JsonIndexParsedObject(std::unique_ptr<nlohmann::json> m)
      : message(std::move(m)) {}

  std::unique_ptr<nlohmann::json> message;
};

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

    std::unique_ptr<ParsedObject> Parse(const ByteBuffer& data) const override;
    ByteBuffer Serialize(const ParsedObject& obj) const override;

    std::vector<std::vector<std::string>> GetIndexKeys() const override { return index_keys_; }

    const std::vector<FieldSchema>& GetInputFieldSchema() const override { return input_field_schema_; }

  private:
    void BuildInputSchema();
    std::vector<std::vector<std::string>> index_keys_;
    nlohmann::json input_template_;
    std::vector<FieldSchema> input_field_schema_;
};

}