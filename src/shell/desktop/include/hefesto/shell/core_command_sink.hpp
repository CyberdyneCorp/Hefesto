// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Binds the desktop shell to the core command API (capability
// hefesto-architecture, requirement "Single command API"; design.md D5/D6).
//
// CoreSession owns one headless command session for the shell: portable
// platform services, the rotating session log (capability
// hefesto-diagnostics, "Diagnostic presentation and logging"), the command
// log (every command, whatever its origin), project settings, the script
// session, the registry (built-ins plus whatever the shell registers) and
// the Dispatcher.
// CoreCommandSink turns each command-bar line into Invocations with origin
// `gui` and runs them through that Dispatcher.

#pragma once

#include <functional>
#include <optional>
#include <string_view>

#include "hefesto/cmd/command_log.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/pdk/manager.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/platform/session_log.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/shell/backup_timer.hpp"
#include "hefesto/shell/command_sink.hpp"

namespace hefesto::shell {

/// The host services of a desktop session (hefesto-files-and-pdks "PDK
/// store", cell-io-safety): off by default so tests stay hermetic.
struct HostServices {
  /// POSIX process runner (the curl Fetcher), the managed PDK store over
  /// the PDK manager, the fcntl() lock service and the POSIX file host
  /// (ownership, ctime, process liveness: import of Magic crash backups).
  bool posix = false;
  /// The PDK root; nullopt: PDK_ROOT, else ~/.ciel.
  std::optional<platform::Path> pdk_root;
};

class CoreSession {
 public:
  /// Registers every core command (cmd::register_all_commands). Configure `options` (storage root, PDK
  /// roots) and `host` before constructing: the Dispatcher keeps a view of the services.
  /// With `log_dir`, the session writes `<log_dir>/hefesto.log` (rotated);
  /// without it, it keeps no log file (tests).
  explicit CoreSession(platform::PortableOptions options, std::optional<platform::Path> log_dir = std::nullopt,
                       HostServices host = {});
  ~CoreSession();
  CoreSession(const CoreSession&) = delete;
  CoreSession& operator=(const CoreSession&) = delete;

  [[nodiscard]] cmd::CommandRegistry& registry() noexcept { return registry_; }
  [[nodiscard]] cmd::Dispatcher& dispatcher() noexcept { return dispatcher_; }
  /// The M1 session state (technology, cells, windows) every command sees.
  [[nodiscard]] editor::EditorState& editor() noexcept { return editor_; }
  [[nodiscard]] platform::FileSystem& files() noexcept { return services_.files(); }
  /// Every command run through the Dispatcher, as replayable script lines.
  [[nodiscard]] const cmd::CommandLog& command_log() const noexcept { return command_log_; }
  /// Set when `log_dir` was given but the log could not be opened (PLATFORM-LOG-001).
  [[nodiscard]] const std::optional<diag::Diagnostic>& log_warning() const noexcept { return log_.warning; }
  /// True once `quit` ran.
  [[nodiscard]] bool quit_requested() const noexcept { return session_.quit_requested; }
  /// The project's typed settings (magic-compat and the compat settings).
  [[nodiscard]] settings::ProjectSettings& project() noexcept { return project_; }
  [[nodiscard]] platform::PlatformServices services() noexcept { return services_.view(); }
  /// The session's PDK install host (environment, symbolic links) for
  /// app::StartupEnvironment::host; nullptr without POSIX host services.
  [[nodiscard]] pdk::InstallHost* install_host() noexcept { return pdk_.host.get(); }
  /// The session's PDK manager (store root, installs); nullptr without host services.
  [[nodiscard]] pdk::PdkManager* pdk_manager() noexcept { return pdk_.manager.get(); }

  /// The crash-backup clock armed by `crashbackups`. Runs `crash save`
  /// through the Dispatcher when it is due (call on the UI thread, between
  /// commands); true when it ran.
  bool poll_backup_timer();
  /// Seconds until the backup timer fires; nullopt when it is not armed.
  [[nodiscard]] std::optional<double> backup_timer_seconds() const { return timer_->seconds_left(); }
  /// Records a command that another Dispatcher ran for this session (the
  /// PDK panel's background job, shell/pdk_job.hpp) as if this one had: the
  /// command log, the diagnostic sinks (all but PDK-PROGRESS-001) and the
  /// command listener. Call on the UI thread.
  void record_command(const cmd::Invocation& invocation, const cmd::CommandResult& result);
  /// Normal exit (cell-io-safety "Normal exit"): removes the session's crash
  /// backup and lock file. Not called on a termination request, so the
  /// backup stays for `crash recover`.
  void end_crash_session();

  /// Called after every command the Dispatcher runs, whatever its origin
  /// (command bar, keys, startup, scripts), so the UI can refresh. Empty:
  /// none. The Dispatcher has no observer removal, so the session keeps one
  /// forwarding observer and a window sets and clears its listener.
  using CommandListener = std::function<void(const cmd::Invocation&, const cmd::CommandResult&)>;
  void set_command_listener(CommandListener listener) { forwarder_.listener = std::move(listener); }

 private:
  struct Forwarder final : cmd::CommandObserver {
    CommandListener listener;
    void on_command(const cmd::Invocation& invocation, const cmd::CommandResult& result) override {
      if (listener) listener(invocation, result);
    }
  };

  platform::PortableServices services_;
  platform::SessionLog log_;  // installed before dispatcher_ takes its view of services_
  pdk::SessionPdk pdk_;       // keeps the managed PDK store's manager and host alive
  cmd::CommandLog command_log_{log_.logger};
  settings::ProjectSettings project_;
  cmd::ScriptSession session_{project_};
  cmd::CommandRegistry registry_;
  editor::EditorState editor_;
  DeadlineTimer* timer_ = nullptr;  // owned by the editor's cell session
  cmd::Dispatcher dispatcher_;
  Forwarder forwarder_;
};

class CoreCommandSink final : public CommandSink {
 public:
  explicit CoreCommandSink(cmd::Dispatcher& dispatcher) : dispatcher_(dispatcher) {}

  /// Parses `line` with the M0 script reader (so `a; b` runs two commands)
  /// and executes each command until one fails. The outcome's message is the
  /// last result's string form, or the first error's text on failure; every
  /// diagnostic is rendered as text.
  CommandOutcome submit(std::string_view line) override;

 private:
  cmd::Dispatcher& dispatcher_;
};

}  // namespace hefesto::shell
