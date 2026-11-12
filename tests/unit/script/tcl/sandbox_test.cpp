// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The script sandbox (openspec/changes/m4-scripting tasks.md 4.2, 4.3;
// design D5; capabilities hefesto-scripting "Embedded sandboxed Tcl on all
// platforms" and hefesto-security "Script sandbox enforcement"): one test per
// removed or restricted command, one per path class of the filesystem layer,
// `cd` within the granted roots (D-33) and the synthesized `env` (D-31).

#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "hefesto/script/tcl/sandbox.hpp"
#include "tcl_fixture.hpp"

namespace script = hefesto::script;
namespace cmd = hefesto::cmd;
namespace fs = std::filesystem;
using hefesto::test::error_text;
using hefesto::test::has_code;
using hefesto::test::SandboxedProject;

#define SANDBOXED "[cap:hefesto-scripting][req:Embedded sandboxed Tcl on all platforms]"
#define ENFORCED "[cap:hefesto-security][req:Script sandbox enforcement]"

namespace {

// Runs `script`, which must fail with a sandbox error naming `operation`
// (the Tcl message starts with it) and carry `code` with a location.
void require_denied(SandboxedProject& p, const std::string& script, const std::string& operation,
                    const std::string& code) {
  INFO(script);
  const auto result = p.run(script);
  REQUIRE_FALSE(result.ok());
  CHECK(error_text(result).rfind(operation + ":", 0) == 0);
  REQUIRE(has_code(result, code));
  REQUIRE_FALSE(p.recorder.denials.empty());
  CHECK(p.recorder.denials.back().operation.rfind(operation, 0) == 0);
}

// `script` raises a path denial naming `operation` (SCRIPT-SANDBOX-004).
void require_path_denied(SandboxedProject& p, const std::string& script, const std::string& operation) {
  require_denied(p, script, operation, "SCRIPT-SANDBOX-004");
  CHECK(error_text(p.run(script)).find("the sandbox's granted roots") != std::string::npos);
}

}  // namespace

// ---- removed and restricted commands (tasks.md 4.2) ---------------------------------------

TEST_CASE("Denied exec", SANDBOXED) {
  SandboxedProject p;
  const auto result = p.run("exec ls");
  REQUIRE_FALSE(result.ok());
  CHECK(error_text(result) == "exec: process execution is not permitted in the script sandbox");
  CHECK(has_code(result, "SCRIPT-SANDBOX-001"));
  REQUIRE(p.recorder.denials.size() == 1);
  CHECK(p.recorder.denials[0].operation == "exec");
  CHECK(p.recorder.denials[0].target == "ls");
  // Hefesto continues running, and so does the interpreter.
  CHECK(p.value("expr {1+1}") == "2");
}

TEST_CASE("open of a command pipeline is process execution", SANDBOXED) {
  SandboxedProject p;
  require_denied(p, "open {|ls -l}", "open", "SCRIPT-SANDBOX-001");
  require_denied(p, "open \"|cat\" r+", "open", "SCRIPT-SANDBOX-001");
  CHECK(error_text(p.run("open |ls")) == "open: process execution is not permitted in the script sandbox");
}

TEST_CASE("Script tries a socket", ENFORCED) {
  SandboxedProject p;
  const auto script = p.project.write("pdk/menus.tcl", "set host example.com\n# connect\nsocket $host 80\n");
  const auto result = p.interp->source(script);
  REQUIRE_FALSE(result.ok());
  CHECK(error_text(result) == "socket: network sockets are not permitted in the script sandbox");
  REQUIRE(has_code(result, "SCRIPT-SANDBOX-002"));
  for (const auto& diagnostic : result.diagnostics) {
    if (diagnostic.code == "SCRIPT-SANDBOX-002") {
      const auto* where = std::get_if<hefesto::diag::SourceLocation>(&diagnostic.location);
      REQUIRE(where != nullptr);
      CHECK(where->file == script.string());
      CHECK(where->line == 3);
    }
  }
  REQUIRE(p.recorder.denials.size() == 1);
  CHECK(p.recorder.denials[0].operation == "socket");
  CHECK(p.recorder.denials[0].target == "example.com 80");
  CHECK(p.recorder.denials[0].location == hefesto::diag::SourceLocation{script.string(), 3});
  // A server socket is refused the same way.
  require_denied(p, "socket -server accept 0", "socket", "SCRIPT-SANDBOX-002");
}

TEST_CASE("load and unload of native extensions are refused", SANDBOXED) {
  SandboxedProject p;
  const auto library = p.project.write("libext.so", "not a library");
  require_denied(p, "load " + library.string(), "load", "SCRIPT-SANDBOX-003");
  require_denied(p, "load {} Tk", "load", "SCRIPT-SANDBOX-003");
  require_denied(p, "unload " + library.string(), "unload", "SCRIPT-SANDBOX-003");
}

TEST_CASE("exit is not available (quit is the engine's exit path)", SANDBOXED) {
  SandboxedProject p;
  require_denied(p, "exit 1", "exit", "SCRIPT-SANDBOX-007");
  CHECK(error_text(p.run("exit")) == "exit: not available in the script sandbox (use quit)");
}

TEST_CASE("only safe interpreters can be created, and their hidden commands stay hidden", SANDBOXED) {
  SandboxedProject p;
  require_denied(p, "interp create child", "interp create", "SCRIPT-SANDBOX-005");
  require_denied(p, "interp cr -- child", "interp create", "SCRIPT-SANDBOX-005");
  CHECK(p.value("interp create -safe child") == "child");
  CHECK(p.value("child eval {expr {6*7}}") == "42");
  CHECK(p.value("interp issafe child") == "1");
  // A safe interpreter has no exec; its hidden one is unreachable.
  CHECK_FALSE(p.run("child eval {exec ls}").ok());
  require_denied(p, "child invokehidden exec ls", "child invokehidden", "SCRIPT-SANDBOX-005");
  require_denied(p, "interp invokehidden child exec ls", "interp invokehidden", "SCRIPT-SANDBOX-005");
  require_denied(p, "interp expose child exec", "interp expose", "SCRIPT-SANDBOX-005");
  require_denied(p, "child expose open", "child expose", "SCRIPT-SANDBOX-005");
  require_denied(p, "interp marktrusted child", "interp marktrusted", "SCRIPT-SANDBOX-005");
  require_denied(p, "child marktrusted", "child marktrusted", "SCRIPT-SANDBOX-005");
  // A child of the safe child is safe too, and reached only through it.
  CHECK(p.value("child eval {interp create grandchild}") == "grandchild");
  CHECK_FALSE(p.run("child eval {grandchild invokehidden exec ls}").ok());
  require_denied(p, "interp invokehidden {child grandchild} exec ls", "interp invokehidden", "SCRIPT-SANDBOX-005");
  CHECK(p.value("interp delete child; interp exists child") == "0");
}

TEST_CASE("package require loads only Tcl's own script packages", SANDBOXED) {
  SandboxedProject p;
  CHECK(p.value("package require Tcl") == "8.6.16");
  CHECK(p.value("package require http") == "2.9.8");
  require_denied(p, "package require Tk", "package require", "SCRIPT-SANDBOX-006");
  // A binary package declared by a script: refused before its load runs.
  const auto library = p.project.write("libfoo.so", "");
  REQUIRE(p.run("package ifneeded foo 1.0 [list load " + library.string() + " Foo]").ok());
  require_denied(p, "package require foo", "package require", "SCRIPT-SANDBOX-006");
  require_denied(p, "package require -exact foo 1.0", "package require", "SCRIPT-SANDBOX-006");
  // A package the script itself provides is present, not loaded.
  CHECK(p.value("package provide mine 2.0; package require mine") == "2.0");
}

TEST_CASE("catch absorbs a denial, which is still reported", ENFORCED) {
  SandboxedProject p;
  const auto result = p.run("set denied [catch {exec ls} message]; list $denied $message");
  REQUIRE(result.ok());
  CHECK(result.value.to_string() == "1 {exec: process execution is not permitted in the script sandbox}");
  CHECK(has_code(result, "SCRIPT-SANDBOX-001"));
  CHECK(p.recorder.denials.size() == 1);
  CHECK(p.value("catch {open " + p.outside.str() + "/x} message; set message").find("outside") != std::string::npos);
  CHECK(p.value("set ::errorCode") == "HEFESTO SANDBOX SCRIPT-SANDBOX-004");
}

// ---- the filesystem layer (tasks.md 4.3) ------------------------------------------------------

TEST_CASE("paths under a granted root are read and written as usual", SANDBOXED) {
  SandboxedProject p;
  const std::string root = p.project.str();
  CHECK(p.value("set f [open " + root + "/a.txt w]; puts $f hello; close $f; "
                "set f [open " + root + "/a.txt]; set t [read -nonewline $f]; close $f; set t") == "hello");
  CHECK(p.value("file mkdir " + root + "/lib/sub; file isdirectory " + root + "/lib/sub") == "1");
  CHECK(p.value("file copy " + root + "/a.txt " + root + "/lib/b.txt; file exists " + root + "/lib/b.txt") == "1");
  CHECK(p.value("lsort [glob -tails -directory " + root + " *]") == "a.txt lib");
  CHECK(p.value("file delete -force " + root + "/lib; file exists " + root + "/lib") == "0");
  CHECK(p.value("file tempfile name " + root + "/tmpXXXX; file dirname $name") == root);
  CHECK(p.recorder.denials.empty());
}

TEST_CASE("open, source and file subcommands outside the granted roots are denied", SANDBOXED) {
  SandboxedProject p;
  const auto secret = p.outside.write("secret.txt", "secret");
  const auto script = p.outside.write("evil.tcl", "set pwned 1");
  const std::string out = p.outside.str();
  const std::string root = p.project.str();
  require_path_denied(p, "open " + secret.string(), "open");
  require_path_denied(p, "open " + out + "/new.txt w", "open");
  require_path_denied(p, "source " + script.string(), "source");
  CHECK_FALSE(p.interp->get_var("pwned").has_value());
  require_path_denied(p, "file exists " + secret.string(), "file exists");
  require_path_denied(p, "file isdirectory " + out, "file isdirectory");
  require_path_denied(p, "file isfile " + secret.string(), "file isfile");
  require_path_denied(p, "file readable " + secret.string(), "file readable");
  require_path_denied(p, "file size " + secret.string(), "file size");
  require_path_denied(p, "file mtime " + secret.string(), "file mtime");
  require_path_denied(p, "file stat " + secret.string() + " st", "file stat");
  require_path_denied(p, "file type " + secret.string(), "file type");
  require_path_denied(p, "file attributes " + secret.string(), "file attributes");
  require_path_denied(p, "file delete " + secret.string(), "file delete");
  require_path_denied(p, "file mkdir " + out + "/made", "file mkdir");
  require_path_denied(p, "file copy " + secret.string() + " " + root + "/copy.txt", "file copy");
  REQUIRE(p.run("set f [open " + root + "/mine.txt w]; close $f").ok());
  require_path_denied(p, "file copy " + root + "/mine.txt " + out + "/copy.txt", "file copy");
  require_path_denied(p, "file rename " + root + "/mine.txt " + out + "/moved.txt", "file rename");
  require_path_denied(p, "file tempfile name " + out + "/tmpXXXX", "file tempfile");
  require_path_denied(p, "file tempfile name", "file tempfile");
  // Nothing outside changed.
  CHECK(fs::exists(secret));
  CHECK_FALSE(fs::exists(p.outside.path() / "new.txt"));
  CHECK_FALSE(fs::exists(p.outside.path() / "made"));
  CHECK_FALSE(fs::exists(p.outside.path() / "copy.txt"));
  CHECK_FALSE(fs::exists(p.outside.path() / "moved.txt"));
  CHECK(fs::exists(p.project.path() / "mine.txt"));
}

TEST_CASE("glob patterns cannot list outside the granted roots", SANDBOXED) {
  SandboxedProject p;
  p.outside.write("a.tcl", "");
  const std::string out = p.outside.str();
  require_path_denied(p, "glob -directory " + out + " *", "glob");
  require_path_denied(p, "glob " + out + "/*", "glob");
  require_path_denied(p, "glob -nocomplain " + out + "/*.tcl", "glob");
  require_path_denied(p, "glob -nocomplain -path " + out + "/ *", "glob");
  require_path_denied(p, "glob -nocomplain -directory " + p.project.str() + " ../*", "glob");
}

TEST_CASE("a relative path cannot escape the granted root", SANDBOXED) {
  SandboxedProject p;
  const auto secret = p.outside.write("secret.txt", "secret");
  const fs::path up = fs::relative(secret, p.project.path());  // ../hefesto-tcl-outside-…/secret.txt
  REQUIRE(p.run("cd " + p.project.str()).ok());
  require_path_denied(p, "open " + up.string(), "open");
  require_path_denied(p, "file exists " + up.string(), "file exists");
  require_path_denied(p, "open " + p.project.str() + "/../" + up.filename().string(), "open");
  require_path_denied(p, "source " + up.string(), "source");
}

TEST_CASE("a symbolic link cannot lead out of the granted root", SANDBOXED) {
  SandboxedProject p;
  p.outside.write("secret.txt", "secret");
  fs::create_directory_symlink(p.outside.path(), p.project.path() / "escape");
  fs::create_symlink(p.outside.path() / "secret.txt", p.project.path() / "secret-link.txt");
  const std::string root = p.project.str();
  require_path_denied(p, "open " + root + "/escape/secret.txt", "open");
  require_path_denied(p, "open " + root + "/secret-link.txt", "open");
  require_path_denied(p, "glob -directory " + root + "/escape *", "glob");
  require_path_denied(p, "cd " + root + "/escape", "cd");
  // Nor can the script make such a link (Tcl checks the link target).
  require_path_denied(p, "file link -symbolic " + root + "/made " + p.outside.str(), "file link");
  CHECK_FALSE(fs::exists(p.project.path() / "made"));
}

TEST_CASE("reading a link whose name holds a NUL character fails as a missing file", SANDBOXED) {
  // fuzz_tcl_sandbox (CI run 43167835900): Tcl's TclpObjLink called readlink()
  // with a NULL native path for such a name and crashed.
  SandboxedProject p;
  const std::string root = p.project.str();
  for (const std::string command : {"file link", "file readlink"}) {
    const auto result = p.run(command + " \"" + root + "/a\\0b\"");
    REQUIRE_FALSE(result.ok());
    CHECK(error_text(result).find("could not read link") != std::string::npos);
    CHECK(error_text(result).find("no such file or directory") != std::string::npos);
  }
}

TEST_CASE("home-directory and double-slash forms are resolved before the check", SANDBOXED) {
  SandboxedProject p;
  p.outside.write("secret.txt", "secret");
  require_path_denied(p, "file exists ~/.magicrc", "file exists");
  require_path_denied(p, "glob -nocomplain ~/*", "glob");
  require_path_denied(p, "open ~root/x", "open");
  require_path_denied(p, "open /" + p.outside.str() + "/secret.txt", "open");
  require_path_denied(p, "open " + p.outside.str() + "//secret.txt", "open");
  require_path_denied(p, "open //etc/passwd", "open");
  // A doubled slash inside a granted root is the same granted path.
  p.project.write("ok.txt", "ok");
  CHECK(p.value("file exists " + p.project.str() + "//ok.txt") == "1");
}

TEST_CASE("cd outside the granted roots", SANDBOXED) {
  SandboxedProject p;
  fs::create_directories(p.project.path() / "lib");
  REQUIRE(p.run("cd " + p.project.str() + "/lib").ok());
  CHECK(p.value("pwd") == (p.project.path() / "lib").string());
  const auto denied = p.run("cd /etc");
  REQUIRE_FALSE(denied.ok());
  CHECK(error_text(denied).rfind("cd: ", 0) == 0);
  CHECK(error_text(denied).find("outside the sandbox's granted roots") != std::string::npos);
  CHECK(has_code(denied, "SCRIPT-SANDBOX-004"));
  CHECK(p.recorder.denials.back().operation == "cd");
  require_path_denied(p, "cd " + p.outside.str(), "cd");
  require_path_denied(p, "cd", "cd");  // the home directory
  // A relative cd within the root works and the working directory is unchanged by the refusals.
  CHECK(p.value("cd ..; pwd") == p.project.str());
}

TEST_CASE("Synthesized environment", SANDBOXED) {
  ::setenv("PDK_ROOT", "/host/pdk-root", 1);
  ::setenv("HEFESTO_SANDBOX_PROBE", "host", 1);
  script::SandboxPolicy policy;
  policy.environment = {{"PDK_ROOT", "/store/ciel/sky130/versions/5740c4a81c42f9fe4af5ef2ee258a99151053d28"},
                        {"PDK", "sky130A"},
                        {"CAD_ROOT", "/store/cad"}};
  SandboxedProject p{policy};
  CHECK(p.value("set env(PDK_ROOT)") == "/store/ciel/sky130/versions/5740c4a81c42f9fe4af5ef2ee258a99151053d28");
  CHECK(p.value("set env(PDK)") == "sky130A");
  CHECK(p.value("lsort [array names env]") == "CAD_ROOT PDK PDK_ROOT");
  CHECK(p.value("info exists env(HOME)") == "0");
  CHECK(p.value("info exists env(HEFESTO_SANDBOX_PROBE)") == "0");
  REQUIRE(p.run("set env(PDK_ROOT) /x; set env(HEFESTO_SANDBOX_PROBE) script; unset env(PDK)").ok());
  CHECK(p.value("set env(PDK_ROOT)") == "/x");
  CHECK(std::string{std::getenv("PDK_ROOT")} == "/host/pdk-root");
  CHECK(std::string{std::getenv("HEFESTO_SANDBOX_PROBE")} == "host");
  // Another project's interpreter has its own env.
  SandboxedProject other;
  CHECK(other.value("array names env") == "");
  ::unsetenv("PDK_ROOT");
  ::unsetenv("HEFESTO_SANDBOX_PROBE");
}

TEST_CASE("no granted root: only the embedded library is readable", SANDBOXED) {
  hefesto::editor::EditorState state;
  hefesto::test::TempDir dir{"plain"};
  auto& interp = script::open_project_interpreter(state, script::SandboxPolicy{});
  const auto result = interp.evaluate("file exists " + dir.str());
  REQUIRE_FALSE(result.ok());
  CHECK(has_code(result, "SCRIPT-SANDBOX-004"));
  CHECK(interp.evaluate("file exists [info library]").value.to_string() == "1");
}

// ---- the path decision ---------------------------------------------------------------

TEST_CASE("SandboxFilesystem allows exactly the paths under its roots", SANDBOXED) {
  hefesto::test::TempDir root{"root"};
  hefesto::test::TempDir other{"other"};
  fs::create_directories(root.path() / "lib");
  fs::create_directory_symlink(other.path(), root.path() / "link");
  script::SandboxPolicy policy;
  policy.granted_roots = {root.path() / "lib" / "..", fs::path{"relative/ignored"}};
  const script::SandboxFilesystem filesystem{policy};
  REQUIRE(filesystem.roots() == std::vector<fs::path>{root.path()});
  const fs::path cwd = root.path() / "lib";
  CHECK(filesystem.check("open", root.str() + "/a.mag", "/").allowed);
  CHECK(filesystem.check("open", root.str(), "/").allowed);
  CHECK(filesystem.check("open", "x.mag", cwd).allowed);
  CHECK(filesystem.check("open", "../x.mag", cwd).allowed);
  CHECK_FALSE(filesystem.check("open", "../../x", cwd).allowed);
  CHECK(filesystem.check("open", "../../x", cwd).normalized == root.path().parent_path() / "x");
  CHECK_FALSE(filesystem.check("open", root.str() + "/link/x", "/").allowed);
  CHECK(filesystem.check("open", root.str() + "/link/x", "/").normalized == other.path() / "x");
  CHECK_FALSE(filesystem.check("open", root.str() + "-sibling/x", "/").allowed);
  CHECK_FALSE(filesystem.check("open", "~/x", cwd).allowed);
  CHECK_FALSE(filesystem.check("open", "", cwd).allowed);
  CHECK_FALSE(filesystem.check("open", other.str(), "/").allowed);
  CHECK(filesystem.check("open", "/" + root.str() + "/a", "/").allowed);
  CHECK(script::is_embedded_path("/__hefesto_embedded__/tcl8.6/init.tcl"));
  CHECK(script::is_embedded_path("/__hefesto_embedded__"));
  CHECK_FALSE(script::is_embedded_path("/__hefesto_embedded__x/y"));
  CHECK(script::default_removed_commands() == std::vector<std::string>{"exec", "socket", "load", "unload", "exit"});
}
