// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The schematic canvas's system clipboard link (schematic_clipboard.hpp;
// M8 task 13.2): Copy runs `schematic clipboard copy` and puts its text on
// the system clipboard; Paste reads the system clipboard into `schematic
// clipboard set <text>` and runs `schematic paste <x> <y>`. A fake
// clipboard stands for Raven's.

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string>
#include <vector>

#include "../schem/edit/edit_fixture.hpp"
#include "hefesto/pcell/framework/tcl_list.hpp"
#include "hefesto/shell/schematic_clipboard.hpp"

using namespace hefesto::shell;
namespace cmd = hefesto::cmd;
namespace schem = hefesto::schem;
using hefesto::test::active;
using hefesto::test::new_schematic;
using hefesto::test::run;
using hefesto::test::SchemSession;

namespace {

class FakeClipboard final : public ClipboardHost {
 public:
  void set_text(const std::string& text) override {
    contents = text;
    ++writes;
  }
  [[nodiscard]] std::string text() override { return contents; }

  std::string contents;
  int writes = 0;
};

cmd::Invocation copy_invocation() {
  return cmd::Invocation{"schematic", {"clipboard", "copy"}, cmd::Origin::gui, std::nullopt};
}

std::vector<std::string> words(const std::string& line) {
  auto parsed = hefesto::pcell::split_tcl_list(line);
  REQUIRE(parsed.ok());
  return parsed.value();
}

}  // namespace

TEST_CASE("Copy puts the copied schematic text on the system clipboard", "[cap:schematic-editing][req:Clipboard]") {
  FakeClipboard clipboard;
  SchematicClipboardLink link{clipboard};
  CHECK(link.copy_command() == "schematic clipboard copy");
  CHECK(link.copy_pending());

  // Another command's result is not the copy.
  const cmd::Invocation other{"schematic", {"clipboard"}, cmd::Origin::gui, std::nullopt};
  CHECK_FALSE(link.copied(other, cmd::CommandResult::success(std::string{"v {}"})));
  CHECK(link.copy_pending());

  CHECK(link.copied(copy_invocation(), cmd::CommandResult::success(std::string{"v {xschem}\nC {res.sym} 0 0 0 0 {}"})));
  CHECK(clipboard.contents == "v {xschem}\nC {res.sym} 0 0 0 0 {}");
  CHECK_FALSE(link.copy_pending());

  // A copy nobody asked for (typed in the command bar) leaves the system clipboard alone.
  CHECK_FALSE(link.copied(copy_invocation(), cmd::CommandResult::success(std::string{"typed"})));
  CHECK(clipboard.writes == 1);

  // Nothing selected: an empty result leaves the clipboard unchanged; a failure too.
  (void)link.copy_command();
  CHECK_FALSE(link.copied(copy_invocation(), cmd::CommandResult::success(std::string{})));
  (void)link.copy_command();
  CHECK_FALSE(link.copied(copy_invocation(),
                          cmd::CommandResult::failure(hefesto::diag::make("SCHEM-CMD-001", "no document"))));
  CHECK(clipboard.writes == 1);

  // A copy that never reached the dispatcher does not wait for a later one.
  (void)link.copy_command();
  link.end_copy();
  CHECK_FALSE(link.copied(copy_invocation(), cmd::CommandResult::success(std::string{"late"})));
  CHECK(clipboard.contents != "late");
}

TEST_CASE("Paste sets the schematic clipboard from the system clipboard and pastes at the pointer",
          "[cap:schematic-editing][req:Clipboard]") {
  FakeClipboard clipboard;
  const SchematicClipboardLink link{clipboard};

  const ClipboardPaste empty = link.paste_commands(schem::Point{0, 0});
  CHECK(empty.lines.empty());
  CHECK(empty.message == "Paste: the clipboard holds no text");

  // Braces, quotes, brackets and newlines reach `clipboard set` unchanged.
  clipboard.contents = "v {xschem version=3.4.8RC file_version=1.3}\nC {res.sym} 0 0 0 0 {name=R1 value=\"[1k]\"}\n";
  const ClipboardPaste paste = link.paste_commands(schem::Point{123.4, -46});
  REQUIRE(paste.lines.size() == 2);
  CHECK(words(paste.lines[0]) == std::vector<std::string>{"schematic", "clipboard", "set", clipboard.contents});
  CHECK(paste.lines[1] == "schematic paste 120 -50");  // snapped to 10
  CHECK(link.paste_commands(schem::Point{-4, 4}).lines[1] == "schematic paste 0 0");  // never "-0"
}

TEST_CASE("a system clipboard text above the paste limit runs nothing", "[cap:schematic-editing][req:Clipboard]") {
  FakeClipboard clipboard;
  const SchematicClipboardLink link{clipboard};
  clipboard.contents.assign(kMaxClipboardBytes, 'x');
  CHECK(link.paste_commands(schem::Point{0, 0}).lines.size() == 2);  // at the limit
  clipboard.contents.push_back('x');
  const ClipboardPaste refused = link.paste_commands(schem::Point{0, 0});
  CHECK(refused.lines.empty());
  CHECK(refused.message == "Paste: the clipboard text is too large (16777217 bytes, limit 16777216)");
}

// Regression (M8 round 6 review): the Console echoed a paste's `schematic
// clipboard set <text>` and a Copy's result whole, one Console row per line,
// so a 16 MiB clipboard of short lines made millions of rows.
TEST_CASE("the Console echoes clipboard text shortened", "[cap:schematic-editing][req:Clipboard]") {
  FakeClipboard clipboard;
  const SchematicClipboardLink link{clipboard};
  std::string text;
  for (int i = 0; i < 1000; ++i) text += "N 0 0 10 0 {}\n";
  clipboard.contents = text;
  const std::string set = link.paste_commands(schem::Point{0, 0}).lines.at(0);
  const std::string echoed = clipboard_echo(set, set);
  CHECK(echoed.find('\n') == std::string::npos);
  CHECK(echoed.size() < kClipboardEchoBytes + 32);
  CHECK(echoed.ends_with("(" + std::to_string(set.size()) + " bytes)"));
  CHECK(clipboard_echo("schematic clipboard copy", text) == "N 0 0 10 0 {} … (14000 bytes)");
  CHECK(clipboard_echo("schematic clipboard copy", "") == "");
  // Other lines and short texts are echoed as they are.
  CHECK(clipboard_echo("schematic netlist", text) == text);
  CHECK(clipboard_echo("schematic paste 0 0", "x\ny") == "x\ny");
  CHECK(clipboard_echo("schematic clipboard copy", "one line") == "one line");
}

TEST_CASE("Copy and Paste through the system clipboard copy an instance between schematics",
          "[cap:schematic-editing][req:Clipboard]") {
  SchemSession s;
  new_schematic(s);
  run(s, {"place", "res.sym", "40", "130", "-props", "name=R1 value=1k"});
  run(s, {"select", "instance", "R1"});

  FakeClipboard clipboard;
  SchematicClipboardLink link{clipboard};
  const std::vector<std::string> copy = words(link.copy_command());
  const cmd::CommandResult copied = s.schematic({copy.begin() + 1, copy.end()}, cmd::Origin::gui);
  REQUIRE(copied.ok());
  CHECK(link.copied(copy_invocation(), copied));
  CHECK(clipboard.contents.find("res.sym") != std::string::npos);

  // The schematic clipboard is cleared, as in another session; the system
  // clipboard still holds the text.
  run(s, {"clipboard", "set", ""});
  const ClipboardPaste paste = link.paste_commands(schem::Point{500, 500});
  REQUIRE(paste.lines.size() == 2);
  for (const std::string& line : paste.lines) {
    const cmd::CommandResult result = s.tcl(line);
    INFO(line);
    REQUIRE(result.ok());
  }
  REQUIRE(active(s).instances.size() == 2);
  CHECK(active(s).instances[1].symbol == "res.sym");
}
