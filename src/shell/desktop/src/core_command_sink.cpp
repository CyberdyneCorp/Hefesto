// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/core_command_sink.hpp"

#include <string>
#include <utility>

#include "hefesto/cell/crash.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/diag/format.hpp"
#include "hefesto/pdk/diag_codes.hpp"
#include "hefesto/platform_net/http_fetcher.hpp"
#include "hefesto/platform_posix/file_host.hpp"
#include "hefesto/platform_posix/lock_service.hpp"
#include "hefesto/ui/bindings.hpp"
#include "hefesto/ui/history.hpp"
#include "hefesto/ui/state.hpp"
#include "hefesto/version.hpp"
#include "hefesto/platform_posix/process_runner.hpp"
#include "hefesto/shell/gui_script.hpp"

namespace hefesto::shell {

namespace {

void append_diagnostics(CommandOutcome& outcome, const std::vector<diag::Diagnostic>& diagnostics) {
  for (const diag::Diagnostic& d : diagnostics) {
    outcome.diagnostics.push_back(diag::format_text(d));
  }
}

void append_output(CommandOutcome& outcome, const std::string& value) {
  if (value.empty()) return;
  outcome.output += (outcome.output.empty() ? "" : "\n") + value;
}

CommandOutcome failure(const diag::Diagnostic& error) {
  CommandOutcome outcome{OutcomeStatus::failed, error.text, {}, {}, {}};
  append_diagnostics(outcome, {error});
  return outcome;
}

std::string first_reported_error(const std::vector<diag::Diagnostic>& diagnostics) {
  for (const diag::Diagnostic& d : diagnostics) {
    if (d.severity == diag::Severity::error && !d.text.empty()) {
      return d.text;
    }
  }
  return "failed";
}

std::string first_error_text(const cmd::CommandResult& result) {
  return first_reported_error(result.diagnostics);
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
  // hefesto-licensing D-13: in-process HTTPS (URLSession on macOS, libcurl on Linux).
  platform_net::HttpFetcherOptions fetch;
  fetch.github_token = pdk::make_local_install_host()->environment("GITHUB_TOKEN");
  pdk::SessionPdk pdk = pdk::make_session_pdk(services.files(), platform_net::make_http_fetcher(fetch), host.pdk_root);
  services.set_pdk_store(pdk::make_managed_pdk_store(*pdk.manager, *pdk.host));
  return pdk;
}

/// The `pdk` command's host mechanisms: the HTTPS Fetcher with host services,
/// else the core defaults (hermetic tests).
pdk::CommandHosts pdk_command_hosts(const HostServices& host, pdk::InstallHost* install_host) {
  if (!host.posix || install_host == nullptr) return {};
  return pdk::CommandHosts{platform_net::http_fetcher_factory(install_host->environment("GITHUB_TOKEN")), {}, {}};
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
  cell::CellSession& cell = editor_.cell_session;
  if (host.posix) {
    cell.host = platform_posix::make_file_host();
    cell.import_magic_backups = true;
  }
  auto timer = std::make_unique<DeadlineTimer>();
  timer_ = timer.get();
  cell.timer = std::move(timer);
  cmd::register_all_commands(registry_, pdk_command_hosts(host, pdk_.host.get()));
  dispatcher_.set_editor(&editor_);
  ui::install_defaults(editor_);  // the tools and default key bindings (Magic's startup files)
  dispatcher_.add_observer(command_log_);
  dispatcher_.add_observer(redisplay_);  // before the UI listener, which redraws
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
  redisplay_.on_command(invocation, result);
  editor_.windows.update_display();  // a background job has no post-command update
  for (const diag::Diagnostic& d : result.diagnostics) {
    if (d.code != pdk::codes::kProgress) dispatcher_.report(d);
  }
  forwarder_.on_command(invocation, result);
}

void CoreSession::end_crash_session() {
  cell::end_crash_session(editor_, services_.files());
}

TypedLine expand_typed_line(editor::EditorState& editor, std::string_view line) {
  ui::CommandHistory& history = ui::state(editor).history;
  auto expanded = history.expand(line);
  if (!expanded.ok()) {
    return TypedLine{std::nullopt, {}, expanded.error()};
  }
  history.add(expanded.value().line);
  if (expanded.value().print_only) {
    return TypedLine{std::nullopt, expanded.value().line, std::nullopt};
  }
  return TypedLine{std::move(expanded.value().line), {}, std::nullopt};
}

CommandOutcome CoreCommandSink::submit(std::string_view line) {
  editor::EditorState* editor = dispatcher_.editor();
  if (editor == nullptr) {
    const diag::Diagnostic error = diag::make(diag::codes::kCmdSession, "the command bar has no editor session");
    dispatcher_.report(error);
    return failure(error);
  }
  const GuiScriptRun run = run_gui_script_traced(*editor, dispatcher_, line, cmd::Origin::gui);
  CommandOutcome outcome;
  append_diagnostics(outcome, run.reported);
  for (const diag::Diagnostic& d : run.reported) {
    if (d.severity == diag::Severity::info) outcome.printed += (outcome.printed.empty() ? "" : "\n") + d.text;
  }
  if (!run.result.ok()) {
    outcome.status = OutcomeStatus::failed;
    outcome.message = first_error_text(run.result);
    return outcome;
  }
  outcome.message = run.result.value.to_string();
  append_output(outcome, outcome.message);
  if (run.command_failed) {
    // A quiet engine failure (Magic's non-strict convention): the Tcl
    // result is ok, the line still shows as failed with the error it printed.
    outcome.status = OutcomeStatus::failed;
    outcome.message = first_reported_error(run.reported);
  }
  return outcome;
}

}  // namespace hefesto::shell
