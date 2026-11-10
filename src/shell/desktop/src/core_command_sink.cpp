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
#include "hefesto/platform_net/http_fetcher.hpp"
#include "hefesto/platform_posix/file_host.hpp"
#include "hefesto/platform_posix/lock_service.hpp"
#include "hefesto/ui/bindings.hpp"
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

void append_output(CommandOutcome& outcome, const std::string& value) {
  if (value.empty()) return;
  outcome.output += (outcome.output.empty() ? "" : "\n") + value;
}

CommandOutcome failure(const diag::Diagnostic& error) {
  CommandOutcome outcome{OutcomeStatus::failed, error.text, {}, {}, {}};
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

namespace {

std::size_t color_window_count(const cmd::Dispatcher& dispatcher) {
  const editor::EditorState* editor = dispatcher.editor();
  return editor != nullptr ? editor->color_map.windows.size() : 0;
}

// The desktop has no color window screen yet (task 10.6): a command that
// opens one keeps its core state but says the window is not shown.
constexpr std::string_view kColorWindowNotShown =
    "The color window is not available in the desktop app yet; its commands still work from the command bar.";

}  // namespace

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
    const std::size_t color_windows = color_window_count(dispatcher_);
    const cmd::CommandResult result = dispatcher_.execute(invocation);
    append_diagnostics(outcome, result.diagnostics);
    if (color_window_count(dispatcher_) > color_windows) {
      outcome.diagnostics.emplace_back(kColorWindowNotShown);
      outcome.printed += (outcome.printed.empty() ? "" : "\n") + std::string(kColorWindowNotShown);
    }
    for (const diag::Diagnostic& d : result.diagnostics) {
      if (d.severity == diag::Severity::info) outcome.printed += (outcome.printed.empty() ? "" : "\n") + d.text;
    }
    if (!result.ok()) {
      outcome.status = OutcomeStatus::failed;
      outcome.message = first_error_text(result);
      break;
    }
    outcome.message = result.value.to_string();
    append_output(outcome, outcome.message);
  }
  return outcome;
}

}  // namespace hefesto::shell
