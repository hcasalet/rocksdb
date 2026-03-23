#pragma once
// Shim header — P2 portability refactor.
// The implementation now lives in libmycelium (namespace mycelium).
// This header forwards the types into ROCKSDB_NAMESPACE so that existing
// RocksDB-tree code continues to compile unchanged.
#include "rocksdb/rocksdb_namespace.h"
#include "mycelium/converter.h"

namespace ROCKSDB_NAMESPACE {
using mycelium::Converter;
}  // namespace ROCKSDB_NAMESPACE