#include "transformer.h"

bool Transformer::ParseSimpleJsonObject(const rocksdb::Slice& value, 
    std::vector<std::pair<std::string, std::string>>* kvs) {

  kvs->clear();

  const char* p = value.data();
  const char* e = p + value.size();

  SkipWs(p, e);
  if (p >= e || *p != '{') return false;
  ++p;

  SkipWs(p, e);
  if (p < e && *p == '}') {  // empty object
    ++p;
    return true;
  }

  while (p < e) {
    SkipWs(p, e);
    if (p >= e || *p != '"') return false;
    ++p;

    // Parse key until next '"'
    const char* k0 = p;
    while (p < e && *p != '"') ++p;
    if (p >= e) return false;
    const char* k1 = p;
    ++p;  // consume closing quote
    std::string key(k0, static_cast<size_t>(k1 - k0));

    SkipWs(p, e);
    if (p >= e || *p != ':') return false;
    ++p;

    SkipWs(p, e);
    if (p >= e || *p != '"') return false;
    ++p;

    // Parse value until next '"'
    const char* v0 = p;
    while (p < e && *p != '"') ++p;
    if (p >= e) return false;
    const char* v1 = p;
    ++p;  // consume closing quote
    std::string val(v0, static_cast<size_t>(v1 - v0));

    kvs->emplace_back(std::move(key), std::move(val));

    SkipWs(p, e);
    if (p >= e) return false;

    if (*p == ',') {
      ++p;
      continue;
    }
    if (*p == '}') {
      ++p;
      return true;
    }
    return false;
  }

  return false;    
}

int Transformer::ColumnIndexFromName(const std::string& name)
{  
  // names are "col0", "col3", ...
  if (name.size() < 4) return -1;
  if (name[0] != 'c' || name[1] != 'o' || name[2] != 'l') return -1;
  int idx = 0;
  for (size_t i = 3; i < name.size(); ++i) {
    char ch = name[i];
    if (ch < '0' || ch > '9') return -1;
    idx = idx * 10 + (ch - '0');
  }
  return idx;
}

void Transformer::AppendJsonField(std::string* out, const std::string& k, const std::string& v, unsigned char& first)
{
  if (!first) out->push_back(',');
  first = 0;
  out->push_back('"');
  out->append(k);
  out->append("\":\"");
  out->append(v);
  out->push_back('"');
}

void Transformer::SplitRecords(const TransformConfig& cfg, const rocksdb::Slice& key,
                    const rocksdb::Slice& value, std::vector<std::pair<std::string, std::string>>* outs)
{
  outs->clear();

  std::vector<std::pair<std::string, std::string>> kvs;
  if (cfg.splits <= 1 || !ParseSimpleJsonObject(value, &kvs) || kvs.empty()) {
    outs->emplace_back(std::string(key.data(), key.size()),
                       std::string(value.data(), value.size()));
    return;
  }

  const int G = cfg.splits;
  const size_t ncols = kvs.size();

  // ceil(ncols / G), but never 0
  const size_t cols_per_split = (ncols + static_cast<size_t>(G) - 1) / static_cast<size_t>(G);

  std::vector<std::string> group_json(G);
  std::vector<uint8_t> first(G, 1);

  // Rough reserve to reduce reallocs.
  for (int g = 0; g < G; ++g) {
    group_json[g].reserve(value.size() / static_cast<size_t>(G) + 32);
    group_json[g].push_back('{');
  }

  // Assign by position in kvs (contiguous splits).
  for (size_t i = 0; i < ncols; ++i) {
    int g = static_cast<int>(i / cols_per_split);
    if (g >= G) g = G - 1;  // safety clamp

    AppendJsonField(&group_json[g], kvs[i].first, kvs[i].second, first[g]);
  }

  outs->reserve(static_cast<size_t>(G));
  for (int g = 0; g < G; ++g) {
    group_json[g].push_back('}');

    // Skip empty groups: "{}"
    if (group_json[g].size() == 2) continue;

    // Tight: reuse original key (no prefixing).
    // If you need uniqueness per split, add a suffix/prefix here (recommended).
    outs->emplace_back(std::string(key.data(), key.size()), std::move(group_json[g]));
  }
}

std::optional<std::pair<std::string, std::string>>
Transformer::TransformOne(const rocksdb::Slice& key, const rocksdb::Slice& value) {
  std::string out_key = "etl:";
  out_key.append(key.data(), key.size());
  std::string out_val(value.data(), value.size());
  return std::make_optional(std::make_pair(std::move(out_key), std::move(out_val)));
}

// Multi-output variant (useful for split/index). Produces 0..N outputs.
void Transformer::TransformOneMulti(
    const rocksdb::Slice& key,
    const rocksdb::Slice& value,
    std::vector<std::pair<std::string, std::string>>* outs) {

  // Example: emit one KV.
  auto maybe = TransformOne(key, value);
  if (maybe) outs->push_back(std::move(*maybe));
}