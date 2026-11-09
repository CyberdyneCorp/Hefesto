// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/core_command_sink.hpp"

#include <string>
#include <utility>

#include "hefesto/cell/crash.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/script.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/diag/format.hpp"
#include "hefesto/pdk/diag_codes.hpp"
#include "hefesto/platform_posix/file_host.hpp"
#include "hefesto/platform_posix/lock_service.hpp"
#include "hefesto/version.hpp"
#include "hefesto/platform_posix/process_runner.hpp"

namespace hefesto::shell {

namespace {

constexpr std::string_view kSourceName = "command bar";

void append_diagnostics(CommandOutcome& outcome, const std::vector<diag::Diagnostic>& diagnostics) {
  for (const diag::Diagnostic& d : diagnostics) {
    outcome.diagnostics.push_back(diag::format_text(d));
  }
}

CommandOutcome failure(const diag::Diagnostic& error) {
  CommandOutcome outcome{OutcomeStatus::failed, error.text, {}};
  append_diagnostics(outcome, {error});
  return outcome;
}

std::string first_error_text(const cmd::CommandResult& result) {
  for (const diag::Diagnostic& d : result.diagnostics) {
    if (d.severity == diag::Severity::error) {
      return d.text;
    }
  }
  return "failed";
}

platform::SessionLog open_session_log(platform::PortableServices& services, const std::optional<platform::Path>& dir) {
  if (!dir) {
    return {};
  }
  return platform::install_session_log(services, *dir, "hefesto", diag::codes::kPlatformLog);
}

/// hefesto-files-and-pdks "PDK store": the managed store over the PDK
/// manager, installed before the Dispatcher takes its view of the services.
pdk::SessionPdk install_pdk_services(platform::PortableServices& services, const HostServices& host) {
  if (!host.posix) return {};
  services.set_process_runner(std::make_unique<platform_posix::PosixProcessRunner>());
  pdk::SessionPdk pdk = pdk::make_session_pdk(services.files(), services.view().processes, host.pdk_root);
  services.set_pdk_store(pdk::make_managed_pdk_store(*pdk.manager, *pdk.host));
  return pdk;
}

}  // namespace

CoreSession::CoreSession(platform::PortableOptions options, std::optional<platform::Path> log_dir, HostServices host)
    : services_(std::move(options)),
      log_(open_session_log(services_, log_dir)),
      pdk_(install_pdk_services(services_, host)),
      dispatcher_(registry_, services_.view(), project_, session_) {
  // cell-io-safety: locks, crash backups and recovery before any command.
  if (host.posix) {
    editor_.locks = platform_posix::make_lock_service();
  }
  cell::CellSession& cell = cell::cell_session(editor_);
  if (host.posix) {
    cell.host = platform_posix::make_file_host();
    cell.import_magic_backups = true;
  }
  auto timer = std::make_unique<DeadlineTimer>();
  timer_ = timer.get();
  cell.timer = std::move(timer);
  cmd::register_all_commands(registry_);
  dispatcher_.set_editor(&editor_);
  dispatcher_.add_observer(command_log_);
  dispatcher_.add_observer(forwarder_);
  if (log_.logger != nullptr) {
    log_.logger->write(diag::Severity::info, "session start hefesto " + std::string{version()});
  }
}

CoreSession::~CoreSession() {
  if (log_.logger != nullptr) {
    log_.logger->write(diag::Severity::info, "session end");
  }
}

bool CoreSession::poll_backup_timer() {
  if (!timer_->take_due()) return false;
  cell::run_backup_timer(dispatcher_);
  return true;
}

void CoreSession::record_command(const cmd::Invocation& invocation, const cmd::CommandResult& result) {
  command_log_.on_command(invocation, result);
  for (const diag::Diagnostic& d : result.diagnostics) {
    if (d.code != pdk::codes::kProgress) dispatcher_.report(d);
  }
  forwarder_.on_command(invocation, result);
}

void CoreSession::end_crash_session() {
  cell::end_crash_session(editor_, services_.files());
}

CommandOutcome CoreCommandSink::submit(std::string_view line) {
  const cmd::ParsedScript parsed = cmd::parse_script(line, kSourceName);
  if (parsed.error) {
    dispatcher_.report(*parsed.error);
    return failure(*parsed.error);
  }
  CommandOutcome outcome;
  for (const cmd::ParsedCommand& command : parsed.commands) {
    cmd::Invocation invocation{command.words.front(),
                               {command.words.begin() + 1, command.words.end()},
                               cmd::Origin::gui,
                               std::nullopt};
    const cmd::CommandResult result = dispatcher_.execute(invocation);
    append_diagnostics(outcome, result.diagnostics);
    if (!result.ok()) {
      outcome.status = OutcomeStatus::failed;
      outcome.message = first_error_text(result);
      break;
    }
    outcome.message = result.value.to_string();
  }
  return outcome;
}

}  // namespace hefesto::shell
