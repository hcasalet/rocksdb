#pragma once
// Shim — P2 portability refactor.  Implementation is in libmycelium (namespace mycelium).
#include "rocksdb/rocksdb_namespace.h"
#include "mycelium/fixedbin64_encoder.h"
namespace ROCKSDB_NAMESPACE {
using mycelium::FixedBin64Encoder;
}  // namespace ROCKSDB_NAMESPACE
