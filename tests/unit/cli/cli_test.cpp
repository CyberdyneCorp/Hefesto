// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "app.hpp"
#include "options.hpp"
#include "posix_process_runner.hpp"

namespace cli = hefesto::cli;
using Args = std::vector<std::string_view>;

#define CLI_TAGS "[cap:hefesto-architecture][req:Headless command-line tool]"
#define PRESENTATION_TAGS "[cap:hefesto-diagnostics][req:Diagnostic presentation and logging]"
#define SERVICES_TAGS "[cap:hefesto-architecture][req:Platform services abstraction]"

namespace {

std::string slurp(std::FILE* f) {
  std::fflush(f);
  std::rewind(f);
  std::string text;
  char buffer[4096];
  std::size_t n = 0;
  while ((n = std::fread(buffer, 1, sizeof buffer, f)) > 0) {
    text.append(buffer, n);
  }
  return text;
}

struct Run {
  int status;
  std::string out;
  std::string err;
};

Run run_cli(Args args) {
  std::FILE* out = std::tmpfile();
  std::FILE* err = std::tmpfile();
  REQUIRE(out != nullptr);
  REQUIRE(err != nullptr);
  const int status = cli::run(args, out, err);
  Run r{status, slurp(out), slurp(err)};
  std::fclose(out);
  std::fclose(err);
  return r;
}

class ScratchDir {
 public:
  ScratchDir() {
    std::random_device rd;
    path_ = std::filesystem::temp_directory_path() / ("hefesto-cli-test-" + std::to_string(rd()));
    std::filesystem::create_directories(path_);
  }
  ScratchDir(const ScratchDir&) = delete;
  ScratchDir& operator=(const ScratchDir&) = delete;
  ~ScratchDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }
  [[nodiscard]] std::string str() const { return path_.string(); }
  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

std::string read_file(const std::filesystem::path& p) {
  std::ifstream in(p);
  std::stringstream s;
  s << in.rdbuf();
  return s.str();
}

}  // namespace

TEST_CASE("argument parsing", CLI_TAGS) {
  const Args args = {"-T", "sky130A", "-c", "echo a", "run.tcl", "--diagnostics=json", "--log-dir", "/tmp/x", "-",
                     "--", "-weird.tcl"};
  const auto parsed = cli::parse_args(args);
  REQUIRE_FALSE(parsed.error.has_value());
  const auto& o = parsed.options;
  CHECK(o.tech == "sky130A");
  CHECK(o.diagnostics == hefesto::diag::Format::json);
  CHECK(o.log_dir == std::filesystem::path{"/tmp/x"});
  using Kind = cli::ScriptSource::Kind;
  CHECK(o.sources == std::vector<cli::ScriptSource>{{Kind::inline_text, "echo a"},
                                                    {Kind::file, "run.tcl"},
                                                    {Kind::standard_input, "-"},
                                                    {Kind::file, "-weird.tcl"}});
  CHECK(cli::parse_args(Args{"--diagnostics", "text"}).options.diagnostics == hefesto::diag::Format::text);
  CHECK(cli::parse_args(Args{"--diagnostics=xml"}).error.has_value());
  CHECK(cli::parse_args(Args{"-c"}).error.has_value());
  CHECK(cli::parse_args(Args{"--frobnicate"}).error.has_value());
}

TEST_CASE("exit status: 0 success, 1 command failure, 2 usage", CLI_TAGS) {
  CHECK(run_cli({"--no-log", "-c", "echo hi; version"}).status == cli::kExitOk);
  CHECK(run_cli({"--no-log", "-c", "nosuch"}).status == cli::kExitFailure);
  CHECK(run_cli({"--no-log", "-c", "echo \"open"}).status == cli::kExitFailure);
  CHECK(run_cli({"--no-log", "/nonexistent/script.tcl"}).status == cli::kExitFailure);
  CHECK(run_cli({"--bogus"}).status == cli::kExitUsage);
  CHECK(run_cli({"--version"}).status == cli::kExitOk);
}

TEST_CASE("results go to stdout, diagnostics to stderr", PRESENTATION_TAGS) {
  const Run r = run_cli({"--no-log", "-c", "echo hello; setting get magic-compat; nosuch; echo never"});
  CHECK(r.out == "hello\non\n");
  CHECK(r.err == "error: CMD-DISPATCH-001: unknown command \"nosuch\" at -c:1\n");
}

TEST_CASE("JSON diagnostics option prints JSON lines", PRESENTATION_TAGS) {
  const Run r = run_cli({"--no-log", "--diagnostics=json", "-c", "nosuch"});
  CHECK(r.err ==
        "{\"severity\":\"error\",\"code\":\"CMD-DISPATCH-001\",\"text\":\"unknown command \\\"nosuch\\\"\","
        "\"location\":{\"file\":\"-c\",\"line\":1},\"related\":null}\n");
}

TEST_CASE("the tech option is accepted and reported as not yet implemented", CLI_TAGS) {
  const Run r = run_cli({"--no-log", "-T", "sky130A", "-c", "echo ok"});
  CHECK(r.status == cli::kExitOk);  // a warning, not a failure
  CHECK(r.out == "ok\n");
  CHECK(r.err.find("warning: CLI-TECH-001: technology 'sky130A' recorded") == 0);
}

TEST_CASE("script files run in order and quit stops the run", CLI_TAGS) {
  ScratchDir dir;
  const auto script = dir.path() / "run.tcl";
  std::ofstream(script) << "# a comment\necho one\nquit\necho never\n";
  const std::string path = script.string();
  const Run r = run_cli({"--no-log", "-c", "echo zero", path, "-c", "echo after"});
  CHECK(r.status == cli::kExitOk);
  CHECK(r.out == "zero\none\n");
}

TEST_CASE("a failing command fails the run but the script continues", CLI_TAGS) {
  const Run r = run_cli({"--no-log", "-c", "setting set nope on; echo still"});
  CHECK(r.status == cli::kExitFailure);
  CHECK(r.out == "still\n");
  CHECK(r.err.find("SETTINGS-NAME-001") != std::string::npos);
}

// Regression: reading a directory used to "succeed" with an empty script.
TEST_CASE("a directory given as a script file fails the run", CLI_TAGS) {
  ScratchDir dir;
  const Run r = run_cli({"--no-log", dir.str()});
  CHECK(r.status == cli::kExitFailure);
  CHECK(r.err.find("CLI-SCRIPT-001") != std::string::npos);
  CHECK(r.err.find("PLATFORM-IO-001") == std::string::npos);  // folded into the CLI diagnostic text
  CHECK(r.err.find("not a regular file") != std::string::npos);
}

// Regression: the usage error was an ordinary failure, so the script went on.
TEST_CASE("hefesto::strict with a bad argument stops the script", "[cap:hefesto-diagnostics][req:Script strict mode switch]") {
  const Run r = run_cli({"--no-log", "-c", "hefesto::strict maybe; echo continued"});
  CHECK(r.status == cli::kExitFailure);
  CHECK(r.out.empty());
  CHECK(r.err.find("CMD-USAGE-001") != std::string::npos);
}

TEST_CASE("each session keeps a rotating log in the app storage", PRESENTATION_TAGS) {
  ScratchDir dir;
  const std::string log_dir = (dir.path() / "logs").string();
  const Run r = run_cli({"--log-dir", log_dir, "-c", "echo logged; nosuch"});
  CHECK(r.status == cli::kExitFailure);
  const std::string log = read_file(dir.path() / "logs" / "hefesto-cli.log");
  CHECK(log.find("info session start hefesto-cli") != std::string::npos);
  CHECK(log.find("info cmd[cli] echo logged -> ok") != std::string::npos);
  CHECK(log.find("error error: CMD-DISPATCH-001") != std::string::npos);
  CHECK(log.find("info session end exit=1") != std::string::npos);
}

TEST_CASE("an unwritable log directory is a warning, not a failure", PRESENTATION_TAGS) {
  ScratchDir dir;
  std::ofstream(dir.path() / "file") << "x";
  const std::string bad = (dir.path() / "file" / "logs").string();
  const Run r = run_cli({"--log-dir", bad, "-c", "echo ok"});
  CHECK(r.status == cli::kExitOk);
  CHECK(r.err.find("warning: CLI-LOG-001") == 0);
}

TEST_CASE("help and diagnostic-codes options", CLI_TAGS) {
  CHECK(run_cli({"--help"}).out.find("usage: hefesto-cli") == 0);
  CHECK(run_cli({"--diagnostic-codes"}).out.find("| `CMD-DISPATCH-001` | error |") != std::string::npos);
}

TEST_CASE("POSIX process runner", SERVICES_TAGS) {
  cli::PosixProcessRunner runner;
  CHECK(runner.supported());
  const auto ok = runner.run({.program = "sh", .args = {"-c", "cat; echo err >&2; exit 3"}, .working_directory = {},
                              .stdin_data = "piped\n"});
  REQUIRE(ok.ok());
  CHECK(ok.value().exit_code == 3);
  CHECK(ok.value().stdout_data == "piped\n");
  CHECK(ok.value().stderr_data == "err\n");

  const auto pwd = runner.run({.program = "pwd", .args = {}, .working_directory = std::filesystem::path{"/"},
                               .stdin_data = {}});
  REQUIRE(pwd.ok());
  CHECK(pwd.value().stdout_data == "/\n");

  const auto missing = runner.run({.program = "hefesto-no-such-program", .args = {}, .working_directory = {},
                                   .stdin_data = {}});
  REQUIRE_FALSE(missing.ok());
  CHECK(missing.error().code == "PLATFORM-PROCESS-001");
}

// Regression: pipe fds were not close-on-exec. Each child kept extra copies of
// its own pipe ends besides 0-2, and a child spawned by another thread could
// inherit (and hold open) another run's write ends, so that run never saw EOF.
TEST_CASE("POSIX process runner: children inherit no pipe fds besides 0, 1 and 2", SERVICES_TAGS) {
  cli::PosixProcessRunner runner;
  const char* script =
      "n=0; for f in /dev/fd/*; do case $f in /dev/fd/0|/dev/fd/1|/dev/fd/2) ;; "
      "*) if [ -p \"$f\" ]; then n=$((n+1)); fi ;; esac; done; echo $n";
  const auto r = runner.run({.program = "sh", .args = {"-c", script}, .working_directory = {}, .stdin_data = {}});
  REQUIRE(r.ok());
  CHECK(r.value().stdout_data == "0\n");
}
