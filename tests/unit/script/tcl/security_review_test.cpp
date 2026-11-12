// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The M4 security review of the script sandbox (openspec/changes/m4-scripting
// tasks.md 13.3, design D19; docs/contributing/security.md). Each attack the
// sandbox denies is kept here as a regression test.
//
// The test cases named "security fix S-NN" are the attacks the review found
// not denied; each is the regression test of its fix (docs/contributing/
// security.md, "Findings and fixes").

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "hefesto/script/binding/tags.hpp"
#include "hefesto/script/tcl/sandbox.hpp"
#include "tcl_fixture.hpp"

namespace script = hefesto::script;
namespace fs = std::filesystem;
using hefesto::test::error_text;
using hefesto::test::has_code;
using hefesto::test::SandboxedProject;

#define SANDBOXED "[cap:hefesto-scripting][req:Embedded sandboxed Tcl on all platforms]"
#define ENFORCED "[cap:hefesto-security][req:Script sandbox enforcement]"
#define PER_PROJECT "[cap:hefesto-scripting][req:One interpreter per open project]"

namespace {

// `script` fails with the SCRIPT-SANDBOX-* `code`.
void require_refused(SandboxedProject& p, const std::string& script, const std::string& code) {
  INFO(script);
  const auto result = p.run(script);
  REQUIRE_FALSE(result.ok());
  CHECK(has_code(result, code));
}

// `script` fails (with any error) and is not a sandbox escape.
void require_fails(SandboxedProject& p, const std::string& script) {
  INFO(script);
  CHECK_FALSE(p.run(script).ok());
}

std::string read_file(const fs::path& path) {
  std::ifstream in{path, std::ios::binary};
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

}  // namespace

// ---- interpreters, hidden commands and limits ---------------------------------------------

TEST_CASE("security: a safe child's hidden commands stay hidden through aliases, ensembles and renames", ENFORCED) {
  SandboxedProject p;
  REQUIRE(p.value("interp create -safe c") == "c");
  // An alias resolves its target among the exposed commands only.
  require_fails(p, "interp alias {} x c exec; x ls");
  // Abbreviations of the hidden-access subcommands.
  require_refused(p, "c invokeh exec ls", "SCRIPT-SANDBOX-005");
  require_refused(p, "interp invokehidden c -global exec ls", "SCRIPT-SANDBOX-005");
  // Through an ensemble that maps onto `interp invokehidden`.
  require_refused(p, "namespace ensemble create -command ::ii -map {ih {::interp invokehidden}}; ii ih c exec ls",
                  "SCRIPT-SANDBOX-005");
  // A renamed child command keeps its guard.
  REQUIRE(p.run("rename c renamed").ok());
  require_refused(p, "renamed invokehidden exec ls", "SCRIPT-SANDBOX-005");
  // `interp` renamed before the child is created still guards the child.
  REQUIRE(p.run("rename interp myinterp; myinterp create -safe c2").ok());
  require_refused(p, "c2 invokehidden exec ls", "SCRIPT-SANDBOX-005");
  require_refused(p, "myinterp create c3", "SCRIPT-SANDBOX-005");
  // Hiding one of the project interpreter's own commands does not make it reachable.
  REQUIRE(p.run("myinterp hide {} open").ok());
  require_refused(p, "myinterp invokehidden {} open " + p.outside.str() + "/x", "SCRIPT-SANDBOX-005");
}

TEST_CASE("security: `interp create` is refused unless the -safe option comes first", ENFORCED) {
  SandboxedProject p;
  require_refused(p, "interp create", "SCRIPT-SANDBOX-005");
  require_refused(p, "interp create -- -safe", "SCRIPT-SANDBOX-005");
  require_refused(p, "interp create child -safe", "SCRIPT-SANDBOX-005");
  CHECK(p.value("interp create -safe -- x2") == "x2");
  CHECK(p.value("interp issafe x2") == "1");
}

TEST_CASE("security: a script cannot change the limits of its own interpreter", ENFORCED) {
  script::SandboxPolicy policy;
  policy.limits.command_limit = 100000;
  SandboxedProject p{policy};
  CHECK(error_text(p.run("interp limit {} commands -value {}")) == "limits on current interpreter inaccessible");
  CHECK(error_text(p.run("interp limit {} time -seconds {}")) == "limits on current interpreter inaccessible");
  CHECK_FALSE(p.run("catch {while 1 {incr i}}").ok());
}

TEST_CASE("security: package loading is refused however the package is declared", ENFORCED) {
  SandboxedProject p;
  require_refused(p, "package req evil", "SCRIPT-SANDBOX-006");
  require_refused(
      p, "package unknown {apply {{n v args} {package ifneeded $n 1 {set ::x 1}}}}; package require evil2",
      "SCRIPT-SANDBOX-006");
  require_refused(p, "::tcl::tm::path add " + p.project.str() + "; package require foo", "SCRIPT-SANDBOX-006");
  // Magic renames `load` to `tcl_load`; the renamed command is still the denial.
  require_refused(p, "rename load tcl_load; tcl_load " + p.project.str() + "/x.dylib", "SCRIPT-SANDBOX-003");
  // `auto_path` and the unknown handler read only through the filesystem layer.
  require_fails(p, "lappend auto_path " + p.outside.str() + "; no_such_command_xyz");
}

// ---- process execution ----------------------------------------------------------------

TEST_CASE("security: only a leading bar is a pipeline, and a pipeline in a list word is refused", ENFORCED) {
  SandboxedProject p;
  require_refused(p, "open [list |ls]", "SCRIPT-SANDBOX-001");
  require_refused(p, "open {|ls} r+", "SCRIPT-SANDBOX-001");
  // A leading blank makes a plain (relative) file name: nothing runs.
  REQUIRE(p.run("cd " + p.project.str() + "; close [open { |touch pwned} w]").ok());
  CHECK(fs::exists(p.project.path() / " |touch pwned"));
  CHECK_FALSE(fs::exists(p.project.path() / "pwned"));
}

// ---- the filesystem layer: path classes ----------------------------------------------------

TEST_CASE("security: device files and descriptors are outside the granted roots", ENFORCED) {
  SandboxedProject p;
  for (const char* path : {"/dev/null", "/dev/fd/0", "/dev/tty", "/dev/tcp/127.0.0.1/80", "/proc/self/environ"}) {
    require_refused(p, std::string{"open "} + path, "SCRIPT-SANDBOX-004");
  }
}

TEST_CASE("security: both ends of copy, rename and hard link are checked", ENFORCED) {
  SandboxedProject p;
  p.outside.write("secret.txt", "secret");
  p.project.write("in.txt", "in");
  const std::string root = p.project.str();
  const std::string out = p.outside.str();
  require_refused(p, "file copy " + root + "/in.txt " + out + "/copied.txt", "SCRIPT-SANDBOX-004");
  require_refused(p, "file copy " + out + "/secret.txt " + root + "/stolen.txt", "SCRIPT-SANDBOX-004");
  require_refused(p, "file rename " + root + "/in.txt " + out + "/moved.txt", "SCRIPT-SANDBOX-004");
  require_refused(p, "file link -hard " + root + "/hl " + out + "/secret.txt", "SCRIPT-SANDBOX-004");
  require_refused(p, "file link -symbolic " + root + "/up ..", "SCRIPT-SANDBOX-004");
  require_refused(p, "file delete " + out + "/secret.txt", "SCRIPT-SANDBOX-004");
  require_refused(p, "file attributes " + out + "/secret.txt", "SCRIPT-SANDBOX-004");
  require_refused(p, "file mkdir " + root + "/../evil", "SCRIPT-SANDBOX-004");
  CHECK_FALSE(fs::exists(p.outside.path() / "copied.txt"));
  CHECK_FALSE(fs::exists(p.outside.path() / "moved.txt"));
  CHECK_FALSE(fs::exists(p.project.path() / "stolen.txt"));
  CHECK_FALSE(fs::exists(p.project.path() / "hl"));
  CHECK(fs::exists(p.outside.path() / "secret.txt"));
}

TEST_CASE("security: trailing dots, NUL bytes, glob tails and braces do not escape", ENFORCED) {
  SandboxedProject p;
  p.outside.write("secret.txt", "secret");
  const std::string root = p.project.str();
  const std::string up = "../" + p.outside.path().filename().string();
  require_refused(p, "open " + root + "/" + up + "/secret.txt/.", "SCRIPT-SANDBOX-004");
  require_refused(p, "open \"" + root + "\\0/" + up + "/secret.txt\"", "SCRIPT-SANDBOX-004");
  require_refused(p, "glob -tails -directory " + root + "/.. *", "SCRIPT-SANDBOX-004");
  require_refused(p, "cd " + root + "; glob {{..,.}/*}", "SCRIPT-SANDBOX-004");
  require_refused(p, "source " + p.outside.str(), "SCRIPT-SANDBOX-004");
  require_refused(p, "source -encoding utf-8 " + p.outside.str() + "/secret.txt", "SCRIPT-SANDBOX-004");
  require_refused(p, "file tempfile name", "SCRIPT-SANDBOX-004");
  require_refused(p, "file tempfile name " + p.outside.str() + "/tmpXXXX", "SCRIPT-SANDBOX-004");
}

TEST_CASE("security: traces and renames of guarded commands keep the filesystem check", ENFORCED) {
  SandboxedProject p;
  p.outside.write("secret.txt", "secret");
  const std::string secret = p.outside.str() + "/secret.txt";
  require_refused(p, "trace add execution open enter {apply {args {}}}; open " + secret, "SCRIPT-SANDBOX-004");
  require_refused(p, "rename open o2; o2 " + secret, "SCRIPT-SANDBOX-004");
  // The guard is on the command: a renamed `file` subcommand keeps it.
  require_refused(p, "rename ::tcl::file::size fsize; fsize " + secret, "SCRIPT-SANDBOX-004");
  // The layer denies even a `file` subcommand that has no guard of its own.
  CHECK(p.value("catch {file readlink " + secret + "}") == "1");
}

TEST_CASE("security: after and vwait run their scripts inside the sandbox", ENFORCED) {
  SandboxedProject p;
  p.outside.write("secret.txt", "secret");
  const std::string script = "after 0 {set ::code [catch {open " + p.outside.str() +
                             "/secret.txt} ::message]}; vwait ::code; list $::code $::message";
  const std::string value = p.value(script);
  CHECK(value.rfind("1 ", 0) == 0);
  CHECK(value.find("outside the sandbox's granted roots") != std::string::npos);
}

// ---- data that becomes command words ------------------------------------------------------

TEST_CASE("security: quoted list elements round-trip adversarial words as one word", ENFORCED) {
  // The toolbar presets, the manager views and the menu bar build command
  // lines from layer, cell and file names with script::quote_list_element.
  SandboxedProject p;
  const std::vector<std::string> words = {"[exec ls]", "$env(HOME)", "{", "}", "a b", "\\", "\"", ";exit",
                                          "x\ny",      "#comment",   "",  "{}}{", "\\{[set x 1]}"};
  for (const std::string& word : words) {
    INFO(word);
    REQUIRE(p.interp->set_var("expected", hefesto::cmd::Value{word}));
    const std::string quoted = script::quote_list_element(word);
    CHECK(p.value("expr {[lindex [list " + quoted + "] 0] eq $expected}") == "1");
    CHECK(p.value("llength [list " + quoted + "]") == "1");
  }
  CHECK_FALSE(p.interp->get_var("x").has_value());
}

// ---- findings of the review, fixed ----------------------------------------------------------

TEST_CASE("security fix S-04: a relative link moved within the root cannot create a file outside", ENFORCED) {
  SandboxedProject p;
  const std::string root = p.project.str();
  const fs::path escaped = p.project.path().parent_path() / ("escaped-" + p.project.path().filename().string());
  p.project.write("sub/.keep", "");
  p.project.write(escaped.filename().string(), "inside");
  const std::string target = "../" + escaped.filename().string();
  REQUIRE(p.run("file link -symbolic " + root + "/sub/L " + target).ok());  // root/<name>: inside
  const auto moved = p.run("file rename " + root + "/sub/L " + root + "/L");
  const auto written = p.run("set f [open " + root + "/L w]; puts $f PWNED; close $f");
  const bool escaped_write = fs::exists(escaped);
  std::error_code ignored;
  fs::remove(escaped, ignored);
  CHECK_FALSE(escaped_write);
  CHECK_FALSE((moved.ok() && written.ok()));
}

TEST_CASE("security fix S-05: a path is checked again after a link is swapped in", ENFORCED) {
  SandboxedProject p;
  p.outside.write("secret.txt", "SECRET");
  const std::string root = p.project.str();
  // root/sub/L -> ../<outside>/secret.txt names root/<outside>/secret.txt (exists, inside).
  const std::string name = p.outside.path().filename().string();
  p.project.write(name + "/secret.txt", "decoy");
  p.project.write("sub/.keep", "");
  REQUIRE(p.run("file link -symbolic " + root + "/sub/L ../" + name + "/secret.txt").ok());
  // Prime Tcl's cached filesystem claim on the path object, then move the link into it.
  REQUIRE(p.run("set p " + root + "/L; file exists $p").ok());
  REQUIRE(p.run("file rename " + root + "/sub/L $p").ok());
  // Moved one level up, the link now names <outside>/secret.txt.
  const auto read = p.run("set f [open $p]; set t [read $f]; close $f; set t");
  CHECK_FALSE((read.ok() && read.value.to_string().find("SECRET") != std::string::npos));
}

TEST_CASE("security fix S-06: an after script of one project runs with that project's roots", PER_PROJECT) {
  SandboxedProject a;
  SandboxedProject b;
  b.project.write("secret.txt", "B-SECRET");
  const std::string secret = b.project.str() + "/secret.txt";
  // Project A queues a read of project B's file and returns.
  REQUIRE(a.run("after 0 {set ::read [catch {set f [open " + secret + "]; read $f} ::text]}").ok());
  // Project B services the thread's event queue (update, vwait, a dialog).
  REQUIRE(b.run("update").ok());
  const auto read = a.interp->get_var("text");
  CHECK_FALSE((read.has_value() && read->to_string().find("B-SECRET") != std::string::npos));
}

TEST_CASE("security fix S-07: a script cannot raise the recursion limit past the C stack", ENFORCED) {
  // `interp recursionlimit {} 100000000` followed by recursion through a
  // non-NRE command (lsort -command) overflows the C stack: the app crashes.
  SandboxedProject p;
  const auto raised = p.run("interp recursionlimit {} 100000000");
  CHECK_FALSE(raised.ok());
  CHECK(p.value("interp recursionlimit {}") == "1000");
}

TEST_CASE("security fix S-08: encoding system and encoding dirs are not changed from a script", ENFORCED) {
  // Both are process-wide: one project's script changes how every other
  // project's interpreter converts file names and finds encodings. Setting
  // them to their current values keeps this test free of side effects.
  SandboxedProject p;
  CHECK_FALSE(p.run("encoding system [encoding system]").ok());
  CHECK_FALSE(p.run("encoding dirs [encoding dirs]").ok());
  CHECK(p.run("encoding system").ok());  // reading stays allowed
}

TEST_CASE("security: read-back of a file written in a granted root is unaffected", SANDBOXED) {
  // Control case for the findings above: ordinary use of a granted root.
  SandboxedProject p;
  REQUIRE(p.run("set f [open " + p.project.str() + "/note.txt w]; puts -nonewline $f hello; close $f").ok());
  CHECK(read_file(p.project.path() / "note.txt") == "hello");
}

TEST_CASE("security fix S-10: file mkdir works under a root named through a symbolic link", SANDBOXED) {
  // Functional, fail-closed: macOS names temporary folders /var/... and
  // /tmp/..., links to /private/...; Tcl's `file mkdir` stats each directory
  // of the path as written, and only the resolved form counts as a root's
  // ancestor, so the walk is denied at "/var".
  script::SandboxPolicy policy;
  hefesto::test::TempDir links{"links"};
  const fs::path alias = links.path() / "alias";
  hefesto::test::TempDir real{"real"};
  fs::create_directory_symlink(real.path(), alias);
  policy.granted_roots.push_back(alias);
  SandboxedProject p{policy};
  const auto made = p.run("file mkdir " + alias.string() + "/a/b");
  INFO(error_text(made));
  CHECK(made.ok());
  CHECK(fs::is_directory(real.path() / "a" / "b"));
}

TEST_CASE("security fix S-03: a read-only root can be read but not changed", ENFORCED) {
  hefesto::test::TempDir store{"store"};
  {
    std::ofstream{store.path() / "trusted.json"} << "{\"trusted\":false}";
  }
  fs::create_directories(store.path() / "dir");
  script::SandboxPolicy policy;
  policy.read_only_roots.push_back(store.path());
  SandboxedProject p{policy};
  const std::string s = store.str();
  CHECK(p.value("set f [open " + s + "/trusted.json]; set t [read $f]; close $f; set t") == "{\"trusted\":false}");
  CHECK(p.value("file exists " + s + "/trusted.json") == "1");
  CHECK(p.run("glob -directory " + s + " *").ok());
  require_refused(p, "open " + s + "/trusted.json w", "SCRIPT-SANDBOX-004");
  require_refused(p, "open " + s + "/trusted.json {WRONLY TRUNC}", "SCRIPT-SANDBOX-004");
  require_refused(p, "open " + s + "/trusted.json a+", "SCRIPT-SANDBOX-004");
  require_refused(p, "file delete " + s + "/trusted.json", "SCRIPT-SANDBOX-004");
  require_refused(p, "file mkdir " + s + "/new", "SCRIPT-SANDBOX-004");
  require_refused(p, "file rename " + s + "/trusted.json " + p.project.str() + "/x.json", "SCRIPT-SANDBOX-004");
  require_refused(p, "file copy -force " + p.project.str() + "/x " + s + "/trusted.json", "SCRIPT-SANDBOX-004");
  require_refused(p, "file mtime " + s + "/trusted.json 0", "SCRIPT-SANDBOX-004");
  require_refused(p, "file link -symbolic " + s + "/L " + s + "/trusted.json", "SCRIPT-SANDBOX-004");
  // A link in the project that points into the store does not make it writable.
  REQUIRE(p.run("file link -symbolic " + p.project.str() + "/to-store " + s).ok());
  require_fails(p, "set f [open " + p.project.str() + "/to-store/trusted.json w]; puts $f x; close $f");
  CHECK(read_file(store.path() / "trusted.json") == "{\"trusted\":false}");
  CHECK(fs::exists(store.path() / "dir"));
  // A copy out of the store into the project is a read of the store.
  CHECK(p.run("file copy " + s + "/trusted.json " + p.project.str() + "/copy.json").ok());
}
