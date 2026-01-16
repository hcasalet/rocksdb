#include <cstdint>  // For int32_t
#include <utility>

#include <nlohmann/json.hpp>
#include "converter.h"

namespace ROCKSDB_NAMESPACE {

ConvertSchemaDescriptor::ConvertSchemaDescriptor(
    Codec input_codec,
    Codec output_codec,
    std::vector<FieldSchema> input_schema,
    std::vector<std::vector<FieldSchema>> output_schemas)
    : input_codec_(std::move(input_codec)),
      output_codec_(std::move(output_codec)),
      input_schema_(std::move(input_schema)),
      output_schemas_(std::move(output_schemas)) {}

std::vector<ByteBuffer> Converter::Transform(
    const ByteBuffer& input_bytes,
    const std::shared_ptr<SchemaDescriptor>& schema) const {
  std::vector<ByteBuffer> outs;
  if (!schema) return outs;

  // Validate + parse using schema input codec.
  if (!schema->Validate(input_bytes)) return outs;

  std::unique_ptr<ParsedObject> parsed = schema->Parse(input_bytes);
  if (!parsed) return outs;

  // Encode using schema output codec.
  ByteBuffer encoded = schema->Serialize(*parsed);

  // NOTE: empty could mean “failure” or a valid empty encoding.
  // For the POC, treat empty as failure.
  if (encoded.empty()) return outs;

  outs.emplace_back(std::move(encoded));
  return outs;
}

}
