#pragma once

#include <memory>
#include <string>
#include <google/protobuf/message.h>
#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

class ProtobufAugmenterSchema : public SchemaDescriptor {
  public:
    ProtobufAugmenterSchema(std::vector<std::vector<std::string>> index_keys,
                            std::unique_ptr<google::protobuf::Message> input_template)
            : index_keys_(std::move(index_keys)),
              input_template_(std::move(input_template)) {
        BuildInputSchema();
    }

    InputOutputDataType InputType() const override { return InputOutputDataType::PROTOBUF; }
    InputOutputDataType OutputType() const override { return InputOutputDataType::PROTOBUF; }

    std::shared_ptr<void> Parse(const ByteBuffer& data) const override;
    ByteBuffer Serialize(const std::shared_ptr<void>& obj) const override;

    const std::vector<std::vector<std::string>>& GetIndexKeys() const { return index_keys_; }

    std::vector<FieldSchema> GetInputFieldSchema() const override { return input_field_schema_; }

  private:
    void BuildInputSchema();
    std::vector<std::vector<std::string>> index_keys_;
    std::unique_ptr<google::protobuf::Message> input_template_;
    std::vector<FieldSchema> input_field_schema_;
};

}