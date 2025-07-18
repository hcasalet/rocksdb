#pragma once

#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

  class ProtobufDistributorSchema : public SchemaDescriptor {
    public:
      ProtobufDistributorSchema(int splits,
                                std::unique_ptr<google::protobuf::Message> input_proto,
                                std::vector<std::unique_ptr<google::protobuf::Message>> output_proto)
        : splits_(splits), input_proto_msgtype_(std::move(input_proto)),
          output_proto_msgtypes_(std::move(output_proto)) {
        BuildInputFieldSchema();
        BuildOutputFieldSchemas();
      }
      
      TransformerType SupportsTransformerType() const override { return TransformerType::DISTRIBUTOR; }
  
      InputOutputDataType InputType() const override { return InputOutputDataType::PROTOBUF; }
      InputOutputDataType OutputType() const override { return InputOutputDataType::PROTOBUF; }

      bool Validate(const ByteBuffer& input_data) const override;
  
      std::shared_ptr<void> Parse(const ByteBuffer& data) const override;
      ByteBuffer Serialize(const std::shared_ptr<void>& obj) const override;

      int GetNumSplits() const override { return splits_; }

      std::unique_ptr<google::protobuf::Message> GetInputSchemaSpec() const { 
        return std::unique_ptr<google::protobuf::Message>(input_proto_msgtype_->New());
      }

      std::vector<std::unique_ptr<google::protobuf::Message>> GetOutputSchemaSpecs() const {
        std::vector<std::unique_ptr<google::protobuf::Message>> clones;
        for (const auto& m : output_proto_msgtypes_) {
          clones.push_back(std::unique_ptr<google::protobuf::Message>(m->New()));
        }
        return clones;
      }

      std::vector<FieldSchema> GetInputFieldSchema() const override {
        return input_field_schema_;
      }
    
      std::vector<std::vector<FieldSchema>> GetOutputFieldSchemas() const override {
        return output_field_schemas_;
      }

    private:
      int splits_;
      std::unique_ptr<google::protobuf::Message> input_proto_msgtype_;
      std::vector<std::unique_ptr<google::protobuf::Message>> output_proto_msgtypes_;

      std::vector<FieldSchema> input_field_schema_;
      std::vector<std::vector<FieldSchema>> output_field_schemas_;

      void BuildInputFieldSchema() {
        const auto* descriptor = input_proto_msgtype_->GetDescriptor();
        for (int i = 0; i < descriptor->field_count(); ++i) {
          const auto* field = descriptor->field(i);
          input_field_schema_.push_back({
            field->name(),
            field->type_name(),  // Or use Type enum
            field->number()
          });
        }
      }

      void BuildOutputFieldSchemas() {
        for (const auto& msg : output_proto_msgtypes_) {
          std::vector<FieldSchema> output_field_schema;
          const auto* descriptor = msg->GetDescriptor();
          for (int i = 0; i < descriptor->field_count(); ++i) {
            const auto* field = descriptor->field(i);
            output_field_schema.push_back({
              field->name(),
              field->type_name(),
              field->number()
            });
          }
          output_field_schemas_.push_back(output_field_schema);
        }
      }

  };

}