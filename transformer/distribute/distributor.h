#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "rocksdb/transformer.h"
#include "rocksdb/rocksdb_namespace.h"

namespace ROCKSDB_NAMESPACE {

// A format-agnostic "view" wrapper for Distributor.
// Encoders can check for this payload and serialize only the selected columns
// from the base parsed object.
struct ProjectedPayload {
  // Original fully-parsed object (whatever your derived parser produced).
  std::shared_ptr<const ParsedObject> base;

  // Column positions to include in this projection.
  // Convention: empty => "serialize all columns" (encoder may choose).
  std::vector<int> columns;

  // Optional: provide input schema so encoders can map positions to metadata.
  // This is helpful if your base payload doesn't carry field metadata.
  std::shared_ptr<const std::vector<FieldSchema>> input_schema;

  ProjectedPayload(std::shared_ptr<const ParsedObject> b,
                   std::vector<int> cols,
                   std::shared_ptr<const std::vector<FieldSchema>> schema = nullptr)
      : base(std::move(b)), columns(std::move(cols)), input_schema(std::move(schema)) {}
};

// DistributorSchemaDescriptor:
// - Uses ONE Codec (parser + encoder) for both input and output.
// - Stores split plan by column positions.
class DistributorSchemaDescriptor final : public SchemaDescriptor {
 public:
  using SplitByPosition = std::vector<std::vector<int>>;

  DistributorSchemaDescriptor(Codec codec,
                              std::vector<FieldSchema> input_schema = {},
                              SplitByPosition splits = {});

  TransformerType SupportsTransformerType() const override {
    return TransformerType::DISTRIBUTOR;
  }

  // Single codec for both directions.
  const Codec& InputCodec() const override { return codec_; }
  const Codec& OutputCodec() const override { return codec_; }

  const std::vector<FieldSchema>& GetInputFieldSchema() const override {
    return input_schema_;
  }

  int GetNumSplits() const override { return static_cast<int>(splits_.size()); }

  // Reuse existing interface hook to expose split-by-position.
  std::vector<std::vector<int>> GetPositionedIndexKeys() const override { return splits_; }

  const SplitByPosition& GetSplits() const { return splits_; }

 private:
  void NormalizeAndValidate_();

  Codec codec_;
  std::vector<FieldSchema> input_schema_;
  SplitByPosition splits_;
};

// DistributorTransformer:
// - Parses input bytes once using schema->Parse (derived parser)
// - For each split, wraps parsed object in ProjectedPayload and calls schema->Serialize
//   (derived encoder) to produce one output per split.
class DistributorTransformer final : public Transformer {
 public:
  std::string Name() const override { return "DistributorTransformer"; }

  TransformerType Supports() const override { return TransformerType::DISTRIBUTOR; }

  std::vector<ByteBuffer> Transform(
      const ByteBuffer& input_bytes,
      const std::shared_ptr<SchemaDescriptor>& schema) const override;

 private:
  static bool IsDistributorSchema_(const std::shared_ptr<SchemaDescriptor>& schema);

  static std::unique_ptr<ParsedObject> MakeProjectedObject_(
      std::shared_ptr<const ParsedObject> base,
      const std::vector<int>& cols,
      std::shared_ptr<const std::vector<FieldSchema>> input_schema,
      InputOutputDataType out_fmt);
};

}  // namespace ROCKSDB_NAMESPACE