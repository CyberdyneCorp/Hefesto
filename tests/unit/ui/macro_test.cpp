// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Key names, macro tables, `macro`/`imacro`, interactive keystrokes, the
// default bindings and the key-binding file (capabilities
// ui-command-dispatch, ui-tools-and-input "Default key bindings").

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/ui/bindings.hpp"
#include "hefesto/ui/input.hpp"
#include "hefesto/ui/macro_file.hpp"
#include "hefesto/ui/macros.hpp"
#include "ui_fixture.hpp"

namespace ui = hefesto::ui;
using hefesto::test::UiSession;

namespace {

ui::Key key(std::string_view name) {
  const auto k = ui::parse_key_name(name);
  REQUIRE(k.ok());
  return k.value();
}

}  // namespace

// --- Macro key names --------------------------------------------------------------

TEST_CASE("key names parse with modifier prefixes and normalize", "[cap:ui-command-dispatch][req:Macro key names]") {
  // "Control shorthand": ^R, Control_r and the control character are one key.
  CHECK(key("^R") == key("Control_r"));
  CHECK(key("^R") == ui::Key{0x12, 0});
  CHECK(key("^A") == key(std::string{'\x01'}));
  // "Shifted letter".
  CHECK(key("Shift_s") == key("S"));
  CHECK(key("Shift_a") == ui::Key{'A', 0});
  // Control with Shift keeps both modifiers; Meta keeps them too.
  CHECK(key("Control_Shift_XK_q") == ui::Key{'Q', ui::kControl | ui::kShift});
  CHECK(key("Meta_a") == ui::Key{'a', ui::kMeta});
  CHECK(key("Alt_a") == key("Meta_a"));
  // XK_ prefix, named keys, buttons, <del>, quotes.
  CHECK(key("XK_Left") == ui::Key{0xFF51, 0});
  CHECK(key("Shift_XK_space") == ui::Key{0x20, ui::kShift});
  CHECK(key("XK_space") == key(" "));
  CHECK(key("Button1") == ui::button_key(1));
  CHECK(key("XK_Pointer_Button4") == ui::button_key(4));
  CHECK(key("Control_Button3") == ui::button_key(3, ui::kControl));
  CHECK(key("<del>") == ui::Key{0x7F, 0});
  CHECK(key("'a'") == key("a"));
  CHECK(key("KP_4") != key("4"));
  CHECK(key("XK_plus") == key("+"));
  CHECK(key("^") == ui::Key{'^', 0});
  const auto bad = ui::parse_key_name("NoSuchKey");
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.error().text == "Unrecognized macro name NoSuchKey");
  CHECK(bad.error().code == ui::codes::kMacroName);
}

TEST_CASE("key display names", "[cap:ui-command-dispatch][req:Macro key names]") {
  CHECK(ui::key_display_name(ui::Key{0x01, 0}) == "^A");
  CHECK(ui::key_display_name(ui::Key{0x7F, 0}) == "<del>");
  CHECK(ui::key_display_name(ui::Key{'j', 0}) == "j");
  CHECK(ui::key_display_name(key("XK_Left")) == "XK_Left");
  CHECK(ui::key_display_name(key("Control_Shift_XK_Left")) == "Control_Shift_XK_Left");
  CHECK(ui::key_display_name(key("Meta_Capslock_a")) == "Meta_Capslock_XK_a");
  CHECK(ui::key_display_name(ui::button_key(1)) == "XK_Pointer_Button1");
  CHECK(ui::key_display_name(ui::Key{0x1234, ui::kShift}) == "0x11234");
  CHECK(ui::keysym_name(0xFF55) == "Prior");
  CHECK(ui::keysym_from_name("Page_Up") == 0xFF55U);
}

// --- Macro tables ----------------------------------------------------------------

TEST_CASE("macro tables: define, help, delete and Magic's listing order", "[cap:ui-command-dispatch][req:Macro tables]") {
  ui::MacroTables t;
  t.define("box", key("a"), "select", std::string{"help a"}, false);
  REQUIRE(t.find("box", key("a")) != nullptr);
  CHECK(t.find("box", key("a"))->help == "help a");
  // Redefining replaces text and flag and clears the help.
  t.define("box", key("a"), "select more", std::nullopt, true);
  CHECK(t.find("box", key("a"))->text == "select more");
  CHECK(t.find("box", key("a"))->interactive);
  CHECK(t.find("box", key("a"))->help.empty());
  CHECK(t.find("wiring", key("a")) == nullptr);
  // A deleted entry stays and ends listings (Magic's quirk).
  t.define("box", key("b"), "box", std::nullopt, false);
  CHECK(t.erase("box", key("a")));
  CHECK_FALSE(t.erase("box", key("a")));
  CHECK(t.find("box", key("a")) == nullptr);
  const auto entries = t.entries("box");
  CHECK(entries.size() == 2);
  // Copy defines every live binding in the other table.
  t.copy("box", "pick");
  REQUIRE(t.find("pick", key("b")) != nullptr);
  CHECK(t.find("pick", key("a")) == nullptr);
  CHECK(t.has_table("pick"));
  CHECK_FALSE(t.has_table("nettool"));
}

TEST_CASE("each tool has its own bindings", "[cap:ui-command-dispatch][req:Macro tables][golden:UI-G-012]") {
  UiSession s;
  REQUIRE(s.run(R"(macro Button1 "box move bl cursor")").ok());
  s.run("tool wiring");
  const std::string wiring = UiSession::printed(s.run("macro Button1"));
  CHECK(wiring == R"(Macro 'XK_Pointer_Button1' contains "trackwire pick")");
  s.run("tool box");
  CHECK(UiSession::printed(s.run("macro Button1")) == R"(Macro 'XK_Pointer_Button1' contains "box move bl cursor")");
}

// --- macro and imacro commands -------------------------------------------------------

TEST_CASE("macro defines, shows and deletes bindings",
          "[cap:ui-command-dispatch][req:macro and imacro commands][golden:UI-G-010][golden:UI-G-011]") {
  UiSession s;
  REQUIRE(s.run(R"(macro z "zoom .5")").ok());
  CHECK(UiSession::printed(s.run("macro z")) == R"(Macro 'z' contains "zoom .5")");
  CHECK(s.run("macro list z").value.to_string() == "zoom .5");
  // "Delete binding".
  REQUIRE(s.run(R"(macro z "")").ok());
  CHECK(UiSession::printed(s.run("macro z")).empty());
  const auto action = ui::handle_key(s.editor, key("z"));
  CHECK(action.kind == ui::KeyActionKind::unknown);
  REQUIRE(action.error.has_value());
  CHECK(action.error->text == "Unknown macro or short command: 'z'");
  // UI-G-011: ^R and Control_r are one key; Shift_a is A.
  CHECK(UiSession::printed(s.run("macro ^R")) == UiSession::printed(s.run("macro Control_r")));
  s.run(R"(macro Shift_a "box")");
  CHECK(UiSession::printed(s.run("macro A")) == R"(Macro 'A' contains "box")");
  // Help text, interactive macros and bad names.
  s.run(R"(macro q "quit" "leave")");
  CHECK(UiSession::printed(s.run("macro help q")) == R"(Macro 'q' contains "leave")");
  s.run(R"(macro help q "go away")");
  CHECK(s.run("macro help list q").value.to_string() == "go away");
  s.run(R"(imacro k "label ")");
  CHECK(UiSession::printed(s.run("macro k")) == R"(Interactive macro 'k' contains "label ")");
  CHECK(UiSession::first_error(s.run("macro Bogus_Key_Name")) == "UI-MACRO-001: Unrecognized macro name Bogus_Key_Name");
  CHECK(UiSession::first_error(s.run("macro a b c d e")).starts_with("UI-USAGE-001: Usage: macro [client]"));
}

TEST_CASE("macro lists a table, flat or reversed, filtered or help",
          "[cap:ui-command-dispatch][req:macro and imacro commands][golden:UI-G-009]") {
  UiSession s;
  ui::install_defaults(s.editor);
  // List the color client's table.
  const std::string color = UiSession::printed(s.run("macro color"));
  CHECK(color.find(R"(Macro 'XK_Pointer_Button1' contains "pushbutton left")") != std::string::npos);
  CHECK(color.find(R"(Macro 'u' contains "undo")") != std::string::npos);
  const auto flat = s.run("macro color list");
  const auto words = flat.value.to_string();
  CHECK(words.find("XK_Pointer_Button1 {pushbutton left}") != std::string::npos);
  CHECK(s.run("macro color list -reverse").value.to_string().find("{pushbutton left} XK_Pointer_Button1") !=
        std::string::npos);
  CHECK(s.run("macro color list search next").value.to_string() == "+ {color next}");
  // imacro lists only interactive macros.
  const std::string interactive = UiSession::printed(s.run("imacro"));
  CHECK(interactive.find("Interactive macro 'p' contains \"paint \"") != std::string::npos);
  CHECK(interactive.find("Macro 's'") == std::string::npos);
  // A table with no entries, and an unknown table.
  s.editor.macros.define("empty2", key("a"), "x", std::nullopt, false);
  s.editor.macros.erase("empty2", key("a"));
  s.editor.tools.add(ui::Tool{"empty2", {}, 0, false});
  CHECK(UiSession::printed(s.run("macro empty2")) == "No macros are defined for this client.");
  s.editor.tools.add(ui::Tool{"ghost", {}, 0, false});
  const auto ghost = s.run("macro ghost");
  CHECK(UiSession::first_error(ghost) == "UI-MACRO-002: Cannot get macro list from current window.");
  CHECK(ghost.value.to_string().find("box") != std::string::npos);
  CHECK(ghost.ok());  // Magic returns the table names as an ok Tcl result (golden UI-G-049)
}

TEST_CASE("client-qualified, unavailable-client and copied tables",
          "[cap:ui-command-dispatch][req:macro and imacro commands][golden:UI-G-013][golden:UI-G-014]") {
  UiSession s;
  // "Client-qualified binding".
  REQUIRE(s.run(R"(macro color plus "color next")").ok());
  const auto plus = ui::handle_key(s.editor, key("+"), "color");
  CHECK(plus.kind == ui::KeyActionKind::run);
  CHECK(plus.text == "color next");
  // "Unavailable client fails quietly".
  const auto quiet = s.run(R"(macro mywind3d L "level up")");
  CHECK(quiet.ok());
  CHECK(quiet.diagnostics.empty());
  // `macro copy` copies the current table and registers the tool.
  REQUIRE(s.run("macro copy mytool").ok());
  REQUIRE(s.editor.tools.find("mytool") != nullptr);
  CHECK_FALSE(s.editor.tools.find("mytool")->handler);
  CHECK(UiSession::printed(s.run("macro mytool s")) == R"(Macro 's' contains "select")");
  // At most ten tools.
  for (int i = 0; i < 5; ++i) {
    REQUIRE(s.run("macro copy t" + std::to_string(i)).ok());
  }
  CHECK(UiSession::first_error(s.run("macro copy eleventh")) ==
        "UI-TOOL-001: Can't add tool \"eleventh\":  no space in button handler");
}

// --- Interactive keystroke handling ---------------------------------------------------

TEST_CASE("keys run macros, open the prompt, or report an unknown key",
          "[cap:ui-command-dispatch][req:Interactive keystroke handling]") {
  UiSession s;
  ui::install_defaults(s.editor);
  // "Repeat last long command": the entered line becomes the `.` macro.
  ui::remember_command_line(s.editor, "paint m1");
  const auto repeat = ui::handle_key(s.editor, key("."));
  CHECK(repeat.kind == ui::KeyActionKind::run);
  CHECK(repeat.text == "paint m1");
  // "Undefined key".
  const auto j = ui::handle_key(s.editor, key("j"));
  REQUIRE(j.error.has_value());
  CHECK(j.error->text == "Unknown macro or short command: 'j'");
  // Interactive macros open the prompt pre-filled; a leading ':' is removed.
  const auto p = ui::handle_key(s.editor, key("p"));
  CHECK(p.kind == ui::KeyActionKind::prompt);
  CHECK(p.text == "paint ");
  const auto colon = ui::handle_key(s.editor, key("XK_colon"));
  CHECK(colon.kind == ui::KeyActionKind::prompt);
  CHECK(colon.text.empty());
  // Without macros `:`/`;` still open the prompt and Return does nothing.
  UiSession bare;
  bare.editor.tools.add(ui::Tool{"box", {}, 0, true});
  CHECK(ui::handle_key(bare.editor, key(";")).kind == ui::KeyActionKind::prompt);
  CHECK(ui::handle_key(bare.editor, key("Return")).kind == ui::KeyActionKind::none);
  // An empty line is not remembered.
  ui::remember_command_line(s.editor, "");
  CHECK(ui::handle_key(s.editor, key(".")).text == "paint m1");
}

// --- Default key bindings -------------------------------------------------------------

TEST_CASE("the default bindings of the system startup file and the tool setup",
          "[cap:ui-tools-and-input][req:Default key bindings][golden:UI-G-049]") {
  UiSession s;
  ui::install_defaults(s.editor);
  const auto run = [&](std::string_view k, std::string_view table = "box") {
    const ui::Macro* m = s.editor.macros.find(table, key(k));
    return m == nullptr ? std::string{"<none>"} : m->text;
  };
  // "s selects", "Ctrl-wheel zoom".
  CHECK(run("s") == "select");
  // toolkit.tcl's gencell binding, defined before the system startup file (UI-G-049).
  CHECK(run("^P") == "magic::gencell {} ; raise .params");
  CHECK(run("^P", "wiring") == "magic::gencell {} ; raise .params");
  CHECK(run("Control_XK_Pointer_Button4") == "zoom 1.6");
  CHECK(run("^N") == "<none>");
  CHECK(run("Control_XK_i") == "select less cell");
  CHECK(run("XK_KP_4") == "stretch l 1i");
  CHECK(run("XK_4") == "stretch l 1i");
  CHECK(run("Shift_XK_KP_4") == "stretch l 1");
  CHECK(run("XK_F5") == "paint m1");
  CHECK(run("!") == "drc find; findbox zoom; zoom 16");
  CHECK(run("Button1") == "*bypass box move bl cursor");
  CHECK(run("Control_Button2") == "*bypass paint cursor");
  CHECK(run("space") == "tool");
  CHECK(run("Shift_space") == "tool box");
  CHECK(run("+") == "autoincr 1");
  CHECK(run("greater") == "pushstack");
  CHECK(s.editor.macros.find("box", key("l"))->interactive);
  // The client tables.
  CHECK(run("Button2", "color") == "pushbutton middle");
  CHECK(run("minus", "color") == "color last");
  // Regression (golden UI-G-049, `macro layout list`): the clients of optional modules this build lacks
  // (netlist, wind3d) are not registered, so their startup bindings define no table, as in Magic.
  CHECK(run("Button3", "netlist") == "<none>");
  CHECK(run("L", "wind3d") == "<none>");
  CHECK_FALSE(s.editor.macros.has_table("wind3d"));
  CHECK_FALSE(s.editor.macros.has_table("netlist"));
  CHECK(s.run("macro layout list").value.to_string() == "pick nettool wiring color box");
  // Box tool current, glyph 0.
  REQUIRE(s.editor.tools.current() != nullptr);
  CHECK(s.editor.tools.current()->name == "box");
  CHECK(ui::state(s.editor).cursor_glyph == 0);
  CHECK(ui::startup_bindings().size() > 150);
}

// --- key-binding file ---------------------------------------------------------------------

TEST_CASE("a key-binding file holds macro lines", "[cap:ui-command-dispatch][req:macro and imacro commands]") {
  UiSession s;
  const auto r = ui::read_macro_file(s.editor,
                                     "# my bindings\n"
                                     "macro z {zoom 4}\n"
                                     "imacro t \"label \"\n"
                                     "box 0 0 1 1\n"
                                     "macro Bogus_Name\n",
                                     "keys.hef");
  for (const auto& d : r.diagnostics) UNSCOPED_INFO(d.code + ": " + d.text);
  CHECK(r.applied == 3);
  REQUIRE(s.editor.macros.find("box", key("z")) != nullptr);
  CHECK(s.editor.macros.find("box", key("z"))->text == "zoom 4");
  CHECK(s.editor.macros.find("box", key("t"))->interactive);
  REQUIRE(r.diagnostics.size() == 2);
  CHECK(r.diagnostics[0].code == ui::codes::kUsage);
  CHECK(r.diagnostics[1].code == ui::codes::kMacroName);
  CHECK(std::get<hefesto::diag::SourceLocation>(r.diagnostics[1].location).line == 5);
  // A malformed file reports the parse error and applies nothing after it.
  const auto broken = ui::read_macro_file(s.editor, "macro y {unclosed", "bad.hef");
  CHECK(broken.applied == 0);
  CHECK_FALSE(broken.diagnostics.empty());
}

// m4-scripting tasks.md 13.2: the file is Tcl (as a startup file's macro
// lines are), read in a safe child interpreter where only `macro` and
// `imacro` exist: every other command, also a substituted one, is reported
// and does nothing.
TEST_CASE("a key-binding file is Tcl in which only macro lines run",
          "[cap:ui-command-dispatch][req:macro and imacro commands]") {
  UiSession s;
  const auto r = ui::read_macro_file(s.editor,
                                     "macro z {zoom 4}; macro y \"box \\\n"
                                     "  grow\"\n"
                                     "set x 1\n"
                                     "macro w [exec touch /tmp/hefesto-macro-file]\n"
                                     "::tcl::mathop::+ 1 2\n"
                                     "macro v $nosuch\n"
                                     "macro u {select}\n",
                                     "keys.tcl");
  CHECK(r.applied == 4);  // z, y, w (with an empty text: `exec` ran nothing) and u
  REQUIRE(s.editor.macros.find("box", key("y")) != nullptr);
  CHECK(s.editor.macros.find("box", key("y"))->text == "box  grow");  // Tcl: backslash-newline-blanks is one space
  CHECK(s.editor.macros.find("box", key("u")) != nullptr);
  std::vector<std::pair<std::string, std::uint32_t>> seen;
  for (const auto& d : r.diagnostics) {
    seen.emplace_back(d.text, std::get<hefesto::diag::SourceLocation>(d.location).line);
  }
  using Seen = std::vector<std::pair<std::string, std::uint32_t>>;
  CHECK(seen == Seen{{"a key-binding file holds only macro and imacro lines, not \"set\"", 3},
                     {"a key-binding file holds only macro and imacro lines, not \"exec\"", 4},
                     {"a key-binding file holds only macro and imacro lines, not \"::tcl::mathop::+\"", 5},
                     {"can't read \"nosuch\": no such variable", 6}});
  CHECK_FALSE(std::filesystem::exists("/tmp/hefesto-macro-file"));
  CHECK(s.editor.script.interp == nullptr);  // the session's interpreter is not involved
}

// Q-UI-42: Magic's golden captures run on the null display, where only
// single-character and ^X key names parse (utils/macros.c:MacroKey), so the
// system startup file defines just those bindings. Defined in the same
// order (toolkit.tcl's ^P, then magic/proto.magicrc.in), Hefesto's table
// lists them in exactly Magic's headless order (golden UI-G-009, line 1 of
// `macro list`): the listing differences of UI-G-009/013/049 come only from
// the keysym bindings Hefesto adds (Q-UI-12) and the hash growth they cause.
TEST_CASE("the null-display bindings list in Magic's headless hash order",
          "[cap:ui-command-dispatch][req:Macro tables][golden:UI-G-009]") {
  struct Binding {
    const char* key;
    const char* text;
  };
  const Binding startup[] = {
      {"^P", "magic::gencell {} ; raise .params"},  // toolkit.tcl, sourced before the startup file
      {"a", "select visible"}, {"A", "select more visible"}, {"^A", "select less visible"},
      {"b", "box"}, {"B", "findbox"}, {"c", "copy"}, {"d", "delete"}, {"^D", "erase $"}, {"e", "edit"},
      {"f", "sideways"}, {"F", "upsidedown"}, {"g", "grid"}, {"G", "grid 2"}, {"i", "select cell"},
      {"I", "select more cell"}, {"l", "label "}, {"L", "shell ls"}, {"^L", "redraw"}, {"m", "move"},
      {"M", "stretch"}, {"^N", ""}, {"o", "openwindow"}, {"O", "closewindow"}, {"p", "paint "},
      {"r", "clockwise"}, {"R", "clockwise 270"}, {"^R", "clockwise 180"}, {"s", "select"},
      {"S", "select more"}, {"^S", "select less"}, {"u", "undo"}, {"U", "redo"}, {"v", "view"},
      {"V", "xview"}, {"w", "writeall"}, {"W", "writeall force"}, {"x", "expand"}, {"X", "unexpand"},
      {"^X", "expand toggle"}, {"z", "zoom .5"}, {"Z", "zoom 2"}, {"^Z", "findbox zoom"}, {"?", "drc why"},
      {"!", "drc find; findbox zoom; zoom 16"}, {"/", "select area; what ; select clear"},
      {",", "select clear"}, {" ", "tool"},
  };
  ui::MacroTables t;
  for (const Binding& b : startup) {
    const std::string text = b.text;
    if (text.empty()) {
      (void)t.erase("box", key(b.key));  // `macro ^N ""` deletes (nothing to delete)
    } else {
      t.define("box", key(b.key), text, std::nullopt, text == "label " || text == "paint ");
    }
  }
  const auto order = [&t](std::string_view table) {
    std::string out;
    for (const auto& [k, macro] : t.entries(table)) {
      if (macro != nullptr) {
        out += (out.empty() ? "" : " ") + ui::key_display_name(k);
      }
    }
    return out;
  };
  CHECK(order("box") ==
        "m I ^A o p L ^D M r s O , u v R / w S x ^L U z V W X ^P Z ^R ^S ^X b a ? ^Z c d e A f B g   ! i G F l");
  // tools.tcl's `macro copy wiring` (golden UI-G-049, `macro wiring list`).
  t.copy("box", "wiring");
  CHECK(order("wiring") ==
        "^A I m o ^D L p M r O s u , R v S w / ^L x U V z W ^P X ^R Z ^S ^X a b c ^Z ? d A e B f g   i ! F G l");
}
