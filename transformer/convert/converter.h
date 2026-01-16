#include <memory>
#include "flatbuffers/flatbuffers.h"
#include "rocksdb/transformer.h"
#include "row_generated.h"
#include "data.pb.h"
#include "json2protobuf_schema.h"
#include "protobuf2flatbuffers_schema.h"

namespace ROCKSDB_NAMESPACE {

class ConvertSchemaDescriptor final : public SchemaDescriptor {
 public:
  ConvertSchemaDescriptor(Codec input_codec,
                          Codec output_codec,
                          std::vector<FieldSchema> input_schema = {},
                          std::vector<std::vector<FieldSchema>> output_schemas = {});

  TransformerType SupportsTransformerType() const override {
    return TransformerType::CONVERTER;
  }

  const Codec& InputCodec() const override { return input_codec_; }
  const Codec& OutputCodec() const override { return output_codec_; }

  const std::vector<FieldSchema>& GetInputFieldSchema() const override { return input_schema_; }
  const std::vector<std::vector<FieldSchema>>& GetOutputFieldSchemas() const override {
    return output_schemas_;
  }

 private:
  Codec input_codec_;
  Codec output_codec_;
  std::vector<FieldSchema> input_schema_;
  std::vector<std::vector<FieldSchema>> output_schemas_;
};

class Converter final : public Transformer {
 public:
  std::string Name() const override { return "convert_transformer"; }
  TransformerType Supports() const override { return TransformerType::CONVERTER; }

  std::vector<ByteBuffer> Transform(
      const ByteBuffer& input_bytes,
      const std::shared_ptr<SchemaDescriptor>& schema) const override;
};

}