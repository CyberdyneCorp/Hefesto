// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "app.hpp"

#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>

#include "hefesto/cmd/builtins.hpp"
#include "hefesto/cmd/command_log.hpp"
#include "hefesto/cmd/script.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/diag/format.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/platform/session_log.hpp"
#include "hefesto/version.hpp"
#include "options.hpp"
#include "posix_process_runner.hpp"

namespace hefesto::cli {

namespace {

namespace codes = diag::codes;
using platform::Path;

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
        values_(out) {
    log_warning_ = open_log();
    services_.set_process_runner(std::make_unique<PosixProcessRunner>());
    cmd::register_builtins(registry_);
    command_log_ = std::make_unique<cmd::CommandLog>(logger_);
    dispatcher_ = std::make_unique<cmd::Dispatcher>(registry_, services_.view(), project_, script_session_);
    dispatcher_->add_sink(sink_);
    dispatcher_->add_observer(*command_log_);
    dispatcher_->add_observer(values_);
  }

  int run() {
    log(diag::Severity::info, "session start hefesto-cli " + std::string{version()});
    if (log_warning_) {
      dispatcher_->report(*log_warning_);
    }
    record_tech();
    const bool ok = run_sources();
    const int status = ok ? kExitOk : kExitFailure;
    log(diag::Severity::info, "session end exit=" + std::to_string(status));
    return status;
  }

 private:
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

  void record_tech() {
    if (!options_.tech) {
      return;
    }
    log(diag::Severity::info, "tech " + *options_.tech);
    dispatcher_->report(diag::make(codes::kCliTechNotImplemented,
                                   "technology '" + *options_.tech + "' recorded; loading arrives in M1"));
  }

  // Runs every source in order; stops at an aborted script or `quit`.
  bool run_sources() {
    bool ok = true;
    for (const auto& source : options_.sources) {
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
  StreamSink sink_;
  std::unique_ptr<cmd::CommandLog> command_log_;
  ValuePrinter values_;
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
    print_line(out, "hefesto-cli " + std::string{version()});
    return kExitOk;
  }
  if (options.show_codes) {
    std::fputs(diag::render_code_table_markdown().c_str(), out);
    return kExitOk;
  }
  return std::nullopt;
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
  Session session{parsed.options, out, err};
  return session.run();
}

}  // namespace hefesto::cli
