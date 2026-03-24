#pragma once
// Shim — P2 portability refactor.  Implementation is in libmycelium (namespace mycelium).
#include "rocksdb/rocksdb_namespace.h"
#include "mycelium/json_parser.h"
namespace ROCKSDB_NAMESPACE {
using mycelium::JsonColsParser;
}  // namespace ROCKSDB_NAMESPACE
