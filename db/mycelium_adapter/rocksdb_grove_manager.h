#pragma once
// db/mycelium_adapter/rocksdb_grove_manager.h
//
// RocksDB concrete implementation of mycelium::GroveManager.
//
// Wraps a DB* and the set of ColumnFamilyHandle* for all derived CFs in a
// grove.  PropagateDelete issues a Delete() into every derived CF so the grove
// stays tombstone-consistent when the user deletes a key from the base CF.
//
// Lifecycle: constructed once per MymBroker instance; the handles are valid
// for as long as the DB is open and the grove is live.

#include <string>
#include <string_view>
#include <vector>

#include "mycelium/compaction_hook.h"
#include "rocksdb/db.h"

namespace ROCKSDB_NAMESPACE {

class RocksDBGroveManager final : public mycelium::GroveManager {
 public:
  // db             — non-owning; must outlive this manager.
  // derived_handles — non-owning CF handles for every derived tree
  //                   (NOT including the base CF).
  RocksDBGroveManager(DB*                                  db,
                      std::vector<ColumnFamilyHandle*>     derived_handles);

  // Issue db->Delete(WriteOptions(), handle, key) for every derived CF.
  // Partial failures are logged to stderr and skipped; the first hard error
  // is returned to the caller.
  mycelium::Status PropagateDelete(std::string_view key) override;

  // Returns the name of each derived CF registered with this manager.
  std::vector<std::string> DerivedCFNames() const override;

 private:
  DB*                              db_;               // non-owning
  std::vector<ColumnFamilyHandle*> derived_handles_;  // non-owning
};

}  // namespace ROCKSDB_NAMESPACE
