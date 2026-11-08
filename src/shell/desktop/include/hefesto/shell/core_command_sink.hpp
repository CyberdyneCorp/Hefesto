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

#include <optional>
#include <string_view>

#include "hefesto/cmd/command_log.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/platform/session_log.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/shell/command_sink.hpp"

namespace hefesto::shell {

class CoreSession {
 public:
  /// Registers the core built-ins. Configure `options` (storage root, PDK
  /// roots) before constructing: the Dispatcher keeps a view of the services.
  /// With `log_dir`, the session writes `<log_dir>/hefesto.log` (rotated);
  /// without it, it keeps no log file (tests).
  explicit CoreSession(platform::PortableOptions options, std::optional<platform::Path> log_dir = std::nullopt);
  ~CoreSession();
  CoreSession(const CoreSession&) = delete;
  CoreSession& operator=(const CoreSession&) = delete;

  [[nodiscard]] cmd::CommandRegistry& registry() noexcept { return registry_; }
  [[nodiscard]] cmd::Dispatcher& dispatcher() noexcept { return dispatcher_; }
  [[nodiscard]] platform::FileSystem& files() noexcept { return services_.files(); }
  /// Every command run through the Dispatcher, as replayable script lines.
  [[nodiscard]] const cmd::CommandLog& command_log() const noexcept { return command_log_; }
  /// Set when `log_dir` was given but the log could not be opened (PLATFORM-LOG-001).
  [[nodiscard]] const std::optional<diag::Diagnostic>& log_warning() const noexcept { return log_.warning; }
  /// True once `quit` ran.
  [[nodiscard]] bool quit_requested() const noexcept { return session_.quit_requested; }

 private:
  platform::PortableServices services_;
  platform::SessionLog log_;  // installed before dispatcher_ takes its view of services_
  cmd::CommandLog command_log_{log_.logger};
  settings::ProjectSettings project_;
  cmd::ScriptSession session_{project_};
  cmd::CommandRegistry registry_;
  cmd::Dispatcher dispatcher_;
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
