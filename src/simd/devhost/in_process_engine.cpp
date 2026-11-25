// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Owner: SIMD. Developer builds only (HEFESTO_DEVELOPER_BUILD).

#include "devhost/in_process_engine.hpp"

#include <mutex>
#include <string_view>
#include <utility>
#include <vector>

#include "server/plugin.hpp"

namespace hefesto::simd {

namespace {

constexpr std::string_view kUnavailable = "SIM-ENGINE-001";
constexpr std::string_view kRejected = "SIM-ENGINE-009";
constexpr std::string_view kBusy = "SIM-ENGINE-011";
constexpr std::string_view kTerminated = "SIM-ENGINE-008";

class InProcessEngine final : public sim::Engine {
 public:
  explicit InProcessEngine(const hefesto_engine_api& api) : api_(api) {}
  InProcessEngine(const InProcessEngine&) = delete;
  InProcessEngine& operator=(const InProcessEngine&) = delete;
  ~InProcessEngine() override {
    if (engine_ != nullptr) {
      api_.destroy(engine_);
    }
  }

  bool create() {
    const hefesto_engine_callbacks callbacks{this, on_output, on_progress, on_state};
    engine_ = api_.create(&callbacks);
    return engine_ != nullptr;
  }

  void set_callbacks(sim::EngineCallbacks callbacks) override {
    std::lock_guard lock{mutex_};
    callbacks_ = std::move(callbacks);
  }

  diag::Status load(const std::vector<std::string>& deck) override {
    std::vector<const char*> lines;
    lines.reserve(deck.size());
    for (const std::string& line : deck) {
      lines.push_back(line.c_str());
    }
    if (loaded_) {
      api_.clear(engine_);
    }
    const int seed = api_.command(engine_, "setseed 1");
    loaded_ = true;
    return seed != HEFESTO_ENGINE_OK ? status(seed) : status(api_.load(engine_, lines.data(), lines.size()));
  }
  diag::Status run() override { return status(api_.run(engine_)); }
  diag::Status pause() override { return status(api_.pause(engine_)); }
  diag::Status resume() override { return status(api_.resume(engine_)); }
  diag::Status cancel() override { return status(api_.cancel(engine_)); }

  diag::Outcome<std::vector<sim::Vector>> vectors() const override {
    std::vector<sim::Vector> out;
    const int rc = api_.vectors(engine_, &out, [](void* user, const hefesto_engine_vector* v) {
      sim::Vector vector;
      vector.name = v->name;
      vector.type = v->type;
      vector.complex = v->complex != 0 && v->imag != nullptr;
      vector.real.assign(v->real, v->real + v->length);
      if (vector.complex) {
        vector.imag.assign(v->imag, v->imag + v->length);
      }
      static_cast<std::vector<sim::Vector>*>(user)->push_back(std::move(vector));
    });
    if (rc != HEFESTO_ENGINE_OK) {
      return diag::make(kRejected, api_.last_error(engine_));
    }
    return out;
  }

 private:
  [[nodiscard]] diag::Status status(int rc) const {
    switch (rc) {
      case HEFESTO_ENGINE_OK: return diag::Done{};
      case HEFESTO_ENGINE_BUSY: return diag::make(kBusy, "engine busy");
      case HEFESTO_ENGINE_FAULT: return diag::make(kTerminated, "simulation engine terminated");
      default: return diag::make(kRejected, api_.last_error(engine_));
    }
  }

  template <typename Fn>
  static void with_callbacks(void* user, Fn&& fn) {
    auto* self = static_cast<InProcessEngine*>(user);
    std::lock_guard lock{self->mutex_};
    std::forward<Fn>(fn)(self->callbacks_);
  }
  static void on_output(void* user, const char* text, std::size_t size, int is_error) {
    with_callbacks(user, [&](const sim::EngineCallbacks& c) {
      if (is_error != 0 && c.on_error) {
        c.on_error(std::string_view{text, size});
      } else if (c.on_output) {
        c.on_output(std::string_view{text, size});
      }
    });
  }
  static void on_progress(void* user, const char* status, std::size_t size, double point) {
    with_callbacks(user, [&](const sim::EngineCallbacks& c) {
      if (c.on_progress) {
        c.on_progress(std::string_view{status, size}, point);
      }
    });
  }
  static void on_state(void* user, int state) {
    with_callbacks(user, [&](const sim::EngineCallbacks& c) {
      if (c.on_state && state >= HEFESTO_ENGINE_IDLE && state <= HEFESTO_ENGINE_FAILED) {
        c.on_state(static_cast<sim::EngineState>(state));
      }
    });
  }

  const hefesto_engine_api& api_;
  hefesto_engine* engine_ = nullptr;
  bool loaded_ = false;
  std::mutex mutex_;
  sim::EngineCallbacks callbacks_;
};

}  // namespace

diag::Outcome<std::unique_ptr<sim::Engine>> make_in_process_engine(const std::string& plugin_path) {
  const LoadedPlugin plugin = load_adapter(plugin_path, adapter_candidates(executable_dir()));
  if (plugin.api == nullptr) {
    return diag::make(kUnavailable, plugin.error);
  }
  auto engine = std::make_unique<InProcessEngine>(*plugin.api);
  if (!engine->create()) {
    return diag::make(kUnavailable, "the simulation engine could not be created (one per process)");
  }
  return std::unique_ptr<sim::Engine>{std::move(engine)};
}

}  // namespace hefesto::simd
