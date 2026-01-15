#include "json_encoder.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace ROCKSDB_NAMESPACE {

InputOutputDataType JsonEncoder::OutputType() const {
  return InputOutputDataType::JSON;
}

ByteBuffer JsonEncoder::Serialize(const ParsedObject& obj) const {
  if (obj.payload.format != InputOutputDataType::JSON) {
    return {};
  }
  auto* doc = obj.payload.As<rapidjson::Document>();
  if (!doc) return {};

  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
  doc->Accept(writer);

  const char* s = sb.GetString();
  const size_t n = sb.GetSize();
  return ByteBuffer(reinterpret_cast<const std::uint8_t*>(s),
                    reinterpret_cast<const std::uint8_t*>(s) + n);
}

}