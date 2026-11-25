// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// sim::JobManager (task 5.5): the job queue, the worker pool and the fault
// policy of D-25 (a) over the Engine interface. See job_manager.hpp for the
// event rules.
//
// Owner: SIMENG.

#include "hefesto/sim/engine/job_manager.hpp"

#include <algorithm>
#include <ctime>
#include <deque>
#include <map>
#include <mutex>
#include <string_view>
#include <utility>

#include "sim/engine/job_manager_events.hpp"

namespace hefesto::sim {

namespace {

constexpr std::size_t kMaxErrorLines = 50;  // "Cancellation and fault states": the last 50 lines
constexpr std::string_view kStderrPrefix = "stderr ";

std::string timestamp(std::chrono::system_clock::time_point when) {
  const std::time_t t = std::chrono::system_clock::to_time_t(when);
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &t);
#else
  localtime_r(&t, &local);
#endif
  char buffer[32];
  const std::size_t n = std::strftime(buffer, sizeof buffer, "%Y%m%d-%H%M%S", &local);
  return std::string(buffer, n);
}

bool is_active(JobState state) noexcept {
  return state == JobState::loading || state == JobState::running || state == JobState::paused;
}

}  // namespace

struct JobManager::Impl {
  struct Slot {
    std::unique_ptr<Engine> engine;
    std::uint64_t generation = 0;  // of `engine`; 0 = none
    std::uint64_t engine_number = 0;
    std::optional<std::size_t> job;
    bool saw_running = false;
    bool cancel_requested = false;
  };

  JobManagerOptions options;
  std::uint32_t workers = 1;
  std::optional<diag::Diagnostic> pool_diagnostic;
  std::deque<JobRecord> records;  // stable addresses for record()
  std::deque<PreparedDeck> decks;   // by record index; released once loaded
  std::map<JobId, std::size_t, std::less<>> index;
  std::deque<std::size_t> queue;
  std::vector<Slot> slots;
  EventQueue events;
  std::uint64_t generations = 0;
  std::uint64_t engines = 0;
  std::size_t active = 0;  // jobs in loading, running or paused
  std::size_t peak = 0;

  explicit Impl(JobManagerOptions opts) : options(std::move(opts)) {
    auto size = worker_pool_size(options.pool);
    if (size.ok()) {
      workers = size.value();
    } else {
      workers = max_worker_pool_size(options.pool);
      pool_diagnostic = size.error();
    }
    slots.resize(workers);
  }

  ~Impl() {
    // Engines first: their reader threads may still post events.
    for (Slot& slot : slots) {
      slot.engine.reset();
    }
  }

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(Impl&&) = delete;

  [[nodiscard]] std::chrono::system_clock::time_point now() const {
    return options.now ? options.now() : std::chrono::system_clock::now();
  }

  [[nodiscard]] diag::Outcome<JobId> make_id(const JobOptions& opts) const {
    if (opts.run_set_point) {
      if (index.contains(*opts.run_set_point)) {
        return diag::make("SIM-ENGINE-036", "job " + *opts.run_set_point + " already exists");
      }
      return *opts.run_set_point;
    }
    const JobId base = timestamp(now()) + "-" + opts.testbench + "-" + opts.corner;
    JobId id = base;
    for (std::size_t n = 2; index.contains(id); ++n) {
      id = base + "-" + std::to_string(n);
    }
    return id;
  }

  void set_state(JobRecord& rec, JobState state) {
    if (is_active(rec.job.state)) {
      --active;
    }
    if (is_active(state)) {
      ++active;
    }
    rec.job.state = state;
    peak = std::max(peak, active);
  }

  [[nodiscard]] std::optional<std::size_t> slot_of(std::size_t job) const {
    for (std::size_t i = 0; i < slots.size(); ++i) {
      if (slots[i].job == job) {
        return i;
      }
    }
    return std::nullopt;
  }

  // --- starting jobs ---

  bool ensure_engine(std::size_t s, JobRecord& rec) {
    Slot& slot = slots[s];
    if (slot.engine) {
      return true;
    }
    diag::Outcome<std::unique_ptr<Engine>> made =
        options.make_engine ? options.make_engine()
                            : diag::Outcome<std::unique_ptr<Engine>>{diag::make("SIM-ENGINE-031", "no engine host")};
    if (!made.ok() || made.value() == nullptr) {
      const std::string reason = made.ok() ? std::string{"the engine host returned no engine"} : made.error().text;
      rec.diagnostics.push_back(diag::make("SIM-ENGINE-031", "simulation engine unavailable: " + reason));
      set_state(rec, JobState::failed);
      return false;
    }
    slot.engine = std::move(made.value());
    slot.generation = ++generations;
    slot.engine_number = ++engines;
    return true;
  }

  void connect(std::size_t s) {
    const std::uint64_t generation = slots[s].generation;
    EventQueue* queue_ptr = &events;
    EngineCallbacks callbacks;
    callbacks.on_output = [queue_ptr, s, generation](std::string_view line) {
      queue_ptr->post(EngineEvent{s, generation, EngineEvent::Kind::output, std::string{line}, EngineState::idle});
    };
    callbacks.on_state = [queue_ptr, s, generation](EngineState state) {
      queue_ptr->post(EngineEvent{s, generation, EngineEvent::Kind::state, {}, state});
    };
    slots[s].engine->set_callbacks(std::move(callbacks));
  }

  void start(std::size_t s, std::size_t job) {
    JobRecord& rec = records[job];
    if (!ensure_engine(s, rec)) {
      decks[job] = PreparedDeck{};
      return;
    }
    Slot& slot = slots[s];
    slot.job = job;
    slot.saw_running = false;
    slot.cancel_requested = false;
    rec.engine = slot.engine_number;
    connect(s);
    set_state(rec, JobState::preparing);
    set_state(rec, JobState::loading);
    const std::vector<std::string> lines = std::move(decks[job].lines);
    decks[job] = PreparedDeck{};
    diag::Status loaded = slot.engine->load(lines);
    if (!loaded.ok()) {
      fault(s, loaded.error());
      return;
    }
    diag::Status ran = slot.engine->run();
    if (!ran.ok()) {
      fault(s, ran.error());
    }
  }

  std::size_t start_jobs() {
    std::size_t started = 0;
    std::size_t s = 0;
    while (s < slots.size() && !queue.empty()) {
      if (slots[s].job) {
        ++s;
        continue;
      }
      const std::size_t job = queue.front();
      queue.pop_front();
      start(s, job);
      ++started;
      if (slots[s].job) {
        ++s;  // otherwise the job ended before it kept the worker: offer the worker again
      }
    }
    return started;
  }

  // --- ending jobs ---

  void append_error_lines(JobRecord& rec, bool only_errors) {
    for (const std::string& line : rec.errors) {
      if (!only_errors || line.starts_with("Error")) {
        rec.diagnostics.push_back(diag::make("SIM-ENGINE-032", line));
      }
    }
  }

  /// D-25 (a): the job fails, the engine is discarded and the worker gets a
  /// new engine for its next job.
  void fault(std::size_t s, std::optional<diag::Diagnostic> cause) {
    Slot& slot = slots[s];
    if (slot.job) {
      JobRecord& rec = records[*slot.job];
      rec.diagnostics.push_back(diag::make("SIM-ENGINE-030", "simulation engine terminated"));
      if (cause) {
        rec.diagnostics.push_back(std::move(*cause));
      }
      append_error_lines(rec, false);
      set_state(rec, JobState::failed);
    }
    slot.engine.reset();
    slot.generation = 0;
    slot.job.reset();
  }

  void finish(std::size_t s) {
    Slot& slot = slots[s];
    JobRecord& rec = records[*slot.job];
    diag::Outcome<std::vector<Vector>> vectors = slot.engine->vectors();
    if (!vectors.ok()) {
      fault(s, vectors.error());
      return;
    }
    rec.vectors = std::move(vectors.value());
    const bool error_line = std::any_of(rec.errors.begin(), rec.errors.end(),
                                        [](const std::string& line) { return line.starts_with("Error"); });
    if (slot.cancel_requested) {
      set_state(rec, JobState::cancelled);
    } else if (error_line) {
      append_error_lines(rec, true);
      set_state(rec, JobState::failed);
    } else {
      set_state(rec, JobState::completed);
    }
    slot.job.reset();  // a healthy engine is reused
  }

  // --- events ---

  void on_output(Slot& slot, std::string line) {
    JobRecord& rec = records[*slot.job];
    if (line.starts_with(kStderrPrefix)) {
      line.erase(0, kStderrPrefix.size());
      rec.errors.push_back(line);
      if (rec.errors.size() > kMaxErrorLines) {
        rec.errors.erase(rec.errors.begin());
      }
    }
    rec.log.push_back(std::move(line));
  }

  void on_state(std::size_t s, EngineState state) {
    Slot& slot = slots[s];
    JobRecord& rec = records[*slot.job];
    switch (state) {
      case EngineState::running:
        slot.saw_running = true;
        set_state(rec, JobState::running);
        break;
      case EngineState::paused:
        if (rec.job.state == JobState::running) {
          set_state(rec, JobState::paused);
        }
        break;
      case EngineState::idle:
      case EngineState::loaded:
        if (slot.saw_running) {
          finish(s);
        }
        break;
      case EngineState::failed:
        fault(s, std::nullopt);
        break;
    }
  }

  void apply(EngineEvent event) {
    if (event.slot >= slots.size() || slots[event.slot].generation != event.generation) {
      return;  // an event of a discarded engine
    }
    Slot& slot = slots[event.slot];
    if (!slot.job) {
      if (event.kind == EngineEvent::Kind::state && event.state == EngineState::failed) {
        fault(event.slot, std::nullopt);  // an idle engine that failed is replaced too
      }
      return;
    }
    if (event.kind == EngineEvent::Kind::output) {
      on_output(slot, std::move(event.line));
    } else {
      on_state(event.slot, event.state);
    }
  }

  std::size_t apply_events() {
    std::vector<EngineEvent> batch = events.take();
    for (EngineEvent& event : batch) {
      apply(std::move(event));
    }
    return batch.size();
  }

  // --- commands ---

  diag::Outcome<std::size_t> find(const JobId& id) const {
    const auto it = index.find(id);
    if (it == index.end()) {
      return diag::make("SIM-ENGINE-034", "no job " + id);
    }
    return it->second;
  }

  diag::Status command(const JobId& id, JobState required, diag::Status (Engine::*call)()) {
    const auto job = find(id);
    if (!job.ok()) {
      return job.error();
    }
    const auto s = slot_of(job.value());
    if (!s || records[job.value()].job.state != required) {
      return diag::make("SIM-ENGINE-035", "job " + id + " is not " + (required == JobState::paused ? "paused" : "running"));
    }
    diag::Status status = (slots[*s].engine.get()->*call)();
    if (!status.ok()) {
      fault(*s, status.error());
    }
    return status;
  }

  diag::Status cancel(const JobId& id) {
    const auto job = find(id);
    if (!job.ok()) {
      return job.error();
    }
    JobRecord& rec = records[job.value()];
    if (rec.job.state == JobState::queued) {
      std::erase(queue, job.value());
      decks[job.value()] = PreparedDeck{};
      set_state(rec, JobState::cancelled);
      return diag::Done{};
    }
    const auto s = slot_of(job.value());
    if (!s) {
      return diag::make("SIM-ENGINE-035", "job " + id + " is not running");
    }
    slots[*s].cancel_requested = true;
    diag::Status status = slots[*s].engine->cancel();
    if (!status.ok()) {
      fault(*s, status.error());
    }
    return status;
  }
};

JobManager::JobManager() : JobManager(JobManagerOptions{}) {}

JobManager::JobManager(JobManagerOptions options) : impl_(std::make_unique<Impl>(std::move(options))) {}

JobManager::~JobManager() = default;
JobManager::JobManager(JobManager&&) noexcept = default;
JobManager& JobManager::operator=(JobManager&&) noexcept = default;

diag::Outcome<JobId> JobManager::submit(PreparedDeck deck, JobOptions options) {
  auto id = impl_->make_id(options);
  if (!id.ok()) {
    return id.error();
  }
  JobRecord rec;
  rec.job.id = id.value();
  rec.job.state = JobState::queued;
  rec.job.names = std::make_shared<const NetNameMap>(std::move(deck.names));
  rec.options = std::move(options);
  const std::size_t n = impl_->records.size();
  impl_->records.push_back(std::move(rec));
  impl_->decks.push_back(std::move(deck));
  impl_->index.emplace(id.value(), n);
  impl_->queue.push_back(n);
  return id;
}

std::size_t JobManager::poll() {
  std::size_t total = 0;
  for (;;) {
    const std::size_t n = impl_->apply_events() + impl_->start_jobs();
    if (n == 0) {
      return total;
    }
    total += n;
  }
}

diag::Status JobManager::cancel(const JobId& id) {
  return impl_->cancel(id);
}

diag::Status JobManager::pause(const JobId& id) {
  return impl_->command(id, JobState::running, &Engine::pause);
}

diag::Status JobManager::resume(const JobId& id) {
  return impl_->command(id, JobState::paused, &Engine::resume);
}

const JobRecord* JobManager::record(const JobId& id) const {
  const auto it = impl_->index.find(id);
  return it == impl_->index.end() ? nullptr : &impl_->records[it->second];
}

std::optional<JobState> JobManager::state(const JobId& id) const {
  const JobRecord* rec = record(id);
  return rec != nullptr ? std::optional<JobState>{rec->job.state} : std::nullopt;
}

std::uint32_t JobManager::workers() const noexcept {
  return impl_->workers;
}

const std::optional<diag::Diagnostic>& JobManager::pool_diagnostic() const noexcept {
  return impl_->pool_diagnostic;
}

std::size_t JobManager::active() const noexcept {
  return impl_->active;
}

std::size_t JobManager::peak_active() const noexcept {
  return impl_->peak;
}

std::uint64_t JobManager::engines_started() const noexcept {
  return impl_->engines;
}

}  // namespace hefesto::sim
