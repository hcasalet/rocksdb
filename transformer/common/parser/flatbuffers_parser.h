#pragma once
// Shim — P2 portability refactor.  Implementation is in libmycelium (namespace mycelium).
#include "rocksdb/rocksdb_namespace.h"
#include "mycelium/flatbuffers_parser.h"
namespace ROCKSDB_NAMESPACE {
using mycelium::FlatbufPayload;
using mycelium::FlatbuffersParser;
}  // namespace ROCKSDB_NAMESPACE
