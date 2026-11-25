// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Symbol resolution off the UI thread (schematic_symbols.hpp). Owner: SHELL.

#include "hefesto/shell/schematic_symbols.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <set>
#include <string_view>
#include <thread>
#include <utility>

#include "hefesto/diag/sink.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"

namespace hefesto::shell {

namespace {

using SymbolMap = std::map<std::string, schem::Symbol, std::less<>>;

// What resolution reads of the document: its path and embedded blocks.
schem::Document resolution_source(const schem::Document& document) {
  schem::Document from;
  from.kind = document.kind;
  from.path = document.path;
  from.embedded = document.embedded;
  return from;
}

}  // namespace

std::vector<std::string> document_references(const schem::Document& document) {
  std::vector<std::string> out;
  std::set<std::string_view> seen;
  for (const schem::InstanceRecord& instance : document.instances) {
    if (seen.insert(instance.symbol).second) out.push_back(instance.symbol);
  }
  return out;
}

// Shared by the UI thread and the worker. `mutex` guards the job state and
// the result; the cache and the queued cache changes are the worker's: the
// UI thread queues them under `mutex` and only one job runs at a time
// (start_job joins the previous worker first).
struct SchematicSymbolResolver::Worker {
  std::mutex mutex;
  std::condition_variable idle;
  bool running = false;
  bool has_result = false;
  std::uint64_t result_generation = 0;
  SymbolMap result;
  std::vector<diag::Diagnostic> result_warnings;
  bool clear_queued = false;                // reload_symbols: the cache starts again
  std::vector<platform::Path> forget_queue;  // files saved since the last job
  bool forget_changed_queued = false;        // schematic reload: drop files changed on disk

  std::atomic<std::uint64_t> generation{0};  // a job of an older generation stops early
  std::atomic<bool> cancelled{false};
  std::thread thread;

  ResolveFn resolve;
  Notify notify;
  std::shared_ptr<platform::FileSystem> files;
  schem::SymbolCache cache;

  // Worker thread: the default ResolveFn, through the worker's own cache.
  schem::Symbol resolve_cached(const SymbolRequest& request, const std::string& reference,
                               std::vector<diag::Diagnostic>& warnings) {
    if (!files) return cache.placeholder();
    diag::CollectingSink sink;
    const schem::ResolveContext context{request.search_path, *files, &sink, request.xschem_compat};
    schem::Symbol symbol = cache.resolve(schem::Reference{reference}, request.from, context);
    for (diag::Diagnostic& d : sink.take()) warnings.push_back(std::move(d));
    return symbol;
  }

  // Worker thread: applies what the UI thread queued for the cache.
  void apply_cache_changes() {
    bool clear = false;
    bool changed = false;
    std::vector<platform::Path> forget;
    {
      const std::lock_guard guard(mutex);
      clear = std::exchange(clear_queued, false);
      changed = std::exchange(forget_changed_queued, false);
      forget.swap(forget_queue);
    }
    if (clear) cache.clear();
    for (const platform::Path& file : forget) cache.forget(file);
    if (changed && files) (void)cache.forget_changed(*files);
  }

  void run(const SymbolRequest& request, std::uint64_t job_generation) {
    apply_cache_changes();
    SymbolMap out;
    std::vector<diag::Diagnostic> warnings;
    for (const std::string& reference : request.references) {
      if (cancelled.load() || generation.load() != job_generation) break;  // stale: dropped anyway
      out.insert_or_assign(reference, resolve ? resolve(request, reference, warnings)
                                              : resolve_cached(request, reference, warnings));
    }
    {
      const std::lock_guard guard(mutex);
      result = std::move(out);
      result_warnings = std::move(warnings);
      result_generation = job_generation;
      has_result = true;
      running = false;
    }
    idle.notify_all();
    if (notify && !cancelled.load()) notify();
  }
};

SchematicSymbolResolver::SchematicSymbolResolver(Options options) : worker_(std::make_shared<Worker>()) {
  worker_->resolve = std::move(options.resolve);
  worker_->notify = std::move(options.notify);
  worker_->files = std::move(options.files);
  pending_ = worker_->cache.placeholder();
  pending_.reference.clear();
}

SchematicSymbolResolver::~SchematicSymbolResolver() {
  worker_->cancelled.store(true);
  if (worker_->thread.joinable()) worker_->thread.join();
}

bool SchematicSymbolResolver::show(schem::DocumentId id, const schem::Document& document,
                                   const schem::SearchPath& search_path, bool xschem_compat) {
  Context context{id,
                  document.path,
                  document.disk_digest,
                  search_path.directories(),
                  search_path.aliases(),
                  xschem_compat};
  if (!has_context_ || !(context == context_)) {
    context_ = std::move(context);
    has_context_ = true;
    worker_->generation.store(++generation_);
    symbols_.clear();
  }
  references_ = document_references(document);
  const std::set<std::string_view> used(references_.begin(), references_.end());
  std::erase_if(symbols_, [&used](const auto& entry) { return !used.contains(entry.first); });
  return start_job(document, search_path);
}

void SchematicSymbolResolver::forget() {
  has_context_ = false;
  context_ = {};
  worker_->generation.store(++generation_);
  references_.clear();
  symbols_.clear();
}

void SchematicSymbolResolver::reload_symbols() {
  {
    const std::lock_guard guard(worker_->mutex);
    worker_->clear_queued = true;
    worker_->forget_queue.clear();
  }
  worker_->generation.store(++generation_);  // a running job read the old files
  symbols_.clear();
}

void SchematicSymbolResolver::forget_file(const platform::Path& file) {
  {
    const std::lock_guard guard(worker_->mutex);
    worker_->forget_queue.push_back(file);
  }
  worker_->generation.store(++generation_);
  std::erase_if(symbols_, [&file](const auto& entry) { return entry.second.file == file || entry.second.embedded; });
}

void SchematicSymbolResolver::forget_changed() {
  {
    const std::lock_guard guard(worker_->mutex);
    worker_->forget_changed_queued = true;
  }
  worker_->generation.store(++generation_);  // a running job may have read an old file
  symbols_.clear();
}

bool SchematicSymbolResolver::start_job(const schem::Document& document, const schem::SearchPath& search_path) {
  std::vector<std::string> missing;
  for (const std::string& reference : references_) {
    if (!symbols_.contains(reference)) missing.push_back(reference);
  }
  if (missing.empty()) return false;
  {
    const std::lock_guard guard(worker_->mutex);
    if (worker_->running || worker_->has_result) return false;  // the next show() after take_finished()
    worker_->running = true;
  }
  if (worker_->thread.joinable()) worker_->thread.join();  // a finished job: returns at once
  SymbolRequest request{resolution_source(document), search_path, context_.xschem_compat, std::move(missing)};
  Worker* worker = worker_.get();
  worker_->thread = std::thread([worker, request = std::move(request), job = generation_] { worker->run(request, job); });
  return true;
}

bool SchematicSymbolResolver::take_finished() {
  SymbolMap result;
  std::vector<diag::Diagnostic> warnings;
  std::uint64_t job_generation = 0;
  {
    const std::lock_guard guard(worker_->mutex);
    if (!worker_->has_result) return false;
    result.swap(worker_->result);
    warnings.swap(worker_->result_warnings);
    job_generation = worker_->result_generation;
    worker_->has_result = false;
  }
  if (worker_->thread.joinable()) worker_->thread.join();
  if (job_generation != generation_) return true;  // another document or context: dropped
  const std::set<std::string_view> used(references_.begin(), references_.end());
  for (auto& [reference, symbol] : result) {
    if (used.contains(reference)) symbols_.insert_or_assign(reference, std::move(symbol));
  }
  for (diag::Diagnostic& d : warnings) warnings_.push_back(std::move(d));
  return true;
}

std::vector<diag::Diagnostic> SchematicSymbolResolver::take_warnings() {
  return std::exchange(warnings_, {});
}

SymbolLookup SchematicSymbolResolver::lookup() const {
  return [this](const schem::InstanceRecord& instance) -> const schem::Symbol& {
    const auto it = symbols_.find(instance.symbol);
    return it == symbols_.end() ? pending_ : it->second;
  };
}

bool SchematicSymbolResolver::running() const {
  const std::lock_guard guard(worker_->mutex);
  return worker_->running;
}

std::size_t SchematicSymbolResolver::pending_count() const {
  return static_cast<std::size_t>(std::count_if(references_.begin(), references_.end(),
                                                [this](const std::string& r) { return !symbols_.contains(r); }));
}

void SchematicSymbolResolver::wait() {
  std::unique_lock lock(worker_->mutex);
  worker_->idle.wait(lock, [this] { return !worker_->running; });
}

}  // namespace hefesto::shell
