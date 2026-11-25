// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Security review, task 12.1 (design D14), finding S8-04: the process's
// standard channels are shared by every interpreter of the thread, so a
// schematic script (a Tcl symbol generator, a `tcleval` value, an
// `xschemrc`, all run without a prompt) must neither close them (the next
// file the host opens would take the freed descriptor) nor write past the
// generator's output cap through `chan puts`. Both modes of `xschem-compat`
// behave the same (hardening; none of these paths reads the mode).
//
// Owner: SECURITY (S8-04 regression tests: SIMINT, m8 round 5).

#include <catch2/catch_test_macros.hpp>

#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/lib/generator.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "hefesto/schem/lib/xschemrc.hpp"
#include "hefesto/schem/tcl/tcl_bridge.hpp"
#include "hefesto/schem/tcl/tcl_environment.hpp"
#include "hefesto/script/tcl/tcl_interpreter.hpp"
#include "security_support.hpp"

namespace schem = hefesto::schem;
namespace diag = hefesto::diag;
namespace platform = hefesto::platform;
namespace script = hefesto::script;
namespace sec = hefesto::test::security;
using std::chrono::milliseconds;

namespace {

/// Keeps a copy of the process's descriptors 0, 1 and 2 and puts back any
/// that a test closed, so that a run without the fix leaves the test
/// process usable.
class StandardDescriptors {
 public:
  StandardDescriptors() {
    for (std::size_t i = 0; i < saved_.size(); ++i) saved_[i] = ::dup(static_cast<int>(i));
  }
  ~StandardDescriptors() {
    for (std::size_t i = 0; i < saved_.size(); ++i) {
      if (saved_[i] < 0) continue;
      if (!open(static_cast<int>(i))) ::dup2(saved_[i], static_cast<int>(i));
      ::close(saved_[i]);
    }
  }
  StandardDescriptors(const StandardDescriptors&) = delete;
  StandardDescriptors& operator=(const StandardDescriptors&) = delete;

  [[nodiscard]] static bool open(int fd) { return ::fcntl(fd, F_GETFD) != -1; }
  [[nodiscard]] static bool all_open() { return open(0) && open(1) && open(2); }

 private:
  std::array<int, 3> saved_{-1, -1, -1};
};

/// Sends the process's standard output to /dev/null while it lives (a run
/// without the fix writes there).
class QuietStdout {
 public:
  QuietStdout() : saved_(::dup(1)) {
    const int null = ::open("/dev/null", O_WRONLY);
    if (null >= 0) {
      ::dup2(null, 1);
      ::close(null);
    }
  }
  ~QuietStdout() {
    if (saved_ >= 0) {
      ::dup2(saved_, 1);
      ::close(saved_);
    }
  }
  QuietStdout(const QuietStdout&) = delete;
  QuietStdout& operator=(const QuietStdout&) = delete;

 private:
  int saved_;
};

/// The ways S8-04 reached a standard channel; each is refused with
/// SCRIPT-SANDBOX-007.
const std::vector<std::string>& channel_escapes() {
  static const std::vector<std::string> scripts = {
      "close stdout",
      "close stderr",
      "close stdin",
      "chan close stdout",
      "chan close stdout write",
      "chan pop stdout",
      "chan push stdout ::hefesto_transform",
      "interp create -safe s; interp transfer {} stdout s; s eval {close stdout}",
      "interp create -safe s; interp share {} stderr s",
  };
  return scripts;
}

/// Puts a pipe holding "typed\n" (then end of file) on the process's
/// descriptor 0 while it lives, so a read of stdin that is not refused
/// returns at once with that text instead of waiting.
class TypedStdin {
 public:
  TypedStdin() : saved_(::dup(0)) {
    std::array<int, 2> ends{-1, -1};
    if (::pipe(ends.data()) != 0) return;
    constexpr std::string_view kTyped = "typed\n";
    (void)!::write(ends[1], kTyped.data(), kTyped.size());
    ::close(ends[1]);
    ::dup2(ends[0], 0);
    ::close(ends[0]);
  }
  ~TypedStdin() {
    if (saved_ >= 0) {
      ::dup2(saved_, 0);
      ::close(saved_);
    }
  }
  TypedStdin(const TypedStdin&) = delete;
  TypedStdin& operator=(const TypedStdin&) = delete;

 private:
  int saved_;
};

/// The reads of stdin a schematic script is refused (SCRIPT-SANDBOX-007).
const std::vector<std::string>& stdin_reads() {
  static const std::vector<std::string> scripts = {
      "gets stdin",       "read stdin 5",        "read -nonewline stdin",  "chan gets stdin",
      "chan read stdin 5", "fcopy stdin stdout -size 1", "chan copy stdin stdout -size 1",
  };
  return scripts;
}

std::vector<diag::Diagnostic> removed_denials(const std::vector<diag::Diagnostic>& all) {
  return sec::with_code_prefix(all, "SCRIPT-SANDBOX-007");
}

}  // namespace

// Regression (review 12.1, S8-04): `close stdout` in a generator closed the
// process's file descriptor 1.
TEST_CASE("S8-04: a symbol generator cannot close the standard channels", sec::kSandbox) {
  for (const std::string& escape : channel_escapes()) {
    INFO(escape);
    const StandardDescriptors keep;
    sec::TempFolder library;
    library.write("gen.tcl", escape + "\nputs {v {xschem version=3.4.8RC file_version=1.2}}\n");
    platform::LocalFileSystem fs;
    const auto output = schem::run_tcl_generator(fs, {library.path() / "gen.tcl", {}, library.path()});
    CHECK(StandardDescriptors::all_open());
    CHECK_FALSE(output.text.has_value());
    CHECK(removed_denials(output.diagnostics).size() == 1);
  }
}

TEST_CASE("S8-04: a tcleval value cannot close the standard channels", sec::kSandbox) {
  for (const std::string& escape : channel_escapes()) {
    INFO(escape);
    const StandardDescriptors keep;
    sec::TempFolder document;
    hefesto::editor::EditorState editor;
    platform::LocalFileSystem files;
    script::SandboxPolicy policy;
    policy.read_only_roots.push_back(document.path());
    schem::TclEnvironment tcl(editor, policy, files);
    diag::CollectingSink sink;
    tcl.set_diagnostics(&sink);
    schem::TclContext context;
    context.owner = "instance R1 (evil.sym)";
    const std::string value = tcl.bridge().evaluate("tcleval([" + escape + "])", context);
    CHECK(StandardDescriptors::all_open());
    CHECK(value == "?\n");
    CHECK(removed_denials(sink.take()).size() == 1);
  }
}

TEST_CASE("S8-04: an xschemrc cannot close the standard channels", sec::kSandbox) {
  for (const std::string& escape : channel_escapes()) {
    INFO(escape);
    const StandardDescriptors keep;
    sec::TempFolder document;
    document.write("xschemrc", "set BEFORE 1\ncatch {" + escape + "}\nset AFTER 1\n");
    platform::LocalFileSystem fs;
    schem::XschemrcInput input;
    input.document_folder = document.path();
    const schem::XschemrcResult result = schem::SearchPath::from_xschemrc(fs, input);
    CHECK(StandardDescriptors::all_open());
    CHECK(result.variables.count("AFTER") == 1);
    CHECK(removed_denials(result.diagnostics).size() == 1);
  }
}

// Regression (review 12.1, S8-04): `chan puts stdout` went past the
// generator's `puts` capture to the process's output, bounded only by the
// time limit. It is now `puts`: captured and capped.
TEST_CASE("S8-04: chan puts in a generator is captured and bounded", sec::kPermissionModel) {
  const QuietStdout quiet;
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/lib/chan.tcl", "chan puts stdout {v {xschem version=3.4.8RC file_version=1.2}}\n"
                                           "chan puts stderr dropped\n")
              .ok());
  REQUIRE(fs.write_atomic("/lib/flood.tcl", "set x [string repeat x 65536]\n"
                                            "for {set i 0} {$i < 100} {incr i} { catch { chan puts stdout $x } }\n")
              .ok());
  const auto captured = schem::run_tcl_generator(fs, {"/lib/chan.tcl", {}, "/w"});
  REQUIRE(captured.text.has_value());
  CHECK(*captured.text == "v {xschem version=3.4.8RC file_version=1.2}\n");
  schem::GeneratorInput flood{"/lib/flood.tcl", {}, "/w"};
  flood.max_output = std::size_t{1} << 20;
  flood.time_limit = milliseconds{5000};
  const auto flooded = schem::run_tcl_generator(fs, flood);
  CHECK_FALSE(flooded.text.has_value());
  REQUIRE_FALSE(flooded.diagnostics.empty());
  CHECK(flooded.diagnostics[0].text.ends_with("it printed more than 1048576 bytes"));
}

// Regression (review 12.1, S8-04): `gets stdin` (or `read stdin`) in a
// schematic script waited for input inside one command, where neither the
// time limit nor the Interrupt reaches, so opening a schematic or listing a
// symbol could hang with no prompt. Refused in all three interpreters.
TEST_CASE("S8-04: a symbol generator cannot read stdin", sec::kSandbox) {
  for (const std::string& read : stdin_reads()) {
    INFO(read);
    const TypedStdin typed;
    const QuietStdout quiet;
    platform::MemoryFileSystem fs;
    REQUIRE(fs.write_atomic("/lib/gen.tcl", read + "\nputs {v {xschem version=3.4.8RC file_version=1.2}}\n").ok());
    const auto output = schem::run_tcl_generator(fs, {"/lib/gen.tcl", {}, "/w"});
    CHECK_FALSE(output.text.has_value());
    CHECK(removed_denials(output.diagnostics).size() == 1);
  }
}

TEST_CASE("S8-04: a tcleval value cannot read stdin", sec::kSandbox) {
  for (const std::string& read : stdin_reads()) {
    INFO(read);
    const TypedStdin typed;
    const QuietStdout quiet;
    hefesto::editor::EditorState editor;
    platform::MemoryFileSystem files;
    schem::TclEnvironment tcl(editor, script::SandboxPolicy{}, files);
    diag::CollectingSink sink;
    tcl.set_diagnostics(&sink);
    schem::TclContext context;
    context.owner = "instance R1 (evil.sym)";
    CHECK(tcl.bridge().evaluate("tcleval([" + read + "])", context) == "?\n");
    CHECK(removed_denials(sink.take()).size() == 1);
  }
}

TEST_CASE("S8-04: an xschemrc cannot read stdin", sec::kSandbox) {
  for (const std::string& read : stdin_reads()) {
    INFO(read);
    const TypedStdin typed;
    const QuietStdout quiet;
    sec::TempFolder document;
    document.write("xschemrc", "catch {set READ [" + read + "]}\nset AFTER 1\n");
    platform::LocalFileSystem fs;
    schem::XschemrcInput input;
    input.document_folder = document.path();
    const schem::XschemrcResult result = schem::SearchPath::from_xschemrc(fs, input);
    CHECK(result.variables.count("AFTER") == 1);
    CHECK(result.variables.count("READ") == 0);
    CHECK(removed_denials(result.diagnostics).size() == 1);
  }
}

// The console's project interpreter is the user's own: it keeps stdin.
TEST_CASE("S8-04: the project interpreter still reads stdin", sec::kSandbox) {
  const TypedStdin typed;
  hefesto::editor::EditorState editor;
  auto interp = script::create_project_interpreter(editor, script::SandboxPolicy{});
  const auto result = interp->evaluate("gets stdin");
  REQUIRE(result.ok());
  CHECK(result.value.to_string() == "typed");
  interp.reset();
}
