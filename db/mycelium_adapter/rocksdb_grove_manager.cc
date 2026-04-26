// db/mycelium_adapter/rocksdb_grove_manager.cc

#include "db/mycelium_adapter/rocksdb_grove_manager.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "mycelium/augmenter.h"   // for kIndexKeySep
#include "rocksdb/iterator.h"
#include "rocksdb/write_batch.h"

namespace ROCKSDB_NAMESPACE {

namespace {

// Scan an AUGMENTER secondary-index CF for all entries whose key ends with
// "$$$KEY$$$<primary_key>" and delete them.
//
// Index keys have the form: field0%%field1%%...$$$KEY$$$<primary_key>
// We cannot seek to them directly (the interesting part is a suffix, not a
// prefix), so we do a full iteration and collect every matching key first,
// then delete after releasing the iterator.
mycelium::Status DeleteIndexEntriesForPrimaryKey(
    DB* db, ColumnFamilyHandle* handle, std::string_view primary_key) {
  // Build the suffix we're looking for: "$$$KEY$$$<primary_key>"
  std::string suffix(mycelium::kIndexKeySep);
  suffix.append(primary_key.data(), primary_key.size());

  ReadOptions ro;
  ro.fill_cache = false;
  std::unique_ptr<Iterator> it(db->NewIterator(ro, handle));

  std::vector<std::string> to_delete;
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    const Slice k = it->key();
    if (k.size() >= suffix.size() &&
        std::memcmp(k.data() + k.size() - suffix.size(),
                    suffix.data(), suffix.size()) == 0) {
      to_delete.emplace_back(k.data(), k.size());
    }
  }

  mycelium::Status first_error = mycelium::Status::OK();
  for (const auto& del_key : to_delete) {
    Status s = db->Delete(WriteOptions(), handle, del_key);
    if (!s.ok()) {
      fprintf(stderr,
              "[RocksDBGroveManager] DeleteIndexEntries: failed for CF '%s': %s\n",
              handle->GetName().c_str(), s.ToString().c_str());
      if (first_error.ok()) {
        first_error = mycelium::Status::Error(s.ToString());
      }
    }
  }
  return first_error;
}

}  // anonymous namespace

RocksDBGroveManager::RocksDBGroveManager(
    DB*                              db,
    std::vector<ColumnFamilyHandle*> derived_handles)
    : db_(db), derived_handles_(std::move(derived_handles)) {}

mycelium::Status RocksDBGroveManager::PropagateDelete(std::string_view key) {
  Slice key_slice(key.data(), key.size());
  mycelium::Status first_error = mycelium::Status::OK();

  for (ColumnFamilyHandle* handle : derived_handles_) {
    const std::string& cf_name = handle->GetName();
    if (cf_name.find("_secondary_index_cf") != std::string::npos) {
      // AUGMENTER secondary-index CF: keys are [fields]$$$KEY$$$<primary_key>.
      // A plain Delete(primary_key) would miss these entries — scan for suffix.
      mycelium::Status ms = DeleteIndexEntriesForPrimaryKey(db_, handle, key);
      if (!ms.ok() && first_error.ok()) first_error = ms;
    } else {
      // SPLIT / CONVERT / IDENTITY / _indexed_data_cf: key is the primary key.
      Status s = db_->Delete(WriteOptions(), handle, key_slice);
      if (!s.ok()) {
        // Log and continue: partial propagation is better than aborting.
        // Grove consistency can be restored via catch-up compaction.
        fprintf(stderr,
                "[RocksDBGroveManager] PropagateDelete: failed for CF '%s': %s\n",
                cf_name.c_str(), s.ToString().c_str());
        if (first_error.ok()) {
          first_error = mycelium::Status::Error(s.ToString());
        }
      }
    }
  }
  return first_error;
}

std::vector<std::string> RocksDBGroveManager::DerivedCFNames() const {
  std::vector<std::string> names;
  names.reserve(derived_handles_.size());
  for (const ColumnFamilyHandle* h : derived_handles_) {
    names.push_back(h->GetName());
  }
  return names;
}

}  // namespace ROCKSDB_NAMESPACE
