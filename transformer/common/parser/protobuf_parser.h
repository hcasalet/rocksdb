#pragma once
// Shim — P2 portability refactor.  Implementation is in libmycelium (namespace mycelium).
#include "rocksdb/rocksdb_namespace.h"
#include "mycelium/protobuf_parser.h"
namespace ROCKSDB_NAMESPACE {
using mycelium::ProtobufParser;
}  // namespace ROCKSDB_NAMESPACE
