#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/utilities/checkpoint.h>
#include <rocksdb/write_batch.h>
#include <rocksdb/slice.h>
#include <rocksdb/transaction_log.h>
#include <rocksdb/listener.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "transformer.h"

using Clock = std::chrono::steady_clock;

// -----------------------------
// Bounded blocking queue
// -----------------------------
template <typename T>
class BoundedQueue {
 public:
  explicit BoundedQueue(size_t capacity) : capacity_(capacity) {}

  bool Push(T item) {
    std::unique_lock<std::mutex> lk(mu_);
    cv_not_full_.wait(lk, [&] { return shutdown_ || q_.size() < capacity_; });
    if (shutdown_) return false;
    q_.push_back(std::move(item));
    cv_not_empty_.notify_one();
    return true;
  }

  bool Pop(T* out) {
    std::unique_lock<std::mutex> lk(mu_);
    cv_not_empty_.wait(lk, [&] { return shutdown_ || !q_.empty(); });
    if (q_.empty()) return false;  // shutdown
    *out = std::move(q_.front());
    q_.pop_front();
    cv_not_full_.notify_one();
    return true;
  }

  void Shutdown() {
    std::lock_guard<std::mutex> lk(mu_);
    shutdown_ = true;
    cv_not_empty_.notify_all();
    cv_not_full_.notify_all();
  }

  size_t Size() const {
    std::lock_guard<std::mutex> lk(mu_);
    return q_.size();
  }

 private:
    mutable std::mutex mu_;
    std::condition_variable cv_not_empty_;
    std::condition_variable cv_not_full_;
    std::deque<T> q_;
    size_t capacity_;
    bool shutdown_{false};
};

// -----------------------------
// A work item: a single WAL record (WriteBatch) + seq metadata
// TransactionLogIterator yields BatchResult containing writeBatchPtr.
// -----------------------------
struct WalItem {
  uint64_t seq = 0;
  std::unique_ptr<rocksdb::WriteBatch> batch;
};

// -----------------------------
// Simple metrics
// -----------------------------
struct Metrics {
  std::atomic<uint64_t> fg_puts{0};
  std::atomic<uint64_t> fg_errors{0};

  std::atomic<uint64_t> elt_records_in{0};   // number of WriteBatch records processed
  std::atomic<uint64_t> elt_kv_in{0};        // number of KV ops seen
  std::atomic<uint64_t> elt_kv_out{0};       // number of KV ops emitted
  std::atomic<uint64_t> elt_write_errors{0};

  std::atomic<uint64_t> wal_items_enqueued{0};
  std::atomic<uint64_t> wal_items_dropped{0};

  std::atomic<uint64_t> next_seq{0};
  std::atomic<uint64_t> last_seen_seq{0};
};

// -----------------------------
// WriteBatch handler to iterate over ops
// -----------------------------
class EltBatchHandler : public rocksdb::WriteBatch::Handler {
 public:
  EltBatchHandler(const TransformConfig& cfg,
                  rocksdb::ColumnFamilyHandle* out_cf,
                  rocksdb::WriteBatch* out_wb,
                  Metrics* m,
                  Transformer* transformer)
      : cfg_(cfg), out_cf_(out_cf), out_wb_(out_wb), m_(m), transformer_(transformer) {}

  rocksdb::Status PutCF(uint32_t cf_id, const rocksdb::Slice& key, const rocksdb::Slice& value) override {
    (void)cf_id;  // You may filter by cf_id if needed.
    m_->elt_kv_in.fetch_add(1, std::memory_order_relaxed);

    tmp_outs_.clear();
    transformer_->SplitRecords(cfg_, key, value, &tmp_outs_);
    for (auto& kv : tmp_outs_) {
      out_wb_->Put(out_cf_, kv.first, kv.second);
      m_->elt_kv_out.fetch_add(1, std::memory_order_relaxed);
    }
    return rocksdb::Status::OK();
  }

  rocksdb::Status DeleteCF(uint32_t cf_id, const rocksdb::Slice& key) override {
    (void)cf_id;
    // Optional: propagate deletes or ignore.
    // If you ignore deletes, output CF may drift vs input semantics; fine for overhead tests.
    m_->elt_kv_in.fetch_add(1, std::memory_order_relaxed);
    return rocksdb::Status::OK();
  }

 private:
  const TransformConfig& cfg_;
  rocksdb::ColumnFamilyHandle* out_cf_;
  rocksdb::WriteBatch* out_wb_;
  Metrics* m_;
  Transformer* transformer_;
  std::vector<std::pair<std::string, std::string>> tmp_outs_;
};

// -----------------------------
// WAL Tailer: reads updates since next_seq and enqueues WalItems
// -----------------------------
class WalTailer {
 public:
  WalTailer(rocksdb::DB* db,
            Metrics* metrics,
            BoundedQueue<WalItem>* q,
            std::chrono::milliseconds poll_backoff = std::chrono::milliseconds(10))
      : db_(db), m_(metrics), q_(q), poll_backoff_(poll_backoff) {}

  void Start() {
    stop_.store(false);
    th_ = std::thread([this] { Run(); });
  }

  void Stop() {
    stop_.store(true);
    if (th_.joinable()) th_.join();
  }

 private:
  void Run() {
    while (!stop_.load()) {
      const uint64_t seq = m_->next_seq.load(std::memory_order_acquire);

      std::unique_ptr<rocksdb::TransactionLogIterator> it;
      rocksdb::Status s = db_->GetUpdatesSince(seq, &it);
      if (!s.ok()) {
        // Common in early startup if seq is too old and WAL has been recycled.
        // For a quick scaffold, just backoff and retry; in production you'd resync via GetLatestSequenceNumber().
        std::this_thread::sleep_for(poll_backoff_);
        continue;
      }

      bool progressed = false;
      for (; it->Valid() && !stop_.load(); it->Next()) {
        rocksdb::Status is = it->status();
        if (!is.ok()) break;

        rocksdb::BatchResult br = it->GetBatch();
        m_->last_seen_seq.store(br.sequence, std::memory_order_release);

        WalItem item;
        item.seq = br.sequence;

        // Copy the WriteBatch because the iterator owns the underlying.
        // This is fine for a quick harness; you can optimize later (e.g., serialize and pass bytes).
        item.batch = std::make_unique<rocksdb::WriteBatch>(*br.writeBatchPtr);

        if (!q_->Push(std::move(item))) {
          // queue shutdown
          return;
        }
        m_->wal_items_enqueued.fetch_add(1, std::memory_order_relaxed);

        // Advance next_seq optimistically by count.
        // Note: br.writeBatchPtr->Count() exists; use it to compute next seq.
        m_->next_seq.store(br.sequence + br.writeBatchPtr->Count(), std::memory_order_release);
        progressed = true;
      }

      if (!progressed) {
        std::this_thread::sleep_for(poll_backoff_);
      }
    }
  }

  rocksdb::DB* db_;
  Metrics* m_;
  BoundedQueue<WalItem>* q_;
  std::chrono::milliseconds poll_backoff_;
  std::atomic<bool> stop_{false};
  std::thread th_;
};

// -----------------------------
// ELT worker: pops WalItems, transforms, writes to output CF
// -----------------------------
class EltWorkerPool {
 public:
  struct Config {
    int num_workers = 2;
    size_t batch_count_limit = 1000;
    size_t batch_bytes_limit = 1 << 20;  // 1MB
  };

  EltWorkerPool(rocksdb::DB* db,
                rocksdb::ColumnFamilyHandle* out_cf,
                const TransformConfig& tcfg,
                const Config& cfg,
                Metrics* metrics,
                Transformer* transformer,
                BoundedQueue<WalItem>* q)
      : db_(db), out_cf_(out_cf), tcfg_(tcfg), cfg_(cfg), 
        m_(metrics), transformer_(transformer), q_(q) {}

  void Start() {
    stop_.store(false);
    for (int i = 0; i < cfg_.num_workers; i++) {
      workers_.emplace_back([this, i] { WorkerLoop(i); });
    }
  }

  void Stop() {
    stop_.store(true);
    q_->Shutdown();
    for (auto& t : workers_) {
      if (t.joinable()) t.join();
    }
  }

 private:
  void WorkerLoop(int worker_id) {
    (void)worker_id;

    rocksdb::WriteOptions wopt;
    // For overhead testing, you typically want WAL enabled for output too (default).
    // You can set disableWAL=true if you want to shift cost away (but note semantics).
    // wopt.disableWAL = true;

    rocksdb::WriteBatch out_wb;
    size_t out_count = 0;
    size_t out_bytes = 0;

    auto flush_out = [&]() {
      if (out_count == 0) return;
      rocksdb::Status ws = db_->Write(wopt, &out_wb);
      if (!ws.ok()) {
        m_->elt_write_errors.fetch_add(1, std::memory_order_relaxed);
      }
      out_wb.Clear();
      out_count = 0;
      out_bytes = 0;
    };

    WalItem item;
    while (!stop_.load()) {
      if (!q_->Pop(&item)) {
        // shutdown
        flush_out();
        return;
      }

      m_->elt_records_in.fetch_add(1, std::memory_order_relaxed);

      EltBatchHandler handler(tcfg_, out_cf_, &out_wb, m_, transformer_);
      rocksdb::Status hs = item.batch->Iterate(&handler);
      if (!hs.ok()) {
        // If iterate fails, skip this batch.
        continue;
      }

      // Estimate growth; WriteBatch doesn't expose exact bytes cheaply.
      // Quick heuristic: count ops. If you want exact, track sizes in handler.
      out_count += static_cast<size_t>(item.batch->Count());
      out_bytes += static_cast<size_t>(item.batch->Count()) * 64;  // heuristic

      if (out_count >= cfg_.batch_count_limit || out_bytes >= cfg_.batch_bytes_limit) {
        flush_out();
      }
    }

    flush_out();
  }

  rocksdb::DB* db_;
  rocksdb::ColumnFamilyHandle* out_cf_;
  TransformConfig tcfg_;
  Config cfg_;
  Metrics* m_;
  Transformer* transformer_;
  BoundedQueue<WalItem>* q_;
  std::atomic<bool> stop_{false};
  std::vector<std::thread> workers_;
};

// -----------------------------
// Foreground ingestion loop (simple)
// -----------------------------
static inline uint64_t XorShift64Star(uint64_t* x) {
  uint64_t v = *x;
  v ^= v >> 12;
  v ^= v << 25;
  v ^= v >> 27;
  *x = v;
  return v * 2685821657736338717ULL;
}

struct JsonValueGenConfig {
  int num_cols = 10;
  int col_size = 10;          // bytes per column value (string payload)
  bool numeric_strings = true; // "0123456789" style values
  bool include_id_field = false; // e.g., {"id":"t1:...","col0":...}
};

static inline void AppendFixedDigits(std::string* out, uint64_t* rng, int len) {
  // appends exactly len chars [0-9]
  out->resize(out->size() + len);
  char* p = out->data() + (out->size() - len);
  for (int i = 0; i < len; ++i) {
    uint64_t r = XorShift64Star(rng);
    p[i] = static_cast<char>('0' + (r % 10));
  }
}

static inline void BuildJsonValue(std::string* out,
                                  const JsonValueGenConfig& cfg,
                                  uint64_t* rng,
                                  const rocksdb::Slice* optional_id /*nullable*/) {
  out->clear();

  // Rough reserve to avoid reallocs.
  // Each field: "colX":"<val>", plus braces/commas.
  out->reserve(static_cast<size_t>(cfg.num_cols) * (cfg.col_size + 16) + 32);

  out->push_back('{');

  bool first = true;
  if (cfg.include_id_field && optional_id != nullptr) {
    // Assumes id has no quotes; if it might, escape it.
    out->append("\"id\":\"");
    out->append(optional_id->data(), optional_id->size());
    out->append("\"");
    first = false;
  }

  for (int c = 0; c < cfg.num_cols; ++c) {
    if (!first) out->push_back(',');
    first = false;

    out->append("\"col");
    out->append(std::to_string(c));
    out->append("\":\"");

    if (cfg.numeric_strings) {
      AppendFixedDigits(out, rng, cfg.col_size);
    } else {
      // simple alpha payload; keep it deterministic if desired
      out->resize(out->size() + cfg.col_size);
      char* p = out->data() + (out->size() - cfg.col_size);
      for (int i = 0; i < cfg.col_size; ++i) {
        uint64_t r = XorShift64Star(rng);
        p[i] = static_cast<char>('a' + (r % 26));
      }
    }

    out->append("\"");
  }

  out->push_back('}');
}

static void ForegroundIngest(rocksdb::DB* db,
                            rocksdb::ColumnFamilyHandle* in_cf,
                            Metrics* m,
                            std::atomic<bool>* stop_flag,
                            JsonValueGenConfig& vcfg,
                            int thread_id) {
  rocksdb::WriteOptions wopt;

  uint64_t i = 0;
  const std::string prefix = "t" + std::to_string(thread_id) + ":";
  uint64_t key_rng = 0x9e3779b97f4a7c15ULL ^ (static_cast<uint64_t>(thread_id) + 1);
  uint64_t val_rng = 0xd1b54a32d192ed03ULL ^ (static_cast<uint64_t>(thread_id) + 101);

  std::string key, val;
  key.reserve(prefix.size() + 16 /*hex*/);

  while (!stop_flag->load(std::memory_order_relaxed)) {
    uint64_t r = XorShift64Star(&key_rng);

    key.clear();
    key.append(prefix);

    char buf[16];
    static const char* hexd = "0123456789abcdef";
    for (int j = 15; j >= 0; --j) {
      buf[j] = hexd[r & 0xF];
      r >>= 4;
    }
    key.append(buf, 16);

    BuildJsonValue(&val, vcfg, &val_rng, nullptr);

    rocksdb::Status s = db->Put(wopt, in_cf, key, val);
    if (s.ok()) m->fg_puts.fetch_add(1, std::memory_order_relaxed);
    else m->fg_errors.fetch_add(1, std::memory_order_relaxed);

    i++;
  }
}

// -----------------------------
// Metrics printer
// -----------------------------
static void PrintMetricsLoop(Metrics* m,
                             rocksdb::DB* db,
                             std::atomic<bool>* stop_flag) {
  using namespace std::chrono_literals;

  uint64_t last_fg = 0;
  uint64_t last_elt_in = 0;
  auto last_t = Clock::now();
  uint64_t total_fg = 0;
  uint64_t total_t = 0;

  while (!stop_flag->load()) {
    std::this_thread::sleep_for(1s);

    auto now = Clock::now();
    double dt = std::chrono::duration<double>(now - last_t).count();
    last_t = now;

    uint64_t fg = m->fg_puts.load();
    uint64_t elt_in = m->elt_records_in.load();
    uint64_t kv_in = m->elt_kv_in.load();
    uint64_t kv_out = m->elt_kv_out.load();

    uint64_t dfg = fg - last_fg;
    uint64_t delt = elt_in - last_elt_in;
    last_fg = fg;
    last_elt_in = elt_in;

    uint64_t next_seq = m->next_seq.load();
    uint64_t last_seen = m->last_seen_seq.load();
    uint64_t lag = (last_seen > next_seq) ? (last_seen - next_seq) : 0;

    // Pull a few useful RocksDB stats quickly.
    std::string stats;
    db->GetProperty("rocksdb.stats", &stats);

    std::cout
      << "[1s] fg_puts/s=" << (dfg / dt)
      << " elt_batches/s=" << (delt / dt)
      << " kv_in=" << kv_in
      << " kv_out=" << kv_out
      << " wal_lag_seq=" << lag
      << " q_depth=" << "?"
      << " fg_err=" << m->fg_errors.load()
      << " elt_write_err=" << m->elt_write_errors.load()
      << "\n";

    // Optional: print condensed stats occasionally (rocksdb.stats is verbose).
    // std::cout << stats << "\n";
    total_fg += dfg;
    total_t += dt;
  }
  std::cout <<"\n average puts/s=" << (total_fg / total_t) << std::endl;
}

// -----------------------------
// Main: open DB with 2 CFs: "in" and "elt_out"
// -----------------------------
int main(int argc, char** argv) {
  int fg_threads = 1;
  if (argc > 1) fg_threads = std::stoi(argv[1]);
  int run_seconds = 30;
  if (argc > 2) run_seconds = std::stoi(argv[2]);
  std::string db_path = "/tmp/rocksdb_elt_test";
  if (argc > 3) db_path = argv[3];

  rocksdb::Options options;
  options.create_if_missing = true;
  options.create_missing_column_families = true;

  // Keep WAL around longer so ELT doesn't fall behind and lose history.
  // For quick experiments, increase these if you see "seq too old" behavior.
  options.keep_log_file_num = 20;
  options.max_log_file_size = 64 * 1024 * 1024;  // 64MB

  // Parallelism preparation for concurrent ingestion
  options.IncreaseParallelism(12);
  options.max_background_jobs = 212;
  options.write_buffer_size = 32 * 1024 * 1024;
  options.max_write_buffer_number = 8;
  options.level0_file_num_compaction_trigger = 4;
  options.level0_slowdown_writes_trigger = 10;
  options.level0_stop_writes_trigger = 20;
  options.max_total_wal_size = 2ULL * 1024 * 1024 * 1024;
  options.keep_log_file_num = 20;
  options.recycle_log_file_num = 0;

  rocksdb::WriteOptions wopts;
  wopts.disableWAL = false;

  // CFs
  std::vector<rocksdb::ColumnFamilyDescriptor> cf_descs;
  cf_descs.emplace_back(rocksdb::kDefaultColumnFamilyName, rocksdb::ColumnFamilyOptions());
  cf_descs.emplace_back("in", rocksdb::ColumnFamilyOptions());
  cf_descs.emplace_back("elt_out", rocksdb::ColumnFamilyOptions());

  rocksdb::DB* db_raw = nullptr;
  std::vector<rocksdb::ColumnFamilyHandle*> handles;
  rocksdb::Status s = rocksdb::DB::Open(options, db_path, cf_descs, &handles, &db_raw);
  if (!s.ok()) {
    std::cerr << "DB::Open failed: " << s.ToString() << "\n";
    return 1;
  }
  std::unique_ptr<rocksdb::DB> db(db_raw);

  rocksdb::ColumnFamilyHandle* in_cf = nullptr;
  rocksdb::ColumnFamilyHandle* out_cf = nullptr;
  for (auto* h : handles) {
    if (h->GetName() == "in") in_cf = h;
    if (h->GetName() == "elt_out") out_cf = h;
  }
  if (!in_cf || !out_cf) {
    std::cerr << "Missing CF handles.\n";
    return 1;
  }

  Metrics metrics;
  // Start tailing at current head to avoid processing historical data.
  // If you want historical replay, set this to 0 and ensure WAL retention is sufficient.
  metrics.next_seq.store(db->GetLatestSequenceNumber());

  TransformConfig tcfg;
  tcfg.splits = 10;
  Transformer transformer(tcfg);

  BoundedQueue<WalItem> q(/*capacity=*/1024);

  WalTailer tailer(db.get(), &metrics, &q);

  EltWorkerPool::Config wcfg;
  wcfg.num_workers = 2;               // set 1 if you want strict ordering
  wcfg.batch_count_limit = 2000;
  wcfg.batch_bytes_limit = 2 << 20;   // 2MB

  EltWorkerPool pool(db.get(), out_cf, tcfg, wcfg, &metrics, &transformer, &q);

  std::atomic<bool> stop{false};

  // Start background ELT
  pool.Start();
  tailer.Start();

  // Generating values to ingest configuration
  JsonValueGenConfig vcfg;
  vcfg.num_cols = 10;
  vcfg.col_size = 16;
  vcfg.numeric_strings = true;
  vcfg.include_id_field = false;

  // Start foreground ingest + metrics thread
  std::vector<std::thread> fg;
  fg.reserve(fg_threads);

  for (int t = 0; t < fg_threads; ++t) {
    fg.emplace_back([&, t] {
      ForegroundIngest(db.get(), in_cf, &metrics, &stop, vcfg, t);
    });
  }

  std::thread mt([&] { PrintMetricsLoop(&metrics, db.get(), &stop); });

  // Run for a fixed duration (quick harness).
  std::this_thread::sleep_for(std::chrono::seconds(run_seconds));
  stop.store(true);

  for (auto& th : fg) th.join();
  mt.join();

  tailer.Stop();
  pool.Stop();

  for (auto* h : handles) {
    db->DestroyColumnFamilyHandle(h);
  }

  return 0;
}