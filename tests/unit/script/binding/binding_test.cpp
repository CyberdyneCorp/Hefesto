// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The command binding (tasks.md 5.1; capability scripting-api "Command
// registration in the magic namespace", "Global import of magic commands",
// "Name collisions with Tcl built-ins", "Unknown-command fallback for
// renamed built-ins", "Command status and error signalling",
// "Window-directed commands", "Post-command bookkeeping", "Scripting host
// choice is abstracted behind a command API").

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>

#include "binding_fixture.hpp"
#include "hefesto/script/binding/window_commands.hpp"
#include "hefesto/script/events.hpp"

namespace script = hefesto::script;
namespace cmd = hefesto::cmd;
using script::test::BindingFixture;

TEST_CASE("Same command vector, same result regardless of host",
          "[cap:scripting-api][req:Scripting host choice is abstracted behind a command API]") {
  BindingFixture f;
  CHECK(f.value("units internal") == "");
  const cmd::CommandResult direct = f.dispatcher->execute(cmd::Invocation{"box", {"values"}, cmd::Origin::script, std::nullopt});
  REQUIRE(direct.ok());
  CHECK(direct.value.to_string() == "0 0 10 20");
  CHECK(f.value("box values") == "0 0 10 20");
}

TEST_CASE("Wizard command prefix", "[cap:scripting-api][req:Command registration in the magic namespace]") {
  BindingFixture f;
  CHECK(f.value("magic::*flags deb") == "0");
  CHECK(f.value("magic::*flags debug on; magic::*flags debug") == "1");
  CHECK(f.value("magic::*flags d") == "1");
  CHECK(f.error("magic::*flags x") == "bad flag \"x\": must be debug, recover, silent, window, console, or printf");
  CHECK(f.value("magic::display") == "NULL");
  CHECK(f.value("magic::startup") == "");
}

TEST_CASE("Abbreviation is not a script command", "[cap:scripting-api][req:Command registration in the magic namespace]") {
  BindingFixture f;
  CHECK(f.error("magic::bo values") == "invalid command name \"magic::bo\"");
  CHECK(f.value("info commands ::magic::box") == "::magic::box");
  CHECK(f.value("info commands {::magic::*bypass}") == "::magic::*bypass");
  // Every registered full name is a command in magic:: (window, layout and
  // the Tcl-side procedures alike).
  for (const std::string& name : f.registry.names()) {
    if (name.find("::") == std::string::npos) {
      INFO(name);
      CHECK(f.interp->has_command("::magic::" + name));
    }
  }
  CHECK(f.interp->has_command("::hefesto::strict"));
}

TEST_CASE("Unqualified call", "[cap:scripting-api][req:Global import of magic commands]") {
  BindingFixture f;
  CHECK(f.value("namespace origin box") == "::magic::box");
  CHECK(f.value("box values") == f.value("magic::box values"));
  CHECK(f.value("namespace origin {*bypass}") == "::magic::*bypass");
  CHECK(f.value("namespace origin tag") == "::magic::tag");
  // Tcl procedures of Magic's library are defined after the import: they
  // are reachable only as magic::<name>, as in Magic.
  CHECK(f.value("info commands gencell") == "");
  CHECK(f.value("info commands ::magic::gencell") == "::magic::gencell");
  // Regression (golden SCRIPT-RSP-01..06, SCRIPT-AUX-02): readspice.tcl and mazeroute.tcl
  // define global procedures, so scripts call them unqualified.
  CHECK(f.value("info commands ::readspice") == "::readspice");
  CHECK(f.value("info commands ::mazeroute") == "::mazeroute");
}

TEST_CASE("Pre-existing global command wins", "[cap:scripting-api][req:Global import of magic commands]") {
  BindingFixture f;
  // magic.tcl.in defines `echo` before the import: Hefesto's `echo` command
  // stays reachable as magic::echo only.
  CHECK(f.value("namespace origin echo") == "::echo");
  CHECK(f.value("info body echo").find("puts stdout $args") != std::string::npos);
  CHECK(f.value("info commands ::magic::echo") == "::magic::echo");
  // Tcl's own `source` beats the engine's command of the same name.
  CHECK(f.value("namespace origin source") == "::source");
}

TEST_CASE("Tcl array semantics preserved", "[cap:scripting-api][req:Name collisions with Tcl built-ins]") {
  BindingFixture f;
  CHECK(f.value("array set a {x 1}; set a(x)") == "1");
  CHECK(f.value("info commands tcl_array") == "tcl_array");
  // In the caller's frame: a procedure's local array.
  CHECK(f.value("proc p {} {array set loc {y 2}; return $loc(y)}; p") == "2");
  CHECK(f.value("info exists loc") == "0");
  CHECK(f.value("info commands orig_clock") == "orig_clock");
}

TEST_CASE("Magic flush fallback", "[cap:scripting-api][req:Name collisions with Tcl built-ins]") {
  BindingFixture f;
  CHECK(f.value("flush stdout") == "");  // tcl_flush handled it
  CHECK(f.value("info commands tcl_flush") == "tcl_flush");
  // `flush` without a channel fails in Tcl: the Magic command runs.
  CHECK(f.eval("flush -noprompt").ok());
}

TEST_CASE("Shared library load", "[cap:scripting-api][req:Name collisions with Tcl built-ins]") {
  BindingFixture f;
  f.take_printed();
  const std::string message = f.error("load /opt/x/libfoo.so");
  CHECK_FALSE(message.empty());
  CHECK(f.editor.cells.find("libfoo") == nullptr);
  CHECK(f.editor.cells.find("libfoo.so") == nullptr);
}

TEST_CASE("a refused shared library load answers with Tcl's text under magic-compat",
          "[cap:scripting-api][req:Name collisions with Tcl built-ins]") {
  // Regression (golden SCRIPT-API-04): Magic's `load /nonexistent/libfoo.so`
  // returns Tcl's dlopen() error; the sandbox's own text only with
  // magic-compat OFF. The denial is recorded either way.
  struct Denials final : script::ScriptObserver {
    int count = 0;
    void on_denied(const script::DeniedOperation&) override { ++count; }
  };
  SECTION("magic-compat ON") {
    Denials denials;
    BindingFixture f{true};
    f.editor.script.events.add(denials);
    CHECK(f.error("load /nonexistent/libfoo.so") ==
          "couldn't load file \"/nonexistent/libfoo.so\": /nonexistent/libfoo.so: cannot open shared object file: "
          "No such file or directory");
    CHECK(f.error("load lib/x.so Pkg") ==
          "couldn't load file \"lib/x.so\": lib/x.so: cannot open shared object file: No such file or directory");
    CHECK(denials.count == 2);
  }
  SECTION("magic-compat OFF") {
    Denials denials;
    BindingFixture f{false};
    f.editor.script.events.add(denials);
    CHECK(f.error("load /nonexistent/libfoo.so") == "load: loading native extensions is not permitted in the script sandbox");
    CHECK(denials.count == 1);
  }
}

TEST_CASE("Cell load with dotted directory", "[cap:scripting-api][req:Name collisions with Tcl built-ins]") {
  BindingFixture f;
  // The built-in fails, the last component has no dot: Magic's load runs
  // (and creates the missing cell, as `load` does).
  CHECK(f.eval("load ../lib.v2/inv").ok());
  CHECK(f.editor.cells.find("inv") != nullptr);
}

TEST_CASE("a cell load records no sandbox denial", "[cap:scripting-api][req:Name collisions with Tcl built-ins]") {
  // Regression: `load inv` tried the sandbox's `tcl_load` first, so a
  // typed `load <cell>` showed "Sandbox blocked load" in the status bar.
  struct Denials final : script::ScriptObserver {
    int count = 0;
    void on_denied(const script::DeniedOperation&) override { ++count; }
  } denials;
  BindingFixture f;
  f.editor.script.events.add(denials);
  CHECK(f.eval("load inv").ok());
  CHECK(f.eval("load cells/inv2.mag").ok());
  CHECK(denials.count == 0);
  CHECK(f.editor.cells.find("inv") != nullptr);
  CHECK_FALSE(f.error("load /opt/x/libfoo.so").empty());
  CHECK(denials.count == 1);
  f.editor.script.events.remove(denials);
}

TEST_CASE("Compressed cell", "[cap:scripting-api][req:Name collisions with Tcl built-ins]") {
  BindingFixture f;
  CHECK(f.eval("load cells/inv.mag.gz").ok());
  CHECK(f.value("magic::load cells/nand.mag") == "");
}

TEST_CASE("Renamed cd is still reachable", "[cap:scripting-api][req:Unknown-command fallback for renamed built-ins]") {
  BindingFixture f;
  CHECK(f.value("proc tcl_foo {a b} {return $a$b}; foo 1 2") == "12");
  CHECK(f.error("proc tcl_bar {} {error boom}; bar") == "boom");
  CHECK(f.value("info commands tcl_unknown") == "tcl_unknown");
  CHECK(f.value("info commands tcl_cd") == "tcl_cd");
}

TEST_CASE("Bad usage is not a Tcl error", "[cap:scripting-api][req:Command status and error signalling]") {
  BindingFixture f;
  f.take_printed();
  CHECK(f.value("catch {box frobnicate} err") == "0");
  CHECK_FALSE(f.take_printed().empty());  // the usage went to the error stream
  // A command that fails internally still returns ok; strict mode turns the
  // failure into a Tcl error carrying the diagnostic.
  CHECK(f.value("catch {paint nosuchlayer}") == "0");
  CHECK(f.value("hefesto::strict on; catch {paint nosuchlayer} err") == "1");
  CHECK_FALSE(f.value("set err").empty());
  CHECK(f.value("hefesto::strict off; catch {paint nosuchlayer}") == "0");
}

TEST_CASE("Missing cell on load -fail", "[cap:scripting-api][req:Command status and error signalling]") {
  BindingFixture f;
  CHECK(f.value("catch {load nosuchcell -fail -silent}") == "0");
}

TEST_CASE("Command for another window type", "[cap:scripting-api][req:Command status and error signalling]") {
  BindingFixture f;
  // A command of the color client sent to the layout window: dispatch
  // fails quietly (error status, empty message, nothing printed).
  std::string color_only;
  for (const auto* spec : f.registry.table(cmd::kColorClient)) {
    if (f.registry.find(spec->name, cmd::kLayoutClient) == nullptr &&
        f.registry.find(spec->name, cmd::kWindowClient) == nullptr) {
      color_only = spec->name;
      break;
    }
  }
  REQUIRE_FALSE(color_only.empty());
  f.take_printed();
  CHECK(f.error("magic::" + color_only) == "");
  CHECK(f.take_printed().empty());
}

TEST_CASE("Directed box query", "[cap:scripting-api][req:Window-directed commands]") {
  BindingFixture f;
  const std::array<std::string, 1> paths{".layout1.pane.top.magic"};
  REQUIRE(script::register_window_commands(*f.interp, *f.dispatcher, paths).ok());
  CHECK(f.value(".layout1.pane.top.magic box values") == "0 0 10 20");
  // Abbreviations are accepted through a window path.
  CHECK(f.value(".layout1.pane.top.magic bo values") == "0 0 10 20");
  // A dispatch failure prints its message and is not an error.
  f.take_printed();
  CHECK(f.value(".layout1.pane.top.magic nosuchcommand") == "");
  CHECK(f.take_printed().find("nosuchcommand") != std::string::npos);
  // The window's command point is set for the command only.
  CHECK_FALSE(f.editor.command_point.has_value());
}

TEST_CASE("Bypass keeps the command count", "[cap:scripting-api][req:Post-command bookkeeping]") {
  BindingFixture f;
  const std::uint64_t before = f.editor.command_count;
  CHECK(f.value("box values") == "0 0 10 20");
  CHECK(f.editor.command_count == before + 1);
  const std::uint64_t n = f.editor.command_count;
  CHECK(f.value("*bypass box values") == "0 0 10 20");
  CHECK(f.editor.command_count == n);
}
