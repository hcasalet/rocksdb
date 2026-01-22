#include "distributor.h"  

#include <unordered_set>
#include <utility>
#include <vector>

#include <arrow/result.h>
#include <arrow/scalar.h>
#include <arrow/status.h>
#include <arrow/type.h>
#include <arrow/type_traits.h>
#include <arrow/util/checked_cast.h>

namespace ROCKSDB_NAMESPACE {

namespace {

bool ValidateSplitGroup(const std::vector<int>& cols, int32_t num_fields) {
  if (cols.empty()) return false;

  std::unordered_set<int> seen;
  seen.reserve(cols.size());

  for (int c : cols) {
    if (c < 0) return false;
    if (c >= num_fields) return false;
    if (!seen.insert(c).second) return false;  // duplicate within the group
  }
  return true;
}

}  // namespace

std::vector<ArrowRecord> Distributor::Transform(const Slice& /*key*/,
                                               const ArrowRecord& input) const {
  std::vector<ArrowRecord> outputs;
  outputs.reserve(splits_.size());

  if (!input) {
    // No input => no outputs (policy choice).
    return outputs;
  }

  const std::shared_ptr<arrow::DataType>& dtype = input->type;
  if (!dtype || dtype->id() != arrow::Type::STRUCT) {
    // Not a struct => cannot split into sub-structs.
    return {};
  }

  const auto& struct_type = arrow::internal::checked_cast<const arrow::StructType&>(*dtype);
  const int32_t num_fields = struct_type.num_fields();

  // For each requested split group, build a new StructType + StructScalar.
  for (const auto& cols : splits_) {
    if (!ValidateSplitGroup(cols, num_fields)) {
      // With this API (no Status return), fail-fast with empty result.
      return {};
    }

    std::vector<std::shared_ptr<arrow::Field>> out_fields;
    out_fields.reserve(cols.size());

    std::vector<std::shared_ptr<arrow::Scalar>> out_scalars;
    out_scalars.reserve(cols.size());

    for (int idx : cols) {
      const std::shared_ptr<arrow::Field>& f = struct_type.field(idx);
      out_fields.emplace_back(f);

      if (input->is_valid) {
        // StructScalar::field(i) returns the i-th child scalar.
        auto s_res = input->field(idx);  
        if (!s_res.ok()) {
          return {};
        }
        std::shared_ptr<arrow::Scalar> s = std::move(*s_res);
        out_scalars.emplace_back(std::move(s));
      } else {
        // Input is null; emit a null scalar of the correct field type.
        auto maybe_null = arrow::MakeNullScalar(f->type());
        if (!maybe_null) {
          return {};
        }
        out_scalars.emplace_back(std::move(maybe_null));
      }
    }

    auto out_type = arrow::struct_(std::move(out_fields));

    // Construct the output struct scalar.
    // This constructor exists in Arrow C++: StructScalar(vector<Scalar>, DataType).
    outputs.emplace_back(
        std::make_shared<arrow::StructScalar>(std::move(out_scalars), out_type));
  }

  return outputs;
}

}  // namespace ROCKSDB_NAMESPACE