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
#include <atomic>
#include <cstdint>
#include <memory>
#include <ctime>
#include <deque>
#include <map>
#include <mutex>
#include <string_view>
#include <thread>
#include <utility>

#include "hefesto/sim/result/progress.hpp"
#include "sim/engine/job_manager_events.hpp"

namespace hefesto::sim {

namespace {

constexpr std::size_t kMaxErrorLines = 50;  // "Cancellation and fault states": the last 50 lines

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

bool rerunnable(JobState state) noexcept {
  return state == JobState::failed || state == JobState::cancelled;
}

bool is_active(JobState state) noexcept {
  return state == JobState::loading || state == JobState::running || state == JobState::paused;
}

// The refusals of a healthy engine (worker_engine.cpp): the deck was
// rejected, a command was rejected, or the engine was busy. Every other
// error of an Engine call is a fault.
bool is_fault(const diag::Diagnostic& error) noexcept {
  return error.code != "SIM-ENGINE-009" && error.code != "SIM-ENGINE-010" && error.code != "SIM-ENGINE-011";
}

// M9 R3 (D18): a limit setting is refused at zero or above twice its default.
template <typename T>
std::optional<diag::Diagnostic> check_limit(std::string_view name, T value, T fallback) {
  if (value != 0 && value <= fallback * 2) {
    return std::nullopt;
  }
  return diag::make("SIM-ENGINE-039", std::string{name} + " " + std::to_string(value) +
                                          " is refused: it must be between 1 and twice the default " +
                                          std::to_string(fallback));
}

// What the final vectors cost against the result budget ("Saved vectors and
// memory budget": 8 bytes per point, 16 when complex).
std::uint64_t result_cost(const std::vector<Vector>& vectors) {
  std::uint64_t cost = 0;
  for (const Vector& v : vectors) {
    cost += static_cast<std::uint64_t>(v.real.size()) * (v.complex ? 16U : 8U);
  }
  return cost;
}

// The first line of a diagnostic: WorkerEngine appends the engine's last
// error-stream lines, which the job record already holds.
diag::Diagnostic first_line(diag::Diagnostic d) {
  if (const std::size_t nl = d.text.find('\n'); nl != std::string::npos) {
    d.text.resize(nl);
  }
  return d;
}

}  // namespace

struct JobManager::Impl {
  // What a loader thread hands back; written by the loader before it posts
  // `started`, read by the owning thread after joining it.
  struct Loader {
    std::thread thread;
    std::atomic<bool> cancel{false};
    std::unique_ptr<Engine> made;                 // a new engine from the factory
    std::optional<diag::Diagnostic> unavailable;  // the factory failed
    std::optional<diag::Diagnostic> error;        // load or run failed
    bool ran = false;                             // run() was called and succeeded
  };

  struct Slot {
    std::unique_ptr<Engine> engine;
    std::uint64_t generation = 0;  // of the job's connection; 0 = none
    std::uint64_t engine_number = 0;
    std::optional<std::size_t> job;
    bool saw_running = false;
    bool cancel_requested = false;
    bool halted = false;                 // the engine reported `paused` and was not resumed since
    std::unique_ptr<Loader> loader;      // set while the job loads
    std::vector<EngineState> deferred;   // state events that arrived while it loaded
    bool failed_while_loading = false;   // a deferred `failed`: later events of the dead engine are dropped
    /// M9 R3: values streamed through on_data by the job's run (the
    /// callback counts them on the engine's thread).
    std::shared_ptr<std::atomic<std::uint64_t>> streamed;
    std::uint64_t over_budget = 0;       // the estimate that passed the budget; 0 = within it
  };

  /// Deferred states kept per loading job before they are coalesced.
  static constexpr std::size_t kMaxDeferredStates = 64;

  JobManagerOptions options;
  std::uint32_t workers = 1;
  std::optional<diag::Diagnostic> pool_diagnostic;
  std::optional<diag::Diagnostic> limits_diagnostic;  // M9 R3: SIM-ENGINE-039
  std::deque<JobRecord> records;         // stable addresses for record()
  std::deque<PreparedDeck> decks;        // by record index; released once handed to the loader
  std::deque<ProgressTracker> trackers;  // by record index
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
    check_limits();
  }

  /// M9 R3: a refused limit setting falls back to its default.
  void check_limits() {
    if (auto refused = check_limit("job log limit", options.max_log_bytes, kMaxJobLogBytes)) {
      options.max_log_bytes = kMaxJobLogBytes;
      limits_diagnostic = std::move(refused);
    }
    if (auto refused = check_limit("result budget", options.result_budget, kDesktopResultBudget)) {
      options.result_budget = kDesktopResultBudget;
      limits_diagnostic = std::move(refused);
    }
  }

  ~Impl() {
    // Loaders first (they use the engines), then the engines: their reader
    // threads may still post events.
    for (Slot& slot : slots) {
      if (slot.loader) {
        slot.loader->cancel = true;
        slot.loader->thread.join();
      }
    }
    for (Slot& slot : slots) {
      slot.engine.reset();
      slot.loader.reset();
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
    JobId id = opts.run_set_point.value_or(timestamp(now()) + "-" + opts.testbench + "-" + opts.corner);
    // M9 R2 review: the id becomes a folder name of the result store (with
    // room for a collision suffix).
    if (!is_safe_job_id(id) || !is_safe_job_id(id + "-99999")) {
      return diag::make("SIM-ENGINE-038", "job id '" + id + "' is not a single safe path component");
    }
    if (opts.run_set_point) {
      // M9 R3: a point that ended without a completed result may run again
      // under its id ("Run set execution": resuming runs exactly those).
      const auto it = index.find(id);
      if (it != index.end() && !rerunnable(records[it->second].job.state)) {
        return diag::make("SIM-ENGINE-036", "job " + id + " already exists");
      }
      return id;
    }
    const JobId base = id;
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

  [[nodiscard]] std::size_t loading() const noexcept {
    return static_cast<std::size_t>(
        std::count_if(slots.begin(), slots.end(), [](const Slot& slot) { return slot.loader != nullptr; }));
  }

  // --- starting jobs ---

  EngineCallbacks callbacks_for(std::size_t s, std::uint64_t generation) {
    EventQueue* queue_ptr = &events;
    std::shared_ptr<std::atomic<std::uint64_t>> streamed = slots[s].streamed;
    EngineCallbacks callbacks;
    callbacks.on_output = [queue_ptr, s, generation](std::string_view line) {
      queue_ptr->post(EngineEvent{s, generation, EngineEvent::Kind::output, std::string{line}, EngineState::idle});
    };
    callbacks.on_error = [queue_ptr, s, generation](std::string_view line) {
      queue_ptr->post(EngineEvent{s, generation, EngineEvent::Kind::error, std::string{line}, EngineState::idle});
    };
    callbacks.on_state = [queue_ptr, s, generation](EngineState state) {
      queue_ptr->post(EngineEvent{s, generation, EngineEvent::Kind::state, {}, state});
    };
    callbacks.on_progress = [queue_ptr, s, generation](std::string_view status, double point) {
      EngineEvent event{s, generation, EngineEvent::Kind::progress, std::string{status}, EngineState::idle};
      event.point = point;
      queue_ptr->post(std::move(event));
    };
    callbacks.on_data = [queue_ptr, s, generation, streamed](std::string_view vector, double value) {
      streamed->fetch_add(1, std::memory_order_relaxed);
      if (vector == "time") {  // the transient scale ("Progress reporting")
        EngineEvent event{s, generation, EngineEvent::Kind::scale, {}, EngineState::idle};
        event.point = value;
        queue_ptr->post(std::move(event));
      }
    };
    return callbacks;
  }

  /// The loader thread: the factory (when the slot has no engine), the
  /// callbacks, load and run; then `started`. It touches only `loader`,
  /// `engine` and the event queue.
  /// The factory on the loader thread; nullptr (and `loader.unavailable`) on failure.
  Engine* make_engine(Loader& loader) const {
    diag::Outcome<std::unique_ptr<Engine>> made = options.make_engine();
    if (made.ok() && made.value() != nullptr) {
      loader.made = std::move(made.value());
      return loader.made.get();
    }
    loader.unavailable = made.ok() ? diag::make("SIM-ENGINE-031", "the engine host returned no engine") : made.error();
    return nullptr;
  }

  void load_job(Loader& loader, Engine* engine, EngineCallbacks callbacks, std::size_t s, std::uint64_t generation,
                const std::vector<std::string>& lines, const LoadOptions& load_options) {
    if (engine == nullptr) {
      engine = make_engine(loader);
    }
    if (engine != nullptr) {
      engine->set_callbacks(std::move(callbacks));
      diag::Status status = loader.cancel ? diag::Status{diag::Done{}} : engine->load(lines, load_options);
      if (status.ok() && !loader.cancel) {
        status = engine->run();
        loader.ran = status.ok();
      }
      if (!status.ok()) {
        loader.error = status.error();
      }
    }
    events.post(EngineEvent{s, generation, EngineEvent::Kind::started, {}, EngineState::idle});
  }

  void start(std::size_t s, std::size_t job) {
    JobRecord& rec = records[job];
    Slot& slot = slots[s];
    if (!options.make_engine && !slot.engine) {
      rec.diagnostics.push_back(diag::make("SIM-ENGINE-031", "simulation engine unavailable: no engine host"));
      set_state(rec, JobState::failed);
      decks[job] = PreparedDeck{};
      return;
    }
    slot.job = job;
    slot.saw_running = false;
    slot.cancel_requested = false;
    slot.halted = false;
    slot.deferred.clear();
    slot.failed_while_loading = false;
    slot.streamed = std::make_shared<std::atomic<std::uint64_t>>(0);
    slot.over_budget = 0;
    slot.generation = ++generations;  // per job: late events of the last job are dropped
    rec.engine = slot.engine_number;
    set_state(rec, JobState::preparing);
    trackers[job] = ProgressTracker{analyses_of(decks[job].lines)};
    set_state(rec, JobState::loading);
    LoadOptions load_options{std::move(decks[job].descriptor.init_commands), rec.options.seed};
    std::vector<std::string> lines = std::move(decks[job].lines);
    decks[job] = PreparedDeck{};
    slot.loader = std::make_unique<Loader>();
    slot.loader->thread =
        std::thread([this, loader = slot.loader.get(), engine = slot.engine.get(),
                     callbacks = callbacks_for(s, slot.generation), s, generation = slot.generation,
                     lines = std::move(lines), load_options = std::move(load_options)]() mutable {
          load_job(*loader, engine, std::move(callbacks), s, generation, lines, load_options);
        });
  }

  /// The loader of slot `s` ended: keep its engine, then end the job or
  /// apply the states held back during the load.
  void on_started(std::size_t s) {
    Slot& slot = slots[s];
    const std::unique_ptr<Loader> loader = std::move(slot.loader);
    loader->thread.join();
    if (loader->made) {
      slot.engine = std::move(loader->made);
      slot.engine_number = ++engines;
    }
    const std::size_t job = *slot.job;
    JobRecord& rec = records[job];
    rec.engine = slot.engine_number;
    const std::vector<EngineState> deferred = std::exchange(slot.deferred, {});
    if (loader->unavailable) {
      end_unstarted(s, rec, "simulation engine unavailable: " + loader->unavailable->text);
      return;
    }
    if (loader->error) {
      start_failed(s, job, std::move(*loader->error), deferred);
      return;
    }
    if (slot.cancel_requested && !loader->ran) {
      end_unstarted(s, rec, {});
      return;
    }
    if (slot.cancel_requested && !cancel_run(s)) {
      return;  // the cancel faulted the engine
    }
    replay(s, deferred);
  }

  /// The job ends before its run started: `cancelled` after cancel, else
  /// `failed` with `reason` (SIM-ENGINE-031). The worker is free again.
  void end_unstarted(std::size_t s, JobRecord& rec, const std::string& reason) {
    if (slots[s].cancel_requested) {
      set_state(rec, JobState::cancelled);
    } else {
      rec.diagnostics.push_back(diag::make("SIM-ENGINE-031", reason));
      set_state(rec, JobState::failed);
    }
    slots[s].job.reset();
  }

  /// A cancel that arrived while the job loaded, after its run started.
  bool cancel_run(std::size_t s) {
    diag::Status status = slots[s].engine->cancel();
    if (!status.ok() && is_fault(status.error())) {
      fault(s, status.error());
      return false;
    }
    return true;  // the run's end (or the halt) ends the job as cancelled
  }

  /// Applies held-back state events in order until one ends the job.
  void replay(std::size_t s, const std::vector<EngineState>& deferred) {
    const std::uint64_t generation = slots[s].generation;
    for (const EngineState state : deferred) {
      if (!slots[s].job || slots[s].generation != generation) {
        return;
      }
      on_state(s, state);
    }
  }

  /// Keeps a state event of a loading job, coalescing a flood: at most
  /// kMaxDeferredStates, then `running` (when one was) and the last state,
  /// or `failed`.
  static void defer(Slot& slot, EngineState state) {
    if (slot.failed_while_loading) {
      return;  // the engine is dead: the fault is applied after `started`
    }
    slot.failed_while_loading = state == EngineState::failed;
    slot.deferred.push_back(state);
    if (slot.deferred.size() <= kMaxDeferredStates) {
      return;
    }
    const auto has = [&slot](EngineState wanted) {
      return std::find(slot.deferred.begin(), slot.deferred.end(), wanted) != slot.deferred.end();
    };
    std::vector<EngineState> kept;
    if (has(EngineState::failed)) {
      kept.push_back(EngineState::failed);
    } else {
      if (has(EngineState::running)) {
        kept.push_back(EngineState::running);
      }
      if (state != EngineState::running) {
        kept.push_back(state);
      }
    }
    slot.deferred = std::move(kept);
  }

  /// A load or run call failed: the states of the load (a fault among
  /// them ends the job as a fault), then the refusal or the fault.
  void start_failed(std::size_t s, std::size_t job, diag::Diagnostic error, const std::vector<EngineState>& deferred) {
    replay(s, deferred);
    if (slots[s].job != job) {
      return;  // a fault event ended it
    }
    if (is_fault(error)) {
      fault(s, std::move(error));
      return;
    }
    JobRecord& rec = records[job];
    if (slots[s].cancel_requested) {
      set_state(rec, JobState::cancelled);
    } else {
      rec.diagnostics.push_back(first_line(std::move(error)));
      append_error_lines(rec, false);
      set_state(rec, JobState::failed);
    }
    slots[s].job.reset();  // the engine refused the deck but is healthy: reused
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

  static void append_error_lines(JobRecord& rec, bool only_errors) {
    for (const std::string& line : rec.errors) {
      if (!only_errors || line.starts_with("Error")) {
        rec.diagnostics.push_back(diag::make("SIM-ENGINE-032", line));
      }
    }
  }

  /// D-25 (a): the job fails, the engine is discarded and the worker gets a
  /// new engine for its next job. Without a cause, the engine's own
  /// diagnostic (WorkerEngine keeps the fault) is the cause.
  void fault(std::size_t s, std::optional<diag::Diagnostic> cause) {
    Slot& slot = slots[s];
    if (!cause && slot.engine) {
      if (auto vectors = slot.engine->vectors(); !vectors.ok()) {
        cause = vectors.error();
      }
    }
    if (slot.job) {
      JobRecord& rec = records[*slot.job];
      rec.diagnostics.push_back(diag::make("SIM-ENGINE-030", "simulation engine terminated"));
      if (cause) {
        rec.diagnostics.push_back(first_line(std::move(*cause)));
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
    if (slot.over_budget == 0 && result_cost(rec.vectors) > options.result_budget) {
      slot.over_budget = result_cost(rec.vectors);
    }
    if (slot.over_budget != 0) {
      exceed_budget(rec, slot.over_budget);
    } else {
      set_state(rec, outcome(rec, slot.cancel_requested));
    }
    rec.partial = rec.job.state != JobState::completed && !rec.vectors.empty();
    if (rec.job.state == JobState::completed) {
      rec.progress = 1.0;
    }
    slot.job.reset();  // a healthy engine is reused
  }

  /// "Saved vectors and memory budget": the job fails with its partial result.
  void exceed_budget(JobRecord& rec, std::uint64_t estimate) {
    rec.budget_exceeded = estimate;
    rec.diagnostics.push_back(diag::make("SIM-RESULT-011", "result memory budget exceeded (" +
                                                               std::to_string(estimate) + " > " +
                                                               std::to_string(options.result_budget) + ")"));
    set_state(rec, JobState::failed);
  }

  /// M9 R3: cancels a running job whose streamed values passed the budget;
  /// its run's end fails it (finish). Returns the number of jobs cancelled.
  std::size_t check_budgets() {
    std::size_t cancelled = 0;
    for (std::size_t s = 0; s < slots.size(); ++s) {
      Slot& slot = slots[s];
      if (!slot.job || slot.loader || slot.over_budget != 0 || !slot.streamed || !slot.engine) {
        continue;
      }
      const std::uint64_t values = slot.streamed->load(std::memory_order_relaxed);
      const std::uint64_t estimate = values > UINT64_MAX / 8 ? UINT64_MAX : values * 8;
      if (estimate > options.result_budget) {
        slot.over_budget = estimate;
        (void)cancel_slot(s);  // a fault ends the job at once; the run's end otherwise
        ++cancelled;
      }
    }
    return cancelled;
  }

  /// "Job lifecycle": completed only with a plot and no `Error` line.
  static JobState outcome(JobRecord& rec, bool cancelled) {
    if (cancelled) {
      return JobState::cancelled;
    }
    if (rec.first_error) {
      // The window of 50 lines may have lost the first Error line.
      if (std::find(rec.errors.begin(), rec.errors.end(), *rec.first_error) == rec.errors.end()) {
        rec.diagnostics.push_back(diag::make("SIM-ENGINE-032", *rec.first_error));
      }
      append_error_lines(rec, true);
      return JobState::failed;
    }
    if (rec.vectors.empty()) {
      rec.diagnostics.push_back(diag::make("SIM-ENGINE-037", "the run produced no plot"));
      return JobState::failed;
    }
    return JobState::completed;
  }

  // --- events ---

  void on_output(Slot& slot, std::string line, bool error) {
    JobRecord& rec = records[*slot.job];
    if (error) {
      if (!rec.first_error && line.starts_with("Error")) {
        rec.first_error = line;
      }
      rec.errors.push_back(line);
      if (rec.errors.size() > kMaxErrorLines) {
        rec.errors.erase(rec.errors.begin());
      }
    }
    if (rec.log_bytes + line.size() > options.max_log_bytes) {
      ++rec.log_dropped;
      return;
    }
    rec.log_bytes += line.size();
    rec.log.push_back(LogLine{error, std::move(line)});
  }

  void on_dropped(Slot& slot, const EngineEvent& event) {
    JobRecord& rec = records[*slot.job];
    rec.log_dropped += event.dropped;
    if (!rec.first_error && !event.line.empty()) {
      rec.first_error = event.line;
    }
  }

  void on_progress(Slot& slot, const EngineEvent& event) {
    const std::size_t job = *slot.job;
    ProgressTracker& tracker = trackers[job];
    if (event.kind == EngineEvent::Kind::scale) {
      tracker.scale(event.point);
    } else {
      tracker.status(event.line, event.point);
      records[job].status = event.line;
    }
    records[job].progress = tracker.progress();
  }

  void on_state(std::size_t s, EngineState state) {
    Slot& slot = slots[s];
    JobRecord& rec = records[*slot.job];
    switch (state) {
      case EngineState::running:
        slot.saw_running = true;
        slot.halted = false;
        set_state(rec, JobState::running);
        break;
      case EngineState::paused:
        slot.halted = true;
        if (slot.cancel_requested) {
          finish(s);  // the adapter halts a cancelled run as it pauses one
        } else if (rec.job.state == JobState::running) {
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

  void apply_state(std::size_t s, EngineState state) {
    if (slots[s].loader) {
      defer(slots[s], state);  // the loader owns the engine until `started`
    } else {
      on_state(s, state);
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
    if (slot.failed_while_loading && event.kind != EngineEvent::Kind::started) {
      return;  // a late event of an engine that failed while the job loaded
    }
    switch (event.kind) {
      case EngineEvent::Kind::started: on_started(event.slot); break;
      case EngineEvent::Kind::state: apply_state(event.slot, event.state); break;
      case EngineEvent::Kind::dropped: on_dropped(slot, event); break;
      case EngineEvent::Kind::progress:
      case EngineEvent::Kind::scale: on_progress(slot, event); break;
      default: on_output(slot, std::move(event.line), event.kind == EngineEvent::Kind::error); break;
    }
  }

  std::size_t apply_events() {
    std::vector<EngineEvent> batch = events.take();
    if (options.on_events_taken) {
      options.on_events_taken();
    }
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
    if (status.ok() && required == JobState::paused) {
      slots[*s].halted = false;  // resumed: a `paused` event follows a new halt only
    }
    if (!status.ok() && is_fault(status.error())) {
      fault(*s, status.error());  // a refusal (the run just ended, say) leaves the job alone
    }
    return status;
  }

  diag::Status cancel_queued(std::size_t job) {
    std::erase(queue, job);
    decks[job] = PreparedDeck{};
    set_state(records[job], JobState::cancelled);
    return diag::Done{};
  }

  diag::Status cancel(const JobId& id) {
    const auto job = find(id);
    if (!job.ok()) {
      return job.error();
    }
    if (records[job.value()].job.state == JobState::queued) {
      return cancel_queued(job.value());
    }
    const auto s = slot_of(job.value());
    if (!s) {
      return diag::make("SIM-ENGINE-035", "job " + id + " is not running");
    }
    return cancel_slot(*s);
  }

  /// Cancels the job of slot `s` in its loader or its engine.
  diag::Status cancel_slot(std::size_t s) {
    Slot& slot = slots[s];
    slot.cancel_requested = true;
    if (slot.loader) {
      slot.loader->cancel = true;  // never waits for the load
      return diag::Done{};
    }
    const bool halted = slot.halted;  // paused: the adapter sends no new event
    diag::Status status = slot.engine->cancel();
    if (!status.ok()) {
      if (is_fault(status.error())) {
        fault(s, status.error());
      } else {
        slot.cancel_requested = false;
      }
      return status;
    }
    if (halted) {
      finish(s);  // the partial vectors are those of the pause
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
  impl_->trackers.emplace_back();
  impl_->index.insert_or_assign(id.value(), n);  // a re-run point takes over its id
  impl_->queue.push_back(n);
  return id;
}

std::size_t JobManager::poll_for(std::chrono::milliseconds timeout) {
  impl_->events.wait(timeout);
  return poll();
}

std::size_t JobManager::poll() {
  std::size_t total = 0;
  for (std::size_t pass = 0; pass < kMaxPollPasses; ++pass) {
    const std::size_t n = impl_->apply_events() + impl_->check_budgets() + impl_->start_jobs();
    if (n == 0) {
      break;
    }
    total += n;
  }
  return total;
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

std::size_t JobManager::loading() const noexcept {
  return impl_->loading();
}

std::size_t JobManager::peak_queued_events() const {
  return impl_->events.peak();
}

const std::optional<diag::Diagnostic>& JobManager::limits_diagnostic() const noexcept {
  return impl_->limits_diagnostic;
}

std::size_t JobManager::max_log_bytes() const noexcept {
  return impl_->options.max_log_bytes;
}

std::uint64_t JobManager::result_budget() const noexcept {
  return impl_->options.result_budget;
}

}  // namespace hefesto::sim
