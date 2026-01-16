#include <iostream>
#include <cassert>
#include <nlohmann/json.hpp>
#include "distributor.h"

namespace ROCKSDB_NAMESPACE {

std::vector<ByteBuffer> Distributor::Transform(
      const ByteBuffer& input,
      const std::shared_ptr<SchemaDescriptor>& schema) const
{
    using google::protobuf::Message;
    using google::protobuf::Reflection;
    using google::protobuf::Descriptor;
    using google::protobuf::FieldDescriptor;
    std::vector<ByteBuffer> outputs;
    
    auto proto_schema = std::static_pointer_cast<const ProtobufDistributorSchema>(schema);
    if (proto_schema) {
        data::ByteRow row;
        if (!row.ParseFromArray(input.data(), input.size())) {
            std::cout << "parsing value input into Protobuf schema had an error." << std::endl;
            return outputs;
        }

        int field_count = row.values_size();
        int num_outputs = proto_schema->GetNumSplits();
        if (field_count == 0 || num_outputs == 0) {
            outputs.push_back(input);
            return outputs;
        }
        int fields_per_output = (field_count + num_outputs - 1) / num_outputs;

        // Create N output messages
        std::vector<std::unique_ptr<Message>> output_msgs;
        for (int i = 0; i < num_outputs; ++i) {
            output_msgs.push_back(std::unique_ptr<Message>(
                proto_schema->GetOutputSchemaSpecs()[i]->New()));
        }

        for (int g = 0; g < num_outputs; ++g) {
            data::ByteRow groupRow;

            const int start = g * fields_per_output;
            const int end   = std::min(start + fields_per_output, field_count);
            for (int i = start; i < end; ++i) {
                const auto& src = row.values(i);
                auto* c = groupRow.add_values();
                c->set_value(src.value());
            }

            std::string serialized;
            serialized.reserve(groupRow.ByteSizeLong());   // optional, avoids reallocs
            groupRow.SerializeToString(&serialized);

            outputs.emplace_back(ByteBuffer(serialized.begin(), serialized.end()));            
        }
    }
    
    return outputs;
}

}