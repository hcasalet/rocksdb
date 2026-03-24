#pragma once
// Shim — P2 portability refactor.  Implementation is in libmycelium (namespace mycelium).
#include "rocksdb/rocksdb_namespace.h"
#include "mycelium/csv_parser.h"
namespace ROCKSDB_NAMESPACE {
using mycelium::CsvParserOptions;
using mycelium::CsvParser;
}  // namespace ROCKSDB_NAMESPACE
