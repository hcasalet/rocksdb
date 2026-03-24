// arrow_compaction_batcher.h
//
// DEPRECATED — Arrow has been removed from the per-record compaction hot path.
//
// The intermediate representation for compaction-time transformation is now
// mycelium::ParsedRow (see src/libmycelium/include/mycelium/transformer.h).
// ParsedRow carries no Arrow allocator overhead and is designed for row-at-a-
// time processing inside the CompactionIterator loop.
//
// This file is intentionally empty.  It remains to avoid breaking any out-of-
// tree include paths that may reference it.  Delete once all consumers are
// updated.
#pragma once
