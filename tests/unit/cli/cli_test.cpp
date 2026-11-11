// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "app.hpp"
#include "hefesto/platform_posix/process_runner.hpp"
#include "options.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

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

// -c text is Tcl: each complete command's non-empty result is printed, and a
// Tcl error stops it like `source` (m4-scripting tasks.md 13.2).
TEST_CASE("results go to stdout, diagnostics to stderr", PRESENTATION_TAGS) {
  const Run r = run_cli({"--no-log", "-c", "echo hello\nsetting get magic-compat\nnosuch\necho never"});
  CHECK(r.out == "hello\non\n");
  CHECK(r.err == "error: SCRIPT-TCL-001: invalid command name \"nosuch\" at -c:3\n");
}

TEST_CASE("JSON diagnostics option prints JSON lines", PRESENTATION_TAGS) {
  const Run r = run_cli({"--no-log", "--diagnostics=json", "-c", "nosuch"});
  CHECK(r.err ==
        "{\"severity\":\"error\",\"code\":\"SCRIPT-TCL-001\",\"text\":\"invalid command name \\\"nosuch\\\"\","
        "\"location\":{\"file\":\"-c\",\"line\":1},\"related\":null}\n");
}

TEST_CASE("Magic console diagnostics print each script command's result like puts", PRESENTATION_TAGS) {
  const Run r = run_cli({"--no-log", "--diagnostics=magic", "-c",
                         "puts [setting get magic-compat]\ncatch {nosuch} result; puts $result\nnosuch\nputs never"});
  // An unknown command is a Tcl error: its message is the result. The reference lines catch it
  // (`catch {cmd} result; puts $result`, Q-UI-41); an uncaught one stops -c and prints its message.
  CHECK(r.out == "on\ninvalid command name \"nosuch\"\n");
  CHECK(r.err == "invalid command name \"nosuch\"\n");
  CHECK(cli::parse_args(Args{"--diagnostics=magic"}).options.diagnostics == hefesto::diag::Format::magic);
}

// Regression (TECH-G41): an ordinary command error is TxError text on stderr
// and the Tcl result stays empty, so `puts` prints an empty line on stdout.
TEST_CASE("Magic console prints an ordinary command error on stderr and an empty result", PRESENTATION_TAGS) {
  const Run r = run_cli({"--no-log", "--diagnostics=magic", "-c",
                         "catch {tech foo} result; puts <$result>\ncatch {tech la} result; puts <$result>\nputs after"});
  CHECK(r.out == "<>\n<>\nafter\n");
  CHECK(r.err ==
        "\"foo\" isn't a valid tech option.  Type \":tech help\" for help.\n"
        "Ambiguous techinfo option: \"la\"\n  Type \":tech help\" for help.\n");
}

// Regression (TECH-G01/G31): Magic's Tcl `tech load` sets an explicit result
// on failure (no period, trailing LF) and prints no TxError line for it.
TEST_CASE("Magic console prints the tech load failure result on stdout", PRESENTATION_TAGS) {
  const ScratchDir dir;
  {
    std::ofstream bad(dir.path() / "bad.tech");
    bad << "tech\n  format 35\n  bad\nend\n";  // no planes section: the full load fails
  }
  const std::string missing = (dir.path() / "nothere").string();
  const std::string broken = (dir.path() / "bad").string();
  const Run r = run_cli({"--no-log", "--diagnostics=magic", "-c",
                         "catch {tech load " + missing + " -noprompt} result; puts $result\n" +
                             "catch {tech load " + broken + " -noprompt} result; puts $result"});
  CHECK(r.out ==
        "Technology file does not exist or is not readable\n\n"
        "Error in loading technology file\n\n");
  CHECK(r.err.find("Could not find file '" + missing + ".tech' in any of these directories:") == 0);
  CHECK(r.err.find("does not exist or is not readable") == std::string::npos);
  CHECK(r.err.find("Error in loading") == std::string::npos);
}

// Regression for the M1 migration: -T used to only record the technology
// (CLI-TECH-001); it now loads it at startup and a failure is a warning.
TEST_CASE("the tech option loads the technology at startup", "[cap:app-startup][req:Startup file order]") {
  const Run r = run_cli({"--no-log", "-T", "no_such_technology", "-c", "echo ok; tech name"});
  CHECK(r.status == cli::kExitOk);  // startup continues with the minimum technology
  CHECK(r.out == "ok\nminimum\n");
  CHECK(r.err.find("CLI-TECH-001") == std::string::npos);
  CHECK(r.err.find("warning: APP-TECH-001: Failed to load technology \"no_such_technology\"") != std::string::npos);
}

TEST_CASE("layout files on the command line are startup file arguments",
          "[cap:app-startup][req:Command-line file arguments]") {
  const auto parsed = cli::parse_args(Args{"inv.mag", "run.tcl", "top.GDS", "x.lef", "dir.mag/script"});
  REQUIRE_FALSE(parsed.error.has_value());
  CHECK(parsed.options.files == std::vector<std::string>{"inv.mag", "top.GDS", "x.lef"});
  REQUIRE(parsed.options.sources.size() == 2);
  CHECK(parsed.options.sources[1].value == "dir.mag/script");
  const Run r = run_cli({"--no-log", "/nonexistent/inv.mag", "-c", "echo after"});
  CHECK(r.out == "after\n");
  CHECK(r.err.find("Loading \"/nonexistent/inv\" from command line.") != std::string::npos);
}

TEST_CASE("the magic option takes Magic's command line", "[cap:app-startup][req:Command-line options]") {
  const auto parsed = cli::parse_args(Args{"--no-log", "--magic", "-dnull", "-noconsole", "-T", "scmos", "a.mag"});
  REQUIRE_FALSE(parsed.error.has_value());
  CHECK(parsed.options.magic_mode);
  CHECK(parsed.options.magic_args == std::vector<std::string>{"-dnull", "-noconsole", "-T", "scmos", "a.mag"});
  const Run unknown = run_cli({"--no-log", "--magic", "-x"});
  CHECK(unknown.status == 1);
  CHECK(unknown.err.starts_with("Unknown option: '-x'\nUsage:  magic"));
  const Run display = run_cli({"--no-log", "--magic", "-d", "FOO"});
  CHECK(display.status == 1);
  CHECK(display.err.find("Use '-d NULL' if you don't need graphics.") != std::string::npos);
  // Magic's script host passes `-T` with the empty string (`lindex` past the
  // end), so Magic 8.3.694's Tcl build fails to load technology "" and
  // starts with "minimum" (checked with `magic -dnull -noconsole -T`).
  const Run missing = run_cli({"--no-log", "-c", "tech name", "--magic", "-dnull", "-noconsole", "-T"});
  CHECK(missing.status == 0);
  CHECK(missing.err.find("Failed to load technology \"\"") != std::string::npos);
  CHECK(missing.out == "minimum\n");
  const Run ok = run_cli({"--no-log", "-c", "tech name", "--magic", "-dnull", "-noconsole"});
  CHECK(ok.status == 0);
  CHECK(ok.out == "minimum\n");
}

TEST_CASE("the version option names the Magic compatibility version", "[cap:app-startup][req:Tcl launcher options]") {
  const Run r = run_cli({"--version"});
  CHECK(r.status == 0);
  CHECK(r.out.find("(Magic 8.3.694 compatible)") != std::string::npos);
}

// The script host's table (scripting-pdk-integration "Command-line options in
// the script host", Q-SCRIPT-37) decides Magic's command line in --magic
// mode; outputs checked against Magic 8.3.694's `magic -dnull -noconsole ...`.
TEST_CASE("the magic option classifies its words with the script host's table",
          "[cap:scripting-pdk-integration][req:Command-line options in the script host]") {
  const Run version = run_cli({"--no-log", "--magic", "-dnull", "-noconsole", "-T", "scmos", "--version", "late"});
  CHECK(version.status == 0);
  CHECK(version.out == "8.3.694\n");
  const Run commit = run_cli({"--no-log", "--magic", "-dnull", "--commit"});
  CHECK(commit.status == 0);
  CHECK(commit.out == "fc5daa9e2856b89a0a3056f296ded1fd4474e39e\n");
  const Run prefix = run_cli({"--no-log", "--magic", "--prefix"});
  CHECK(prefix.status == 0);
  CHECK(prefix.err.empty());
  // `-nowrapper` is the script host's and never reaches the engine's parser.
  const Run nowrapper = run_cli({"--no-log", "-c", "tech name", "--magic", "-dnull", "-noconsole", "-nowrapper"});
  CHECK(nowrapper.status == 0);
  CHECK(nowrapper.out == "minimum\n");
  // `^-n*` passes an unknown option to the engine, which rejects it.
  const Run unknown = run_cli({"--no-log", "--magic", "-dnull", "-zz"});
  CHECK(unknown.status == 1);
  CHECK(unknown.err.starts_with("Unknown option: '-zz'"));
}

TEST_CASE("session settings from the command line", "[cap:hefesto-settings][req:Layered settings resolution]") {
  CHECK(cli::parse_args(Args{"--setting", "units=microns"}).options.settings ==
        std::vector<std::pair<std::string, std::string>>{{"units", "microns"}});
  CHECK(cli::parse_args(Args{"--setting", "novalue"}).error.has_value());
  const Run r = run_cli({"--no-log", "--setting", "magic-compat=off", "-c", "setting get magic-compat"});
  CHECK(r.out == "off\n");
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

TEST_CASE("quit ends the session with its exit status", "[cap:app-startup][req:quit command and exit status]") {
  const Run three = run_cli({"--no-log", "-c", "echo before; quit 3 -noprompt; echo never"});
  CHECK(three.status == 3);
  CHECK(three.out == "before\n");
  // A bad status cancels the quit: the script goes on and the run fails.
  const Run bad = run_cli({"--no-log", "-c", "quit 300; echo still"});
  CHECK(bad.status == cli::kExitFailure);
  CHECK(bad.out == "still\n");
  CHECK(bad.err.find("Invalid exit_status: 300") != std::string::npos);
  CHECK(bad.err.find("Usage: quit [exit_status] [-noprompt]") != std::string::npos);
  // A plain quit keeps the failure status of an earlier command.
  CHECK(run_cli({"--no-log", "-c", "nosuch; quit"}).status == cli::kExitFailure);
}

TEST_CASE("headless mode takes a command script from a file argument",
          "[cap:app-startup][req:Batch and headless operation]") {
  ScratchDir dir;
  const auto script = dir.path() / "batch.tcl";
  std::ofstream(script) << "box 0 0 10 10\nbox\nquit -noprompt\necho never\n";
  const std::string path = script.string();
  const Run r = run_cli({"--no-log", "--magic", "-dnull", "-noconsole", path});
  INFO(r.err);
  CHECK(r.status == cli::kExitOk);
  CHECK(r.out.find("never") == std::string::npos);
  // The null display draws nothing but the box is set in the startup window.
  CHECK(r.err.find("Root cell box:") != std::string::npos);
  CHECK(r.err.find("10 x 10") != std::string::npos);
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
  const Run r = run_cli({"--log-dir", log_dir, "-c", "setting get magic-compat; nosuch"});
  CHECK(r.status == cli::kExitFailure);
  const std::string log = read_file(dir.path() / "logs" / "hefesto-cli.log");
  CHECK(log.find("info session start hefesto-cli") != std::string::npos);
  // Through Tcl, with the origin of -c text: typed by the user (security finding S-01).
  CHECK(log.find("info cmd[cli] setting get magic-compat -> ok") != std::string::npos);
  CHECK(log.find("error error: SCRIPT-TCL-001") != std::string::npos);
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
  hefesto::platform_posix::PosixProcessRunner runner;
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
namespace {
// Pipes above fd 2 that this test process already holds without close-on-exec
// (inherited from CTest or the sanitizer runtime). A child inherits these
// regardless of the runner, so they are the baseline.
int inheritable_pipe_count() {
  int count = 0;
  const long max_fd = ::sysconf(_SC_OPEN_MAX);
  for (int fd = 3; fd < max_fd && fd < 4096; ++fd) {
    struct stat st {};
    const int flags = ::fcntl(fd, F_GETFD);
    if (flags != -1 && (flags & FD_CLOEXEC) == 0 && ::fstat(fd, &st) == 0 && S_ISFIFO(st.st_mode)) {
      ++count;
    }
  }
  return count;
}
}  // namespace

TEST_CASE("POSIX process runner: children inherit no pipe fds besides 0, 1 and 2", SERVICES_TAGS) {
  hefesto::platform_posix::PosixProcessRunner runner;
  const std::string baseline = std::to_string(inheritable_pipe_count()) + "\n";
  const char* script =
      "n=0; for f in /dev/fd/*; do case $f in /dev/fd/0|/dev/fd/1|/dev/fd/2) ;; "
      "*) if [ -p \"$f\" ]; then n=$((n+1)); fi ;; esac; done; echo $n";
  const auto r = runner.run({.program = "sh", .args = {"-c", script}, .working_directory = {}, .stdin_data = {}});
  REQUIRE(r.ok());
  CHECK(r.value().stdout_data == baseline);
}

// Regression (golden UI-G-001..004, Q-UI-41): script and -c lines need the exact command name.
TEST_CASE("the CLI resolves command names exactly, like Magic's Tcl build",
          "[cap:ui-command-dispatch][req:Command name lookup][golden:UI-G-004]") {
  const Run r = run_cli({"--no-log", "--diagnostics=magic", "-c",
                         "catch {vers} r; puts $r\ncatch {VERSION} r; puts $r\nputs [magic::echo ok]"});
  CHECK(r.out == "invalid command name \"vers\"\ninvalid command name \"VERSION\"\nok\n");
  CHECK(r.err.empty());
}

// Regression (golden UI-G-060, UI-G-081, Q-UI-43): with no `setpoint`, each script command has the
// point at screen (100, 100) of the window under it, as Magic's null display gives every command.
TEST_CASE("the CLI gives script commands Magic's default point (100, 100)",
          "[cap:ui-command-dispatch][req:pushbutton and setpoint][golden:UI-G-081]") {
  const Run r = run_cli({"--no-log", "--diagnostics=magic", "-c", "load x\nsetpoint\n*winddebug\nbox\n*winddebug"});
  CHECK(r.out.find("100 100 ") != std::string::npos);
  CHECK(r.out.find("Sending command:\n 'box' at (100, 100)\n") != std::string::npos);
  CHECK(r.out.find("Sending command:\n '*winddebug' at (100, 100)\n") != std::string::npos);
}

// Regression (round 3 review): when the default point (100, 100) falls on a window's chrome (here the
// frame of a layout window opened at 95..400), Magic sends the command to the `*window` client
// (windSend.c), so a layout command such as `box` fails quietly with an empty result.
TEST_CASE("a default point on a window's chrome sends the command to the window client",
          "[cap:ui-command-dispatch][req:Command dispatch across global and client tables]") {
  const Run r = run_cli({"--no-log", "--diagnostics=magic", "-c",
                         "load x; closewindow; specialopen 95 95 400 400 layout; catch {box values} r; puts <$r>; echo end"});
  CHECK((r.out + r.err).find("Box tool must be present") == std::string::npos);
  CHECK(r.out.find("invalid command name") == std::string::npos);
  CHECK(r.out.find("<>\nend\n") != std::string::npos);
  // The same command with the point inside the default window reaches the layout client.
  const Run inside = run_cli({"--no-log", "--diagnostics=magic", "-c", "load x; box values"});
  CHECK((inside.out + inside.err).find("Box tool must be present") != std::string::npos);
}

// Regression (round 3 review): a `magic::`-qualified name of another client's command fails quietly
// (not `invalid command name`) and `*winddebug` echoes it with its qualifier, as Magic does.
TEST_CASE("a namespaced command of another client fails quietly and is echoed",
          "[cap:ui-command-dispatch][req:Command dispatch across global and client tables]") {
  const Run r = run_cli({"--no-log", "--diagnostics=magic", "-c", "load x; *winddebug; catch magic::color; echo end"});
  CHECK(r.out.find("invalid command name") == std::string::npos);
  // Magic also prints the `*winddebug` echo of `magic::color` (WindSendCommand prints before its
  // lookup); the binding refuses the name before dispatching it: open item for BINDING (13.2).
  CHECK(r.out.find("end\n") != std::string::npos);
}

// ---- M4: Tcl through the project's interpreter (m4-scripting tasks.md 13.2) --

TEST_CASE("script files are Tcl: each command's result is printed and a Tcl error stops the run with its line",
          "[cap:hefesto-architecture][req:Headless command-line tool]") {
  ScratchDir dir;
  const auto script = dir.path() / "run.hef";
  std::ofstream(script) << "set n [expr {2 * 3}]\nhefesto::debug::hashorder 32 +a +b\nforeach i {1 2} {\n  incr n\n}\n"
                           "set n\nerror boom\nputs never\n";
  const Run r = run_cli({"--no-log", script.string(), "-c", "puts after"});
  CHECK(r.status == cli::kExitFailure);
  CHECK(r.out == "6\nb a\n8\n");  // a later source does not run either
  CHECK(r.err == "error: SCRIPT-TCL-001: boom at " + script.string() + ":7\n");
}

TEST_CASE("-c text continues a command over lines and reports a Tcl error at its line",
          "[cap:hefesto-architecture][req:Headless command-line tool]") {
  const Run r = run_cli({"--no-log", "-c", "set a {x\ny}\nputs [string length $a]\nif {1} {\n  nosuch\n}"});
  CHECK(r.out == "x\ny\n3\n");
  CHECK(r.err == "error: SCRIPT-TCL-001: invalid command name \"nosuch\" at -c:4\n");
}


TEST_CASE("a .tcl file argument in --magic mode runs as Tcl with the magic commands",
          "[cap:scripting-api][req:Global import of magic commands]") {
  ScratchDir dir;
  const auto script = dir.path() / "run.tcl";
  std::ofstream(script) << "set n 0\n"
                           "foreach w {a b c} { incr n }\n"
                           "puts \"n=$n [expr {[llength [info commands ::magic::box]] == 1}]\"\n"
                           "box 0 0 10 20\n"
                           "puts [box values]\n"
                           "puts -nonewline stdout \"[catch {nosuchcommand} message] \"\n"
                           "puts $message\n"
                           "puts stderr to-stderr\n"
                           "puts \"tag=[magic::tag select]\"\n"
                           "quit -noprompt\n"
                           "puts never\n";
  const Run r = run_cli({"--no-log", "--diagnostics=magic", "--magic", "-dnull", "-noconsole", "-norc",
                         script.string()});
  INFO(r.err);
  CHECK(r.status == cli::kExitOk);
  // `puts` goes to the session's streams; engine commands print no result.
  CHECK(r.out.find("n=3 1\n") != std::string::npos);
  CHECK(r.out.find("\n0 0 10 20\n1 invalid command name \"nosuchcommand\"\n") != std::string::npos);
  CHECK(r.out.find("never") == std::string::npos);  // `quit` unwinds the evaluation
  CHECK(r.err.find("to-stderr\n") != std::string::npos);
  // toolkit.tcl's select tag is installed with the interpreter (PCELL 7.3).
  CHECK(r.out.find("tag=magic::gencell_update %1\n") != std::string::npos);
}

TEST_CASE("a Tcl error in a .tcl file argument prints Magic's error text",
          "[cap:scripting-pdk-integration][req:Startup files are scripts]") {
  ScratchDir dir;
  const auto script = dir.path() / "bad.tcl";
  std::ofstream(script) << "puts before\nerror boom\nputs after\n";
  // -c: a source after startup, so standard input is not read.
  const Run r = run_cli({"--no-log", "-c", "quit -noprompt", "--diagnostics=magic", "--magic", "-dnull", "-noconsole",
                         "-norc", script.string()});
  CHECK(r.out.find("before\n") != std::string::npos);
  CHECK(r.out.find("after") == std::string::npos);
  CHECK(r.err.find("Error parsing \"" + script.string() + "\": boom") != std::string::npos);
}

TEST_CASE("the trust option names the PDKs whose scripts may run in batch mode",
          "[cap:hefesto-security][req:Consent before implicit script execution]") {
  const auto parsed = cli::parse_args(Args{"--trust-pdk", "sky130A", "--trust-pdk=gf180mcu", "--magic", "-dnull"});
  REQUIRE_FALSE(parsed.error.has_value());
  CHECK(parsed.options.trust_pdk == std::vector<std::string>{"sky130A", "gf180mcu"});
  CHECK(cli::parse_args(Args{"--trust-pdk"}).error.has_value());

  // A PDK's rc file given with -rcfile asks for consent: declined in batch
  // mode unless --trust-pdk names the PDK.
  ScratchDir dir;
  const auto magic = dir.path() / "sky130A" / "libs.tech" / "magic";
  std::filesystem::create_directories(magic);
  const auto rc = magic / "sky130A.magicrc";
  std::ofstream(rc) << "puts \"rc ran\"\nset VDD VPWR\n";
  const auto script = dir.path() / "probe.tcl";
  std::ofstream(script) << "puts \"VDD=$VDD\"\n";
  const Run declined = run_cli(
      {"--no-log", "-c", "quit -noprompt", "--magic", "-dnull", "-noconsole", "-rcfile", rc.string(), script.string()});
  CHECK(declined.out.find("rc ran") == std::string::npos);
  CHECK(declined.out.find("VDD=vdd!") != std::string::npos);
  CHECK(declined.err.find("SCRIPT-TRUST-001") != std::string::npos);
  const Run trusted = run_cli({"--no-log", "-c", "quit -noprompt", "--trust-pdk", "sky130A", "--magic", "-dnull",
                               "-noconsole", "-rcfile", rc.string(), script.string()});
  CHECK(trusted.out.find("rc ran\n") != std::string::npos);
  CHECK(trusted.out.find("VDD=VPWR") != std::string::npos);
  CHECK(trusted.err.find("SCRIPT-TRUST-001") == std::string::npos);
}

TEST_CASE("the reference-library stripper is a CLI option",
          "[cap:scripting-auxiliary-tools][req:Reference-library stripper]") {
  ScratchDir dir;
  std::ofstream(dir.path() / "a.mag") << "magic\ntech scmos\nstring GDS_START 0\nstring GDS_END 10\n<< end >>\n";
  const Run r = run_cli({"--no-log", "--strip-reflibs", dir.str()});
  CHECK(r.status == cli::kExitOk);
  std::ifstream in(dir.path() / "a.mag");
  const std::string text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
  CHECK(text == "magic\ntech scmos\n<< end >>\n");
}
