#include <rapidjson/document.h>

#include "rocksdb/arrow_compaction_batcher.h"
#include "distribute/protobuf_distributor_schema.h"

#include <utility>

#include <arrow/api.h>

namespace ROCKSDB_NAMESPACE {

arrow::Result<ParsedRow> ValueParser::Parse(const rocksdb::Slice& value) const {
  InputOutputDataType fmt = schema_.InputType();
  
  switch (fmt) {
    case InputOutputDataType::JSON:     return ParseJson(value);
    case InputOutputDataType::CSV:      return ParseCsv(value);
    case InputOutputDataType::PROTOBUF: {
        auto* proto_schema = dynamic_cast<const ProtobufDistributorSchema*>(&schema_);
        auto spec = proto_schema->GetInputSchemaSpec();
        return ParseProtobuf(value, spec);
    }
    case InputOutputDataType::FIXEDBIN64:  return ParseBin64(value);
    case InputOutputDataType::FLATBUFFERS:
    case InputOutputDataType::AVRO:
    case InputOutputDataType::PARQUET:
    case InputOutputDataType::UNKNOWN:
    default:
      return arrow::Status::Invalid("Unknown format");
  }
  return arrow::Status::Invalid("Unknown format");
}

arrow::Result<ParsedFields> ValueToParsedField(const std::string& name,
                                               const rapidjson::Value& v) {
  ParsedFields f;
  f.name = name;

  if (v.IsNull()) {
    // No type information; you can return null<null> and let Add() decide,
    // but it’s cleaner to reject or coerce. Here: treat as null string.
    f.declared_type = arrow::utf8();
    //f.scalar = std::make_shared<arrow::StringScalar>(std::nullopt);
    f.scalar =  arrow::MakeNullScalar(arrow::utf8());
    return f;
  }

  if (v.IsBool()) {
    f.declared_type = arrow::boolean();
    f.scalar = std::make_shared<arrow::BooleanScalar>(v.GetBool());
    return f;
  }
  if (v.IsInt64()) {
    f.declared_type = arrow::int64();
    f.scalar = std::make_shared<arrow::Int64Scalar>(v.GetInt64());
    return f;
  }
  if (v.IsUint64()) {
    // If you want uint64 support, use arrow::uint64(). Otherwise, coerce.
    f.declared_type = arrow::int64();
    uint64_t u = v.GetUint64();
    if (u > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
      return arrow::Status::Invalid("uint64 too large for int64 field ", name);
    }
    f.scalar = std::make_shared<arrow::Int64Scalar>(static_cast<int64_t>(u));
    return f;
  }
  if (v.IsDouble()) {
    f.declared_type = arrow::float64();
    f.scalar = std::make_shared<arrow::DoubleScalar>(v.GetDouble());
    return f;
  }
  if (v.IsString()) {
    f.declared_type = arrow::utf8();
    f.scalar = std::make_shared<arrow::StringScalar>(
        std::string(v.GetString(), v.GetStringLength()));
    return f;
  }

  // Nested values: either reject, stringify, or emit struct/list/map.
  // For compaction MVP: stringify.
  f.declared_type = arrow::utf8();
  // RapidJSON stringify omitted here; simplest: reject for now.
  return arrow::Status::NotImplemented("Nested JSON not supported for field ", name);
}

inline arrow::Result<std::shared_ptr<arrow::DataType>>
ArrowTypeForProtoField(const google::protobuf::FieldDescriptor& f) {
  using FD = google::protobuf::FieldDescriptor;

  // Repeated / map / message: not supported in MVP
  if (f.is_repeated()) {
    return arrow::Status::NotImplemented("Repeated proto field not supported: ", f.full_name());
  }
  if (f.cpp_type() == FD::CPPTYPE_MESSAGE) {
    return arrow::Status::NotImplemented("Nested message field not supported: ", f.full_name());
  }

  // Distinguish bytes vs string
  if (f.type() == FD::TYPE_BYTES) return arrow::binary();
  if (f.type() == FD::TYPE_STRING) return arrow::utf8();

  switch (f.cpp_type()) {
    case FD::CPPTYPE_BOOL:   return arrow::boolean();

    case FD::CPPTYPE_INT32:
    case FD::CPPTYPE_INT64:
    case FD::CPPTYPE_UINT32:
    case FD::CPPTYPE_UINT64:
    case FD::CPPTYPE_ENUM:
      // MVP: represent enums as int64 (numeric value)
      return arrow::int64();

    case FD::CPPTYPE_FLOAT:
    case FD::CPPTYPE_DOUBLE:
      return arrow::float64();

    case FD::CPPTYPE_STRING:
      // Should be handled by TYPE_BYTES / TYPE_STRING above.
      return arrow::utf8();

    default:
      return arrow::Status::NotImplemented("Unsupported proto field type: ", f.full_name());
  }
}

// Convert a single (non-repeated, non-message) field to a scalar, honoring presence.
inline arrow::Result<std::shared_ptr<arrow::Scalar>>
ScalarForProtoField(const google::protobuf::Message& msg,
                    const google::protobuf::Reflection& refl,
                    const google::protobuf::FieldDescriptor& f,
                    const std::shared_ptr<arrow::DataType>& arrow_type) {
  using FD = google::protobuf::FieldDescriptor;

  // Presence semantics:
  // - proto2: HasField works for singular fields.
  // - proto3: HasField works for message fields and 'optional' fields; for plain
  //   scalars it may be false even if "default" is present.
  // Policy: if HasField is false, return null; otherwise return value.
  const bool has = refl.HasField(msg, &f);
  if (!has) {
    return arrow::MakeNullScalar(arrow_type);
  }

  switch (f.cpp_type()) {
    case FD::CPPTYPE_BOOL: {
      return std::make_shared<arrow::BooleanScalar>(refl.GetBool(msg, &f));
    }

    case FD::CPPTYPE_INT32: {
      return std::make_shared<arrow::Int64Scalar>(
          static_cast<int64_t>(refl.GetInt32(msg, &f)));
    }
    case FD::CPPTYPE_INT64: {
      return std::make_shared<arrow::Int64Scalar>(
          static_cast<int64_t>(refl.GetInt64(msg, &f)));
    }
    case FD::CPPTYPE_UINT32: {
      return std::make_shared<arrow::Int64Scalar>(
          static_cast<int64_t>(refl.GetUInt32(msg, &f)));
    }
    case FD::CPPTYPE_UINT64: {
      const uint64_t u = refl.GetUInt64(msg, &f);
      if (u > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
        return arrow::Status::Invalid("uint64 too large for int64: ", f.full_name());
      }
      return std::make_shared<arrow::Int64Scalar>(static_cast<int64_t>(u));
    }

    case FD::CPPTYPE_ENUM: {
      const auto* e = refl.GetEnum(msg, &f);
      if (!e) return arrow::Status::Invalid("Null enum value: ", f.full_name());
      return std::make_shared<arrow::Int64Scalar>(static_cast<int64_t>(e->number()));
    }

    case FD::CPPTYPE_FLOAT: {
      return std::make_shared<arrow::DoubleScalar>(
          static_cast<double>(refl.GetFloat(msg, &f)));
    }
    case FD::CPPTYPE_DOUBLE: {
      return std::make_shared<arrow::DoubleScalar>(
          static_cast<double>(refl.GetDouble(msg, &f)));
    }

    case FD::CPPTYPE_STRING: {
      if (f.type() == FD::TYPE_BYTES) {
        const std::string bytes = refl.GetString(msg, &f);
        // BinaryScalar holds a buffer; easiest is to allocate a buffer and wrap it.
        auto buf = arrow::Buffer::FromString(bytes);
        return std::make_shared<arrow::BinaryScalar>(buf);
      }
      // TYPE_STRING
      return std::make_shared<arrow::StringScalar>(refl.GetString(msg, &f));
    }

    default:
      return arrow::Status::NotImplemented("Unsupported proto field cpp_type: ", f.full_name());
  }
}

inline std::uint64_t LoadFixed64LE(const std::uint8_t* p) {
  // Little-endian decode, explicit (portable).
  return (static_cast<std::uint64_t>(p[0])      ) |
         (static_cast<std::uint64_t>(p[1]) <<  8) |
         (static_cast<std::uint64_t>(p[2]) << 16) |
         (static_cast<std::uint64_t>(p[3]) << 24) |
         (static_cast<std::uint64_t>(p[4]) << 32) |
         (static_cast<std::uint64_t>(p[5]) << 40) |
         (static_cast<std::uint64_t>(p[6]) << 48) |
         (static_cast<std::uint64_t>(p[7]) << 56);
}

arrow::Result<ParsedRow> ValueParser::ParseJson(const rocksdb::Slice& value) const {
    rapidjson::Document d;
    d.Parse(value.data(), value.size());
    if (d.HasParseError()) {
      return arrow::Status::Invalid("JSON parse error");
    }
    if (!d.IsObject()) {
      return arrow::Status::Invalid("Expected JSON object");
    }

    ParsedRow out;
    out.fields.reserve(d.MemberCount());

    for (auto it = d.MemberBegin(); it != d.MemberEnd(); ++it) {
      const auto& k = it->name;
      const auto& v = it->value;

      std::string name(k.GetString(), k.GetStringLength());

      ARROW_ASSIGN_OR_RAISE(auto pf, ValueToParsedField(name, v));
      out.fields.push_back(std::move(pf));
    }
    return out;
}

arrow::Result<ParsedRow> ValueParser::ParseCsv(const rocksdb::Slice& value) const {
  const char* p = value.data();
  const char* end = value.data() + value.size();

  while (end > p && (end[-1] == '\n' || end[-1] == '\r')) --end;

  // Split the line into tokens (very simple splitter; extend for quotes if needed)
  std::vector<std::string_view> tokens;
  const char* start = p;
  const char delim = ',';

  while (p < end) {
    if (*p == delim) {
      tokens.emplace_back(start, static_cast<size_t>(p - start));
      start = p + 1;
    }
    ++p;
  }
  // last token
  tokens.emplace_back(start, static_cast<size_t>(p - start));

  ParsedRow out;
  out.fields.reserve(tokens.size());

  for (size_t i = 0; i < tokens.size(); ++i) {
    std::string col = "c" + std::to_string(i);

    out.fields.push_back(ParsedFields{
      .name = std::move(col),
      .scalar = std::make_shared<arrow::StringScalar>(std::string(tokens[i])),
      .declared_type = arrow::utf8(),
    });
  }

  return out;
}

arrow::Result<ParsedRow> ValueParser::ParseProtobuf(const rocksdb::Slice& value,
                    std::unique_ptr<google::protobuf::Message>& msg) const {
  if (!msg) {
    return arrow::Status::Invalid("ParseProtobuf message is empty");
  }

  if (value.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return arrow::Status::Invalid("ParseProtobuf: value too large");
  }
  if (!msg->ParseFromArray(value.data(), static_cast<int>(value.size()))) {
    return arrow::Status::Invalid("Protobuf parse failed");
  }

  const auto* desc = msg->GetDescriptor();
  const auto* refl = msg->GetReflection();
  if (!refl) {
    return arrow::Status::Invalid("ParseProtobuf: missing reflection for ", desc->full_name());
  }

  ParsedRow out;
  out.fields.reserve(static_cast<size_t>(desc->field_count()));

  for (int i = 0; i < desc->field_count(); ++i) {
    const google::protobuf::FieldDescriptor* f = desc->field(i);
    if (!f) continue;

    // MVP: skip unsupported field shapes (repeated/map/message)
    if (f->is_repeated()) continue;
    if (f->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) continue;

    ARROW_ASSIGN_OR_RAISE(auto arrow_type, ArrowTypeForProtoField(*f));
    ARROW_ASSIGN_OR_RAISE(auto scalar, ScalarForProtoField(*msg, *refl, *f, arrow_type));

    ParsedFields pf;
    pf.name = f->name();  // or f->json_name() if you prefer
    pf.scalar = std::move(scalar);
    pf.declared_type = std::move(arrow_type);

    out.fields.push_back(std::move(pf));
  }

  return out;
}

arrow::Result<ParsedRow> ValueParser::ParseBin64(const rocksdb::Slice& value) const {
  ParsedRow row;

  const auto n = static_cast<std::size_t>(value.size());
  if (n == 0) return row;

  if ((n % 8) != 0) {
    return arrow::Status::Invalid(
        "ParseBin: value length must be a multiple of 8 bytes (uint64_t), got ",
        n);
  }

  const std::size_t num_cols = n / 8;
  row.fields.reserve(num_cols);

  const auto* bytes = reinterpret_cast<const std::uint8_t*>(value.data());
  const auto dtype = arrow::uint64();

  for (std::size_t i = 0; i < num_cols; ++i) {
    const std::uint64_t v = LoadFixed64LE(bytes + (i * 8));

    ParsedFields f;
    f.name = "col" + std::to_string(i);              
    f.declared_type = dtype;             
    f.scalar = std::make_shared<arrow::UInt64Scalar>(v);

    row.fields.emplace_back(std::move(f));
  }

  return row;
}

ArrowCompactionBatcher::ArrowCompactionBatcher(const SchemaDescriptor& schema) 
    : parser_(schema) {}

arrow::Result<std::unique_ptr<ArrowCompactionBatcher>> ArrowCompactionBatcher::Create(const SchemaDescriptor& schema)
{
  auto ptr = std::unique_ptr<ArrowCompactionBatcher>(
      new ArrowCompactionBatcher(schema));
  ARROW_RETURN_NOT_OK(ptr->BuildFromSchema(schema));
  return ptr;
}

arrow::Status ArrowCompactionBatcher::BuildFromSchema(const SchemaDescriptor& schema) {
  fields_.clear();
  builders_.clear();
  schema_.reset();

  auto input_fields = schema.GetInputFieldSchema();
  fields_.reserve(1 + input_fields.size());
  builders_.reserve(1 + input_fields.size());

  fields_.push_back(arrow::field("key", arrow::binary(), /*nullable=*/false));
  builders_.push_back(std::make_unique<arrow::BinaryBuilder>());

  for (const auto& f : input_fields) {
    if (f.type == "fixedbin64") {
      fields_.push_back(arrow::field(f.name, arrow::uint64(), /*nullable=*/true));
      builders_.push_back(std::make_unique<arrow::UInt64Builder>()); 
    } else if (f.type == "string") {
      fields_.push_back(arrow::field(f.name, arrow::utf8(), /*nullable=*/true));
      builders_.push_back(std::make_unique<arrow::StringBuilder>());
    } else if (f.type == "numeric") {
      fields_.push_back(arrow::field(f.name, arrow::int64(), /*nullable=*/true));
      builders_.push_back(std::make_unique<arrow::Int64Builder>());
    } else {
      return arrow::Status::Invalid("Unsupported field type: ", f.type,
                                    " for field: ", f.name);
    }
  }

  schema_ = arrow::schema(fields_);
  return arrow::Status::OK();
}

// Clear is to clean up everything (get a clean slate)
arrow::Status ArrowCompactionBatcher::Clear() {
  num_rows_ = 0;
  num_bytes_ = 0;

  fields_.clear();
  builders_.clear();
  schema_.reset();

  return arrow::Status::OK();
}

// Reset is just to clear the data but keeps the structure
arrow::Status ArrowCompactionBatcher::Reset() {
  num_rows_ = 0;
  num_bytes_ = 0;

  for (auto& b : builders_) {
    if (b) b->Reset();
  }

  return arrow::Status::OK();
}

arrow::Status ArrowCompactionBatcher::Add(const Slice& internal_key,
                                         const Slice& value) {
  // Arrow BinaryBuilder::Append expects (const uint8_t*, int32_t)
  // Guard size conversion; RocksDB values can exceed 2GB in theory, but in
  // practice are far smaller. We fail loudly if someone hits this.
  auto to_i32 = [](size_t n) -> arrow::Result<int32_t> {
    if (n > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
      return arrow::Status::Invalid("Value too large for Arrow binary append");
    }
    return static_cast<int32_t>(n);
  };

  ARROW_ASSIGN_OR_RAISE(int32_t ik_sz, to_i32(internal_key.size()));
  auto* key_b = static_cast<arrow::BinaryBuilder*>(builders_[0].get());
  ARROW_RETURN_NOT_OK(key_b->Append(
      reinterpret_cast<const uint8_t*>(internal_key.data()), ik_sz));

  ARROW_ASSIGN_OR_RAISE(ParsedRow row, parser_.Parse(value));

  int col_idx = 1;
  for (const auto& f : row.fields) {
    ARROW_RETURN_NOT_OK(AppendScalarToBuilder(builders_[col_idx++].get(), *f.scalar));
  }

  num_rows_ += 1;
  num_bytes_ += internal_key.size() + value.size();
  return arrow::Status::OK();
}

arrow::Status ArrowCompactionBatcher::AppendScalarToBuilder(
    arrow::ArrayBuilder* b,
    const arrow::Scalar& s) {
  if (!s.is_valid) {
    return b->AppendNull();
  }

  switch (s.type->id()) {
    case arrow::Type::INT64: {
      auto& sb = static_cast<arrow::Int64Builder&>(*b);
      auto& ss = static_cast<const arrow::Int64Scalar&>(s);
      return sb.Append(ss.value);
    }
    case arrow::Type::DOUBLE: {
      auto& sb = static_cast<arrow::DoubleBuilder&>(*b);
      auto& ss = static_cast<const arrow::DoubleScalar&>(s);
      return sb.Append(ss.value);
    }
    case arrow::Type::STRING: {
      auto& sb = static_cast<arrow::StringBuilder&>(*b);
      auto& ss = static_cast<const arrow::StringScalar&>(s);
      // StringScalar stores a buffer; Append copies bytes
      return sb.Append(ss.value->ToString());
    }
    case arrow::Type::BINARY: {
      auto& sb = static_cast<arrow::BinaryBuilder&>(*b);
      auto& ss = static_cast<const arrow::BinaryScalar&>(s);
      // Append copies bytes; need (uint8_t*, i32)
      const auto& buf = *ss.value;
      if (buf.size() > static_cast<int64_t>(std::numeric_limits<int32_t>::max())) {
        return arrow::Status::Invalid("Binary scalar too large for builder");
      }
      return sb.Append(buf.data(), static_cast<int32_t>(buf.size()));
    }
    default:
      return arrow::Status::NotImplemented("AppendScalarToBuilder for type: ",
                                           s.type->ToString());
  }
}

arrow::Status ArrowCompactionBatcher::Flush(std::shared_ptr<arrow::RecordBatch>* out) {
  if (!out) return arrow::Status::Invalid("out is null");
  if (num_rows_ == 0) {
    *out = nullptr;
    return arrow::Status::OK();
  }

  std::vector<std::shared_ptr<arrow::Array>> arrays;
  arrays.reserve(builders_.size());
  for (auto& b : builders_) {
    std::shared_ptr<arrow::Array> arr;
    ARROW_RETURN_NOT_OK(b->Finish(&arr));
    arrays.push_back(std::move(arr));
  }

  *out = arrow::RecordBatch::Make(schema_, static_cast<int64_t>(num_rows_), std::move(arrays));
  return Reset();
}

}  // namespace rocksdb