// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Output, prompts, the terminal input queue, `cd` and the generic helpers
// (tasks.md 5.3; capability scripting-api "Output, flushing and escaping"
// (Q-SCRIPT-4), "Prompts and dialogs through the script layer", "Terminal
// input queue without console", "Directory change rebases cell paths"
// (D-33), "Generic Tcl helpers defined at startup" (D-34)).

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "binding_fixture.hpp"
#include "hefesto/cmd/prompt.hpp"
#include "hefesto/script/binding/output.hpp"

namespace script = hefesto::script;
namespace diag = hefesto::diag;
using script::test::BindingFixture;

namespace {

/// Replaces `puts` with a recorder of "<stream>:<text>" lines in ::out.
void capture_puts(BindingFixture& f) {
  f.value("rename puts ::hefesto_test_puts; set ::out {}; "
          "proc puts {args} {lappend ::out \"[lindex $args end-1]:[lindex $args end]\"}");
}

}  // namespace

TEST_CASE("Output is escaped for puts", "[cap:scripting-api][req:Output, flushing and escaping]") {
  CHECK(script::escape_output("cell a$abc") == "cell a\\$abc");
  CHECK(script::escape_output("[x] \"q\" \\") == "\\[x\\] \\\"q\\\" \\\\");
  CHECK(script::puts_script(script::OutputStream::err, "a") == "puts -nonewline stderr \"a\"");
}

TEST_CASE("Dollar in a cell name", "[cap:scripting-api][req:Output, flushing and escaping]") {
  BindingFixture f;
  capture_puts(f);
  REQUIRE(script::write_output(*f.interp, f.editor.script.output, script::OutputStream::out, "cell a$abc").ok());
  CHECK(f.value("lindex $::out 0") == "stdout:cell a$abc");
}

TEST_CASE("Dollar naming an existing variable", "[cap:scripting-api][req:Output, flushing and escaping]") {
  BindingFixture f;
  capture_puts(f);
  f.value("set VDD VPWR");
  REQUIRE(script::write_output(*f.interp, f.editor.script.output, script::OutputStream::out, "net $VDD [x]").ok());
  CHECK(f.value("lindex $::out 0") == "stdout:net $VDD [x]");
}

TEST_CASE("Diagnostics print through puts", "[cap:scripting-api][req:Output, flushing and escaping]") {
  BindingFixture f;
  capture_puts(f);
  script::ScriptOutputSink sink{*f.interp, f.editor.script.output, diag::Format::magic};
  sink.emit(diag::make("SCRIPT-BIND-003", "no netgen"));
  diag::Diagnostic partial = diag::make("SCRIPT-TCL-001", "error $x");
  partial.no_newline = true;
  sink.emit(partial);
  sink.emit(diag::make_native("SCRIPT-BIND-001", "native only"));  // dropped in the magic format
  CHECK(f.value("set ::out") == "{stdout:no netgen\n} {stderr:error $x}");
  // Suspended output prints nothing.
  f.editor.script.output.suspended = true;
  sink.emit(diag::make("SCRIPT-BIND-003", "hidden"));
  CHECK(f.value("llength $::out") == "2");
}

TEST_CASE("Batch mode confirmation", "[cap:scripting-api][req:Prompts and dialogs through the script layer]") {
  BindingFixture f;
  capture_puts(f);
  f.value("rename gets ::hefesto_test_gets; proc gets {channel} {return \"yes\\n\"}");
  CHECK(script::read_line(*f.interp, f.editor.script.output, "Continue? ") == "yes");
  CHECK(f.value("lindex $::out 0") == "stdout:Continue? ");
  f.value("proc gets {channel} {return {}}");
  CHECK_FALSE(script::read_line(*f.interp, f.editor.script.output, "").has_value());
  f.value("proc gets {channel} {return abcdef}");
  CHECK(script::read_line(*f.interp, f.editor.script.output, "", 4) == "abc");
  // The console stubs of batch mode are no-ops.
  for (const char* stub : {"magic::suspendout", "magic::resumeout", "magic::dialog", "magic::consolegeometry",
                           "magic::consolefocus"}) {
    CHECK(f.value(stub) == "");
  }
}

TEST_CASE("Console prompts go to magic::dialog", "[cap:scripting-api][req:Prompts and dialogs through the script layer]") {
  BindingFixture f{true, true, true};
  CHECK(f.value("info commands magic::suspendout") == "");  // the console defines them
  f.value("proc magic::dialog {args} {set ::asked $args; return \"answer\\n\"}");
  CHECK(script::read_line(*f.interp, f.editor.script.output, "name: ") == "answer");
  CHECK(f.value("set ::asked") == "{} {name: }");
}

TEST_CASE("Multiple-choice questions use tk_dialog", "[cap:scripting-api][req:Prompts and dialogs through the script layer]") {
  BindingFixture f;
  script::ScriptPromptHost host{*f.interp};
  const std::array<std::string, 2> answers{"no", "yes"};
  // Without Tk there is no tk_dialog: the default answer.
  CHECK(host.ask("Really?", answers, 0) == 0);
  f.value("proc tk_dialog {args} {set ::dialog $args; return 1}");
  CHECK(host.ask("Quit $x?", answers, 0) == 1);
  CHECK(f.value("set ::dialog") == ".dialog Dialog {Quit $x?} {} 0 no yes");
  f.value("proc tk_dialog {args} {return nope}");
  CHECK(host.ask("Really?", answers, 1) == 1);
}

// Regression (SCRIPT-PDK-07, SCRIPT-PCELL-12; Q-SCRIPT-48): TxDialog gives -1
// when tk_dialog gives no integer, and Magic's callers that stop only on "no"
// go ahead. try_ask() tells that case apart; cmd::confirm() goes ahead with
// magic-compat ON and declines with it OFF.
TEST_CASE("A confirmation that cannot be asked", "[cap:scripting-api][req:Prompts and dialogs through the script layer]") {
  BindingFixture f;
  script::ScriptPromptHost host{*f.interp};
  const std::array<std::string, 2> answers{"no", "yes"};
  CHECK_FALSE(host.try_ask("Really?", answers, 0).has_value());  // no tk_dialog without Tk
  CHECK(hefesto::cmd::confirm(&host, "Really?", true));
  CHECK_FALSE(hefesto::cmd::confirm(&host, "Really?", false));
  CHECK(hefesto::cmd::confirm(nullptr, "Really?", true));
  CHECK_FALSE(hefesto::cmd::confirm(nullptr, "Really?", false));
  hefesto::cmd::HeadlessPromptHost headless;
  CHECK_FALSE(hefesto::cmd::try_confirm(&headless, "Really?").has_value());
  CHECK(headless.ask("Really?", answers, 0) == 0);  // plain questions keep their default
  f.value("proc tk_dialog {args} {return -1}");
  CHECK_FALSE(host.try_ask("Really?", answers, 0).has_value());
  f.value("proc tk_dialog {args} {return 0}");
  CHECK_FALSE(hefesto::cmd::confirm(&host, "Really?", true));  // an answered "no" declines in both settings
  f.value("proc tk_dialog {args} {return 1}");
  CHECK(hefesto::cmd::confirm(&host, "Really?", false));
}

TEST_CASE("Piped startup commands are not lost", "[cap:scripting-api][req:Terminal input queue without console]") {
  std::vector<std::string> terminal{"box 0 0 1 1\nquit -nopr", "ompt\n"};
  std::size_t next = 0;
  script::TerminalInputQueue queue{[&]() -> std::optional<std::string> {
                                     if (next == terminal.size()) {
                                       return std::nullopt;
                                     }
                                     return terminal[next++];
                                   },
                                   "already buffered\n"};
  queue.queue("typed in window\n");
  CHECK(queue.next_line() == "already buffered");
  CHECK(queue.next_line() == "typed in window");
  CHECK(queue.next_line() == "box 0 0 1 1");
  CHECK(queue.next_line() == "quit -noprompt");
  CHECK_FALSE(queue.next_line().has_value());
}

TEST_CASE("Escape removes the incomplete queued line", "[cap:scripting-api][req:Terminal input queue without console]") {
  script::TerminalInputQueue queue{{}};
  queue.queue("box\npai");
  CHECK(queue.cancel_partial_line() == 3);
  CHECK(queue.next_line() == "box");
  CHECK_FALSE(queue.next_line().has_value());
}

TEST_CASE("Relative cell path survives cd", "[cap:scripting-api][req:Directory change rebases cell paths]") {
  namespace fs = std::filesystem;
  const fs::path saved = fs::current_path();
  const fs::path w = fs::weakly_canonical(fs::temp_directory_path()) / "hefesto_binding_cd_test";
  fs::remove_all(w);
  fs::create_directories(w / "sub");
  fs::create_directories(w / "lib");
  {
    BindingFixture f;
    f.value("cd {" + w.string() + "}");
    hefesto::db::CellDef* inv = f.editor.cells.create("inv", f.editor.technology->plane_count()).value();
    inv->set_file_path("./lib/inv.mag");
    hefesto::db::CellDef* abs = f.editor.cells.create("abs", f.editor.technology->plane_count()).value();
    abs->set_file_path("/x/abs.mag");
    f.value("cd sub");
    CHECK(fs::path{f.value("pwd")} == fs::canonical(w / "sub"));
    CHECK((fs::current_path() / inv->file_path()).lexically_normal() == fs::canonical(w) / "lib/inv.mag");
    CHECK(inv->file_path() == fs::path{"../lib/inv.mag"});
    CHECK(abs->file_path() == fs::path{"/x/abs.mag"});
    // A failed cd changes nothing.
    CHECK_FALSE(f.eval("cd nosuchdir").ok());
    CHECK(inv->file_path() == fs::path{"../lib/inv.mag"});
  }
  fs::current_path(saved);
  fs::remove_all(w);
}

TEST_CASE("No implicit exec", "[cap:scripting-api][req:Generic Tcl helpers defined at startup]") {
  BindingFixture f;
  CHECK(f.value("set auto_noexec") == "1");
  CHECK(f.error("ls") == "invalid command name \"ls\"");
}

TEST_CASE("Generic helpers", "[cap:scripting-api][req:Generic Tcl helpers defined at startup]") {
  BindingFixture f;
  capture_puts(f);
  f.value("echo a {b c}");
  CHECK(f.value("lindex $::out 0") == "stdout:a {b c}");
  f.take_printed();
  CHECK(f.value("netgen -batch lvs") == "");
  CHECK(f.value("xcircuit") == "");
  CHECK(f.value("irsim") == "");
  const std::string printed = f.take_printed();
  CHECK(printed.find("\"lvs\"") != std::string::npos);
  CHECK(printed.find("\"xcircuit\" is not available") != std::string::npos);
  CHECK(printed.find("\"irsim\"") != std::string::npos);
  CHECK(f.value("list $UsingIRSIM $UsingXCircuit $UsingNetgen") == "0 0 0");
  CHECK(f.value("magic::drcstate busy") == "");
  CHECK(f.value("namespace origin drcstate") == "::magic::drcstate");
}
