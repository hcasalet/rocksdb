#include "distributor.h"

#include <algorithm>
#include <stdexcept>

namespace ROCKSDB_NAMESPACE {

DistributorSchemaDescriptor::DistributorSchemaDescriptor(
    Codec codec,
    std::vector<FieldSchema> input_schema,
    SplitByPosition splits)
    : codec_(std::move(codec)),
      input_schema_(std::make_shared<const std::vector<FieldSchema>>(std::move(input_schema))),
      splits_(std::move(splits)) {
  NormalizeAndValidate_();
}

void DistributorSchemaDescriptor::NormalizeAndValidate_() {
  // Distributor requires both parser and encoder.
  if (!codec_.parser) {
    throw std::invalid_argument("DistributorSchemaDescriptor: codec.parser is null");
  }
  if (!codec_.encoder) {
    throw std::invalid_argument("DistributorSchemaDescriptor: codec.encoder is null");
  }

  // Validate indices if we have a known schema size.
  const int n = input_schema_ ? static_cast<int>(input_schema_->size()) : 0;
  const bool can_validate = (n > 0);

  // Default: if no splits specified, produce 1 output containing all columns.
  if (splits_.empty()) {
    if (n > 0) {
      std::vector<int> all;
      all.reserve(n);
      for (int i = 0; i < n; ++i) all.push_back(i);
      splits_.push_back(std::move(all));
    } else {
      splits_.push_back({});
    }
  }

  for (const auto& group : splits_) {
    // Check duplicates (always).
    std::vector<int> sorted = group;
    std::sort(sorted.begin(), sorted.end());
    if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end()) {
      throw std::invalid_argument("DistributorSchemaDescriptor: duplicate column in split group");
    }

    // Bounds-check if possible.
    if (can_validate) {
      for (int idx : group) {
        if (idx < 0 || idx >= n) {
          throw std::invalid_argument("DistributorSchemaDescriptor: split index out of range");
        }
      }
    }
  }
}

bool Distributor::IsDistributorSchema_(
    const std::shared_ptr<SchemaDescriptor>& schema) {
  if (!schema) return false;
  return (schema->SupportsTransformerType() & TransformerType::DISTRIBUTOR) !=
         TransformerType::NOTRANSFORMATION;
}

std::unique_ptr<ParsedObject> Distributor::MakeProjectedObject_(
    std::shared_ptr<const ParsedObject> base,
    const std::vector<int>& cols,
    std::shared_ptr<const std::vector<FieldSchema>> input_schema,
    InputOutputDataType out_fmt) {
  auto out = std::make_unique<ParsedObject>();
  auto proj = std::make_unique<ProjectedPayload>(std::move(base), cols, std::move(input_schema));

  // Store projection wrapper in payload. We tag format as the schema's OutputType.
  out->payload = ParsedPayload::Make<ProjectedPayload>(out_fmt, std::move(proj));
  return out;
}

std::vector<ByteBuffer> Distributor::Transform(
    const ByteBuffer& input_bytes,
    const std::shared_ptr<SchemaDescriptor>& schema) const {
  std::vector<ByteBuffer> outputs;

  if (!IsDistributorSchema_(schema)) {
    return outputs;
  }

  // Parse once (derived parser).
  std::unique_ptr<ParsedObject> parsed_unique = schema->Parse(input_bytes);
  if (!parsed_unique) {
    return outputs;
  }

  // Keep parsed object alive for multiple projections.
  std::shared_ptr<const ParsedObject> parsed_shared(parsed_unique.release());

  // Get distributor splits.
  const auto* dist_schema = dynamic_cast<const DistributorSchemaDescriptor*>(schema.get());
  if (!dist_schema) {
    return outputs;
  }

  const auto& splits = dist_schema->GetSplits();
  outputs.reserve(splits.size());

  const InputOutputDataType out_fmt = schema->OutputType();

  for (const auto& group : splits) {
    auto projected = MakeProjectedObject_(parsed_shared, group, dist_schema->InputSchemaPtr(), out_fmt);

    ByteBuffer encoded = schema->Serialize(*projected);

    // Fail-fast on serialization errors to catch mismatch early.
    if (encoded.empty()) {
      outputs.clear();
      return outputs;
    }

    outputs.push_back(std::move(encoded));
  }

  return outputs;
}

}  // namespace ROCKSDB_NAMESPACE