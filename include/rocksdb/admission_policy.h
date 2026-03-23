#pragma once
// P2 shim: rocksdb/admission_policy.h forwards to mycelium/admission_policy.h
#include "rocksdb/rocksdb_namespace.h"
#include "mycelium/admission_policy.h"

namespace ROCKSDB_NAMESPACE {
  using mycelium::AdmissionPolicy;
  using mycelium::AlwaysAdmitPolicy;
  using mycelium::NeverAdmitPolicy;
  using mycelium::ThresholdAdmissionPolicy;
  using mycelium::EWMAAdmissionPolicy;
}  // namespace ROCKSDB_NAMESPACE
