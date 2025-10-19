## Mycelium: A Work-hiding LSM-tree that transforms data inline with LSM maintenance

Mycelium is built on top of RocksDB.

Mycelium exposes a programmable, compaction-time substrate (mroutines) that can split rows into column groups, 
convert formats, and augment datasets with auxiliary structures such as secondary indexes; it orchestrates these 
within a logical LSM “grove” so transformations can be chained while preserving the standard API and operational model. 
Mycelium improves read performance—delivering multi-x speedups on analytic queries—while keeping write-path overhead 
modest, and it accelerates index queries by orders of magnitude compared to a baseline without native secondary indexes, 
all while reusing compaction I/O and exploiting otherwise idle CPU slack.

## License

RocksDB is dual-licensed under both the GPLv2 (found in the COPYING file in the root directory) and Apache 2.0 License (found in the LICENSE.Apache file in the root directory).  You may select, at your option, one of the above-listed licenses.
