#pragma once
// Shim — P2 portability refactor.  Implementation is in libmycelium (namespace mycelium).
#include "rocksdb/rocksdb_namespace.h"
#include "mycelium/fixedbin64_parser.h"
namespace ROCKSDB_NAMESPACE {
using mycelium::FixedBin64RowPayload;
using mycelium::FixedBin64Parser;
}  // namespace ROCKSDB_NAMESPACE
