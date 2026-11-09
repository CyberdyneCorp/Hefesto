// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "app.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "hefesto/app/startup.hpp"
#include "hefesto/cell/crash.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/command_log.hpp"
#include "hefesto/cmd/script.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/diag/format.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/pdk/manager.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/platform/session_log.hpp"
#include "hefesto/platform_net/http_fetcher.hpp"
#include "hefesto/platform_posix/file_host.hpp"
#include "hefesto/platform_posix/lock_service.hpp"
#include "hefesto/platform_posix/process_runner.hpp"
#include "hefesto/version.hpp"
#include "options.hpp"

namespace hefesto::cli {

namespace {

namespace codes = diag::codes;
using platform::Path;

/// The Magic release this Hefesto build is compatible with (app-startup
/// "Tcl launcher options": --version names it).
constexpr std::string_view kMagicCompatVersion = "8.3.694";

void print_line(std::FILE* stream, std::string_view text) {
  std::fwrite(text.data(), 1, text.size(), stream);
  std::fputc('\n', stream);
  std::fflush(stream);
}

/// Prints each diagnostic to a stream as text or JSON lines.
class StreamSink final : public diag::DiagnosticSink {
 public:
  StreamSink(std::FILE* stream, diag::Format format) : stream_(stream), format_(format) {}

  void emit(const diag::Diagnostic& diagnostic) override {
    const std::scoped_lock lock{mutex_};
    print_line(stream_, diag::format(diagnostic, format_));
  }

 private:
  std::FILE* stream_;
  diag::Format format_;
  std::mutex mutex_;
};

/// Prints the non-empty value of each successful command to stdout.
class ValuePrinter final : public cmd::CommandObserver {
 public:
  explicit ValuePrinter(std::FILE* stream) : stream_(stream) {}

  void on_command(const cmd::Invocation& /*invocation*/, const cmd::CommandResult& result) override {
    if (result.ok() && !result.value.is_none()) {
      print_line(stream_, result.value.to_string());
    }
  }

 private:
  std::FILE* stream_;
};

/// A failure that is a Tcl error in Magic's interpreter: a raised usage error
/// or a dispatch failure (unknown or ambiguous command name).
bool raises_tcl_error(const cmd::CommandResult& result) {
  return result.raises || std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                                      [](const diag::Diagnostic& d) { return d.code == codes::kCmdDispatch; });
}

/// --diagnostics=magic: the console of `magic -dnull -noconsole` running a
/// golden reference script in which every command line is `puts [cmd]` or
/// `catch {cmd} result; puts $result` (tools/golden/README.md). Each script
/// command prints its result on stdout, an empty line for an empty result.
/// As in Magic's Tcl dispatcher (scripting-api "Command result and error
/// conventions"), a failed command's diagnostics are TxError text on stderr
/// and its result stays empty, except when the command set an explicit result
/// (`tech load`), which replaces its last diagnostic, or raised a Tcl error,
/// whose message becomes the result. Other diagnostics are TxError (stderr)
/// or, for informational ones, TxPrintf (stdout) text.
class MagicConsole final : public diag::DiagnosticSink, public cmd::CommandObserver {
 public:
  MagicConsole(std::FILE* out, std::FILE* err) : out_(out), err_(err) {}
  MagicConsole(const MagicConsole&) = delete;
  MagicConsole& operator=(const MagicConsole&) = delete;
  ~MagicConsole() override { flush(); }

  void emit(const diag::Diagnostic& diagnostic) override {
    const std::scoped_lock lock{mutex_};
    flush_pending();
    if (diagnostic.severity == diag::Severity::error) {
      pending_ = diagnostic;  // may turn out to be a command's result
      has_pending_ = true;
      return;
    }
    const bool printf_like = diagnostic.severity == diag::Severity::info || diagnostic.severity == diag::Severity::debug;
    print_line(printf_like ? out_ : err_, diag::format_magic(diagnostic));
  }

  void on_command(const cmd::Invocation& invocation, const cmd::CommandResult& result) override {
    const std::scoped_lock lock{mutex_};
    if (!invocation.source) {
      flush_pending();  // startup commands are not script lines: no result printed
      return;
    }
    const bool is_result = !result.ok() && has_pending_ && !result.diagnostics.empty() &&
                           pending_ == result.diagnostics.back() && (raises_tcl_error(result) || !result.value.is_none());
    if (is_result) {
      has_pending_ = false;
    }
    flush_pending();
    print_line(out_, result_text(result, is_result));
  }

  void flush() {
    const std::scoped_lock lock{mutex_};
    flush_pending();
  }

 private:
  // The Tcl result `puts` prints for one command line.
  static std::string result_text(const cmd::CommandResult& result, bool is_result) {
    if (result.ok() || !result.value.is_none()) {
      return result.value.to_string();
    }
    return is_result ? result.diagnostics.back().text : std::string{};
  }

  void flush_pending() {
    if (has_pending_) {
      print_line(err_, diag::format_magic(pending_));
      has_pending_ = false;
    }
  }

  std::FILE* out_;
  std::FILE* err_;
  // A plain member plus a flag, not std::optional: GCC 14 reports a bogus
  // -Wmaybe-uninitialized when an optional<Diagnostic> is assigned.
  diag::Diagnostic pending_;
  bool has_pending_ = false;
  std::mutex mutex_;
};

std::optional<std::string> read_stdin() {
  std::string text{std::istreambuf_iterator<char>{std::cin}, std::istreambuf_iterator<char>{}};
  if (std::cin.bad()) {
    return std::nullopt;
  }
  return text;
}

// The session state of one CLI run.
class Session {
 public:
  Session(const Options& options, std::FILE* out, std::FILE* err)
      : options_(options),
        services_(platform::PortableOptions{.storage_root = platform::default_storage_dir(), .pdk_roots = {}}),
        sink_(err, options.diagnostics),
        values_(out),
        magic_console_(out, err) {
    log_warning_ = open_log();
    services_.set_process_runner(std::make_unique<platform_posix::PosixProcessRunner>());
    install_pdk_store();
    // cell-io-safety: POSIX locks and the crash-backup file host, before any command.
    editor_.locks = platform_posix::make_lock_service();
    editor_.cell_session.host = platform_posix::make_file_host();
    cmd::register_all_commands(
        registry_, pdk::CommandHosts{platform_net::http_fetcher_factory(install_host_->environment("GITHUB_TOKEN")), {}, {}});
    command_log_ = std::make_unique<cmd::CommandLog>(logger_);
    dispatcher_ = std::make_unique<cmd::Dispatcher>(registry_, services_.view(), project_, script_session_);
    dispatcher_->set_editor(&editor_);
    dispatcher_->add_observer(*command_log_);
    if (options.diagnostics == diag::Format::magic) {
      dispatcher_->add_sink(magic_console_);
      dispatcher_->add_observer(magic_console_);
    } else {
      dispatcher_->add_sink(sink_);
      dispatcher_->add_observer(values_);
    }
  }

  int run(const app::StartupOptions& startup) {
    log(diag::Severity::info, "session start hefesto-cli " + std::string{version()});
    if (log_warning_) {
      dispatcher_->report(*log_warning_);
    }
    run_startup(startup);
    const bool ok = script_session_.quit_requested || run_sources();
    magic_console_.flush();
    cell::end_crash_session(editor_, services_.files());  // normal exit: drop this session's backup and lock
    // `quit N` sets the status (app-startup "quit command and exit
    // status"); a plain `quit` keeps the failure status of earlier commands.
    const int status = script_session_.exit_status != 0 ? script_session_.exit_status : (ok ? kExitOk : kExitFailure);
    log(diag::Severity::info, "session end exit=" + std::to_string(status));
    return status;
  }

 private:
  // hefesto-files-and-pdks: the session's PdkStore resolves through the PDK
  // manager's installs (pinned, enabled, <root>/<name>, PDKPATH) under the root
  // of --pdk-root, PDK_ROOT or ~/.ciel. Before the dispatcher takes its view.
  void install_pdk_store() {
    // hefesto-licensing D-13: in-process HTTPS (URLSession on macOS, libcurl on Linux).
    platform_net::HttpFetcherOptions fetch;
    fetch.github_token = install_host_->environment("GITHUB_TOKEN");
    pdk_fetcher_ = platform_net::make_http_fetcher(std::move(fetch));
    pdk_manager_ = std::make_unique<pdk::PdkManager>(services_.files(), *install_host_, *pdk_fetcher_,
                                                     pdk::Catalog::builtin());
    pdk_manager_->set_root(pdk_manager_->resolve_root(options_.pdk_root));
    services_.set_pdk_store(pdk::make_managed_pdk_store(*pdk_manager_, *install_host_));
  }

  // The sink of this session's diagnostic format.
  diag::DiagnosticSink& console_sink() {
    if (options_.diagnostics == diag::Format::magic) {
      return magic_console_;
    }
    return sink_;
  }

  // Installs the rotating session log; a warning when it cannot be opened.
  std::optional<diag::Diagnostic> open_log() {
    if (options_.no_log) {
      return std::nullopt;
    }
    const Path dir = options_.log_dir.value_or(platform::default_storage_dir() / "logs");
    platform::SessionLog log = platform::install_session_log(services_, dir, "hefesto-cli", codes::kCliLog);
    logger_ = log.logger;
    return log.warning;
  }

  void log(diag::Severity level, const std::string& message) {
    if (logger_ != nullptr) {
      logger_->write(level, message);
    }
  }

  // app-startup: settings layers, startup files, technology, file arguments.
  void run_startup(const app::StartupOptions& startup) {
    if (startup.technology) {
      log(diag::Severity::info, "tech " + *startup.technology);
    }
    const auto result =
        app::run_startup(*dispatcher_, project_, services_.files(), startup, environment(), console_sink());
    if (!result) {
      dispatcher_->report(result.error());
    }
  }

  app::StartupEnvironment environment() {
    app::StartupEnvironment env;
    env.host = install_host_.get();
    env.install_root = app::resolve_install_root(install_host_->environment("CAD_ROOT"), Path{});
    env.home = install_host_->home_directory();
    std::error_code ec;
    env.working_directory = std::filesystem::current_path(ec);
    env.project_folder = options_.project;
    if (!env.project_folder && services_.files().exists(env.working_directory / std::string{app::kProjectFileName})) {
      env.project_folder = env.working_directory;
    }
    env.pdk_root = options_.pdk_root;
    env.user_preferences = platform::default_storage_dir() / "preferences.json";
    env.recent_projects = platform::default_storage_dir() / "recent-projects.json";
    return env;
  }

  // Runs every source in order; stops at an aborted script or `quit`.
  // In --magic mode without -c or scripts, commands come from standard input.
  bool run_sources() {
    std::vector<ScriptSource> sources = options_.sources;
    if (options_.magic_mode && sources.empty()) {
      sources.push_back({ScriptSource::Kind::standard_input, "-"});
    }
    bool ok = true;
    for (const auto& source : sources) {
      const auto run = run_source(source);
      ok = ok && run.ok();
      if (run.aborted || run.quit) {
        break;
      }
    }
    return ok;
  }

  cmd::ScriptRun run_source(const ScriptSource& source) {
    using Kind = ScriptSource::Kind;
    if (source.kind == Kind::inline_text) {
      return cmd::run_script(*dispatcher_, source.value, cmd::Origin::cli, "-c");
    }
    const auto text = load_source(source);
    if (!text) {
      cmd::ScriptRun failed;
      failed.aborted = true;
      return failed;
    }
    return cmd::run_script(*dispatcher_, *text, cmd::Origin::script, source.value);
  }

  std::optional<std::string> load_source(const ScriptSource& source) {
    if (source.kind == ScriptSource::Kind::standard_input) {
      auto text = read_stdin();
      if (!text) {
        dispatcher_->report(diag::make(codes::kCliScript, "cannot read script from standard input"));
      }
      return text;
    }
    auto text = services_.files().read(source.value);
    if (!text) {
      dispatcher_->report(diag::make(codes::kCliScript, "cannot read script: " + text.error().text));
      return std::nullopt;
    }
    return std::move(text).value();
  }

  const Options& options_;
  platform::PortableServices services_;
  platform::Logger* logger_ = nullptr;
  std::optional<diag::Diagnostic> log_warning_;
  settings::ProjectSettings project_;
  cmd::ScriptSession script_session_{project_};
  cmd::CommandRegistry registry_;
  editor::EditorState editor_;
  std::unique_ptr<pdk::InstallHost> install_host_ = pdk::make_local_install_host();
  std::unique_ptr<pdk::Fetcher> pdk_fetcher_;
  std::unique_ptr<pdk::PdkManager> pdk_manager_;  // outlives every use of the store (the dispatcher dies first)
  StreamSink sink_;
  std::unique_ptr<cmd::CommandLog> command_log_;
  ValuePrinter values_;
  MagicConsole magic_console_;
  std::unique_ptr<cmd::Dispatcher> dispatcher_;
};

int report_usage_error(const std::string& text, std::FILE* err) {
  print_line(err, diag::format_text(diag::make(codes::kCliUsage, text)));
  print_line(err, "try 'hefesto-cli --help'");
  return kExitUsage;
}

// --help, --version, --diagnostic-codes; nullopt when none was given.
std::optional<int> run_informational(const Options& options, std::FILE* out) {
  if (options.show_help) {
    std::fputs(usage_text().c_str(), out);
    return kExitOk;
  }
  if (options.show_version) {
    print_line(out, "hefesto-cli " + std::string{version()} + " (Magic " + std::string{kMagicCompatVersion} +
                        " compatible)");
    return kExitOk;
  }
  if (options.show_codes) {
    std::fputs(diag::render_code_table_markdown(options.codes_area).c_str(), out);
    return kExitOk;
  }
  return std::nullopt;
}

// The startup options: Magic's command line in --magic mode, else -T and
// the layout files. A Magic option or display error prints Magic's text
// and fails with status 1 (app-startup "Command-line options").
std::optional<app::StartupOptions> startup_options(const Options& options, std::FILE* err) {
  app::StartupOptions startup;
  if (options.magic_mode) {
    const std::vector<std::string_view> words(options.magic_args.begin(), options.magic_args.end());
    auto parsed = app::parse_startup_options(words);
    auto display = parsed ? app::resolve_display(parsed.value().display_type, false)
                          : diag::Outcome<app::DisplayChoice>{parsed.error()};
    if (!display) {
      print_line(err, display.error().text);
      return std::nullopt;
    }
    startup = std::move(parsed).value();
  } else {
    startup.technology = options.tech;
    for (const auto& file : options.files) {
      startup.files.push_back(app::classify_file_argument(file));
    }
  }
  startup.session_settings.insert(startup.session_settings.end(), options.settings.begin(), options.settings.end());
  return startup;
}

}  // namespace

int run(std::span<const std::string_view> args, std::FILE* out, std::FILE* err) {
  const ParsedArgs parsed = parse_args(args);
  if (parsed.error) {
    return report_usage_error(*parsed.error, err);
  }
  if (const auto status = run_informational(parsed.options, out)) {
    return *status;
  }
  const auto startup = startup_options(parsed.options, err);
  if (!startup) {
    return kExitFailure;
  }
  Session session{parsed.options, out, err};
  return session.run(*startup);
}

}  // namespace hefesto::cli
