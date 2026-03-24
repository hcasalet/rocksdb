#pragma once
// Shim — P2 portability refactor.  Implementation is in libmycelium (namespace mycelium).
#include "rocksdb/rocksdb_namespace.h"
#include "mycelium/flatbuffers_encoder.h"
namespace ROCKSDB_NAMESPACE {
using mycelium::FlatbuffersEncoder;
}  // namespace ROCKSDB_NAMESPACE
