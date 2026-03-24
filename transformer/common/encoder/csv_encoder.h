#pragma once
// Shim — P2 portability refactor.  Implementation is in libmycelium (namespace mycelium).
#include "rocksdb/rocksdb_namespace.h"
#include "mycelium/csv_encoder.h"
namespace ROCKSDB_NAMESPACE {
using mycelium::CsvEncoder;
}  // namespace ROCKSDB_NAMESPACE
