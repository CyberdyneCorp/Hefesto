// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/pdk_job.hpp"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <thread>
#include <utility>

#include "hefesto/cmd/script.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/settings/project_settings.hpp"

namespace hefesto::shell {

namespace {

std::string_view phase_verb(pdk::InstallPhase phase) {
  switch (phase) {
    case pdk::InstallPhase::download: return "Downloading";
    case pdk::InstallPhase::verify: return "Verifying";
    case pdk::InstallPhase::unpack: return "Unpacking";
    case pdk::InstallPhase::finalize: return "Finishing";
  }
  return "Working";
}

double part_fraction(const pdk::InstallProgress& p) {
  if (p.phase == pdk::InstallPhase::finalize) return 1.0;
  if (p.total == 0) return 0.0;
  return std::clamp(static_cast<double>(p.bytes) / static_cast<double>(p.total), 0.0, 1.0);
}

}  // namespace

PdkProgressView pdk_progress_view(const pdk::InstallProgress& p) {
  if (p.parts == 0) {  // live download, asset count unknown: the asset's own progress
    const double within = part_fraction(p);
    std::string text = std::string(phase_verb(p.phase)) + " " + p.asset + " (asset " + std::to_string(p.part) + ")";
    if (p.total > 0) text += " " + std::to_string(static_cast<int>(std::floor(within * 100.0))) + "%";
    return PdkProgressView{within, std::move(text)};
  }
  const std::size_t parts = p.parts;
  const std::size_t done = std::min(p.part > 0 ? p.part - 1 : 0, parts);
  const double within = part_fraction(p);
  PdkProgressView view;
  view.fraction = p.phase == pdk::InstallPhase::finalize
                      ? 1.0
                      : std::clamp((static_cast<double>(done) + within) / static_cast<double>(parts), 0.0, 1.0);
  view.text = std::string(phase_verb(p.phase));
  if (!p.asset.empty()) view.text += " " + p.asset;
  view.text += " (" + std::to_string(std::min(std::max<std::size_t>(p.part, 1), parts)) + "/" +
               std::to_string(parts) + ")";
  if (p.phase != pdk::InstallPhase::finalize && p.total > 0) {
    view.text += " " + std::to_string(static_cast<int>(std::floor(within * 100.0))) + "%";
  }
  return view;
}

std::string with_progress(std::string_view line) {
  const std::string s(line);
  const bool long_op = s.starts_with("pdk install ") || s.starts_with("pdk import ");
  if (!long_op || s.find(" -progress") != std::string::npos) return s;
  return s + " -progress";
}

// Plain members plus flags, not std::optional: GCC 14 reports a bogus
// -Wmaybe-uninitialized when an optional of a struct with strings is assigned.
struct PdkJob::State {
  mutable std::mutex mutex;
  bool running = false;
  pdk::InstallProgress progress;
  bool has_progress = false;
  Finished finished;
  bool has_finished = false;
  std::thread worker;
};

namespace {

using Update = std::function<void(const pdk::InstallProgress&)>;

/// The Dispatcher publishes a command's diagnostics when it returns, so
/// PDK-PROGRESS-001 cannot drive a live bar: downloads (the long part) are
/// observed at the Fetcher instead, live from the fetcher's thread. The
/// asset count is not known there (parts = 0).
class ObservingFetcher final : public pdk::Fetcher {
 public:
  ObservingFetcher(std::unique_ptr<pdk::Fetcher> inner, Update update)
      : inner_(std::move(inner)), update_(std::move(update)) {}
  diag::Outcome<std::string> get_text(std::string_view url) override { return inner_->get_text(url); }
  diag::Status download(std::string_view url, const platform::Path& to,
                        const std::function<void(std::uint64_t, std::uint64_t)>& progress) override {
    ++part_;
    const std::string_view asset = url.substr(url.find_last_of('/') + 1);
    pdk::InstallProgress event{pdk::InstallPhase::download, std::string(asset), part_, 0, 0, 0};
    update_(event);
    return inner_->download(url, to, [&](std::uint64_t bytes, std::uint64_t total) {
      event.bytes = bytes;
      event.total = total;
      update_(event);
      if (progress) progress(bytes, total);
    });
  }

 private:
  std::unique_ptr<pdk::Fetcher> inner_;
  Update update_;
  std::size_t part_ = 0;
};

/// The last progress event among a finished command's diagnostics; false when none.
bool last_progress(const cmd::CommandResult& result, pdk::InstallProgress& out) {
  bool found = false;
  for (auto it = result.diagnostics.rbegin(); it != result.diagnostics.rend() && !found; ++it) {
    if (const auto p = pdk::parse_install_progress(*it)) {
      out = *p;
      found = true;
    }
  }
  return found;
}

/// A headless command session with the PDK commands only, built and used
/// on the worker thread.
cmd::CommandResult run_job(const cmd::Invocation& invocation, PdkJob::Setup setup, const Update& update) {
  platform::PortableServices services(std::move(setup.options));
  settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  if (setup.hosts.fetcher) {
    setup.hosts.fetcher = [make = std::move(setup.hosts.fetcher), update](platform::PlatformServices& s) {
      return std::make_unique<ObservingFetcher>(make(s), update);
    };
  }
  (void)pdk::register_pdk_commands(registry, std::move(setup.hosts));
  editor::EditorState editor;
  editor.settings = std::move(setup.settings);
  cmd::Dispatcher dispatcher(registry, services.view(), project, script);
  dispatcher.set_editor(&editor);
  return dispatcher.execute(invocation);
}

}  // namespace

PdkJob::PdkJob() : state_(std::make_shared<State>()) {}

PdkJob::~PdkJob() {
  wait();
}

bool PdkJob::start(std::string_view line, Setup setup) {
  const cmd::ParsedScript parsed = cmd::parse_script(line, "PDK manager");
  if (parsed.error || parsed.commands.size() != 1 || parsed.commands.front().words.empty()) return false;
  const auto& words = parsed.commands.front().words;
  cmd::Invocation invocation{words.front(), {words.begin() + 1, words.end()}, cmd::Origin::gui, std::nullopt};
  {
    const std::lock_guard guard(state_->mutex);
    if (state_->running) return false;  // never blocks the UI thread
  }
  wait();  // joins the previous, finished worker
  {
    const std::lock_guard guard(state_->mutex);
    state_->running = true;
    state_->has_progress = false;
    state_->has_finished = false;
  }
  // The worker shares only the state (by value: a shared_ptr), never `this`.
  std::shared_ptr<State> state = state_;
  state_->worker = std::thread([state, invocation, setup = std::move(setup)]() mutable {
    const auto update = [state](const pdk::InstallProgress& p) {
      const std::lock_guard guard(state->mutex);
      state->progress = p;
      state->has_progress = true;
    };
    cmd::CommandResult result = run_job(invocation, std::move(setup), update);
    const std::lock_guard guard(state->mutex);
    if (last_progress(result, state->progress)) state->has_progress = true;
    state->finished.invocation = invocation;
    state->finished.result = std::move(result);
    state->has_finished = true;
    state->running = false;
  });
  return true;
}

bool PdkJob::running() const {
  const std::lock_guard guard(state_->mutex);
  return state_->running;
}

bool PdkJob::progress(pdk::InstallProgress& out) const {
  const std::lock_guard guard(state_->mutex);
  if (state_->has_progress) out = state_->progress;
  return state_->has_progress;
}

bool PdkJob::take_finished(Finished& out) {
  const std::lock_guard guard(state_->mutex);
  if (!state_->has_finished) return false;
  out = std::move(state_->finished);
  state_->has_finished = false;
  return true;
}

void PdkJob::wait() {
  if (state_->worker.joinable() && state_->worker.get_id() != std::this_thread::get_id()) state_->worker.join();
}

}  // namespace hefesto::shell
