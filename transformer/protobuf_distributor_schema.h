#pragma once

#include <google/protobuf/message.h>
#include "rocksdb/transformer.h"

namespace ROCKSDB_NAMESPACE {

  class ProtobufDistributorSchema : public SchemaDescriptor {
    public:
      ProtobufDistributorSchema(std::unique_ptr<google::protobuf::Message> input_proto,
                                std::vector<std::unique_ptr<google::protobuf::Message>> output_proto) :
        input_proto_msgtype_(std::move(input_proto)), output_proto_msgtypes_(std::move(output_proto)) {}
  
      InputOutputDataType InputType() const override { return InputOutputDataType::PROTOBUF; }
      InputOutputDataType OutputType() const override { return InputOutputDataType::PROTOBUF; }
      bool Validate(const ByteBuffer& input_data) const override {
        return !input_data.empty();  // Add real checks if needed later
      }
  
      std::shared_ptr<void> Parse(const ByteBuffer& data) const override;
      ByteBuffer Serialize(const std::shared_ptr<void>& obj) const override;

      int GetNumSplits() const { return output_proto_msgtypes_.size(); }
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

    private:
      std::unique_ptr<google::protobuf::Message> input_proto_msgtype_;
      std::vector<std::unique_ptr<google::protobuf::Message>> output_proto_msgtypes_;
  };

}