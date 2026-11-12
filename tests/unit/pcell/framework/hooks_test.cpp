// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// capability pcell-toolkit "Selection-driven dialog refresh and key
// binding" and "Toolkit menu hooks" through BINDING's real binding: the
// toolkit.tcl hooks of install_toolkit (the `select` tag, Tk's `raise` for
// `.params`), the ^P macro text, and the menu-model event of the toolkit
// menus.

#include <catch2/catch_test_macros.hpp>

#include "framework_fixture.hpp"
#include "hefesto/cmd/script.hpp"
#include "hefesto/pcell/framework/hooks.hpp"
#include "hefesto/script/binding/bind.hpp"
#include "hefesto/script/events.hpp"

namespace pcell = hefesto::pcell;
namespace script = hefesto::script;
using hefesto::test::PcellSession;

namespace {

// The fixture with BINDING's binding (tags fire after Tcl-invoked
// commands) and toolkit.tcl's hooks, as app::run_startup installs them.
// The fixture's stand-in globals (`::select`, ...) stay, so the tests call
// the bound `magic::select`.
struct BoundSession : PcellSession {
  BoundSession() {
    const auto bound = script::bind_registry(*interp, registry, dispatcher);
    INFO((bound.diagnostics.empty() ? std::string{} : bound.diagnostics.front().text));
    REQUIRE(bound.ok());
    REQUIRE(pcell::install_toolkit(*interp, editor).ok());
  }

  [[nodiscard]] const pcell::DialogModel& dialog() const {
    REQUIRE(editor.script.gencell.dialog.has_value());
    return *editor.script.gencell.dialog;
  }
};

std::string error_of(const hefesto::cmd::CommandResult& r) {
  REQUIRE_FALSE(r.ok());
  REQUIRE_FALSE(r.diagnostics.empty());
  return r.diagnostics.front().text;
}

struct MenuCounter : script::ScriptObserver {
  int changes = 0;
  void on_menu_model_changed() override { ++changes; }
};

}  // namespace

TEST_CASE("the ^P macro is toolkit.tcl's text",
          "[cap:pcell-toolkit][req:Selection-driven dialog refresh and key binding]") {
  CHECK(pcell::kGencellKeyMacro == "magic::gencell {} ; raise .params");
}

TEST_CASE("Control-P opens and raises the edit dialog of the selected device",
          "[cap:pcell-toolkit][req:Selection-driven dialog refresh and key binding]") {
  BoundSession s;
  CHECK(s.value("magic::gencell tpdk::res x1") == "0");
  REQUIRE(s.tcl("magic::select cell x1").ok());
  SECTION("evaluated by the interpreter") {
    REQUIRE(s.tcl(pcell::kGencellKeyMacro).ok());
  }
  SECTION("dispatched as the layout window's key macro (shell InputRouter::run)") {
    const auto parsed = hefesto::cmd::parse_script(pcell::kGencellKeyMacro, "key");
    REQUIRE_FALSE(parsed.error);
    REQUIRE(parsed.commands.size() == 2);
    for (const auto& c : parsed.commands) {
      const hefesto::cmd::Invocation inv{c.words.front(), {c.words.begin() + 1, c.words.end()},
                                         hefesto::cmd::Origin::key, std::nullopt};
      const auto r = s.dispatcher.execute(inv);
      INFO((r.diagnostics.empty() ? std::string{} : r.diagnostics.front().text));
      REQUIRE(r.ok());
    }
  }
  CHECK(s.dialog().mode == pcell::DialogMode::edit_device);
  CHECK(s.dialog().instance == "x1");
  // gencell_dialog ends with `raise .params`, then the macro raises again.
  CHECK(s.dialog().raise_count == 2);
}

TEST_CASE("Control-P with nothing selected opens no dialog",
          "[cap:pcell-toolkit][req:Selection-driven dialog refresh and key binding]") {
  BoundSession s;
  REQUIRE(s.tcl("magic::select clear").ok());
  const auto r = s.tcl(pcell::kGencellKeyMacro);
  CHECK_FALSE(r.ok());  // the macro stops at gencell's error; `raise` does not run
  CHECK_FALSE(s.editor.script.gencell.dialog.has_value());
}

TEST_CASE("raise is Tk's raise for .params", "[cap:pcell-toolkit][req:Selection-driven dialog refresh and key binding]") {
  BoundSession s;
  // No dialog: .params does not exist (Tk's TkGetWindowFromObj message).
  CHECK(error_of(s.tcl("raise .params")) == "bad window path name \".params\"");
  CHECK(error_of(s.tcl("raise")) == "wrong # args: should be \"raise window ?aboveThis?\"");
  CHECK(error_of(s.tcl("raise .params .params .params")) == "wrong # args: should be \"raise window ?aboveThis?\"");
  REQUIRE(s.tcl("magic::gencell tpdk::res").ok());  // the new-device dialog
  const std::uint32_t opened = s.dialog().raise_count;
  CHECK(opened == 1);
  REQUIRE(s.tcl("raise .params").ok());
  REQUIRE(s.tcl("raise .params .params").ok());
  CHECK(s.dialog().raise_count == opened + 2);
  // Hefesto has no other Tk window.
  CHECK(error_of(s.tcl("raise .layout1")) == "bad window path name \".layout1\"");
  CHECK(error_of(s.tcl("raise .params .texthelper")) == "bad window path name \".texthelper\"");
  CHECK(s.dialog().raise_count == opened + 2);
  // The command-line form is the same command.
  REQUIRE(s.run("raise .params").ok());
  CHECK(s.dialog().raise_count == opened + 3);
}

TEST_CASE("the select tag refreshes an open dialog",
          "[cap:pcell-toolkit][req:Selection-driven dialog refresh and key binding]") {
  BoundSession s;
  CHECK(s.value("magic::gencell tpdk::res x1") == "0");
  CHECK(s.value("magic::gencell tpdk::res x2 w 2") == "0");
  CHECK(s.value("magic::tag select") == "magic::gencell_update %1");
  SECTION("closed dialog: selecting opens nothing") {
    REQUIRE(s.tcl("magic::select cell x2").ok());
    CHECK_FALSE(s.editor.script.gencell.dialog.has_value());
  }
  SECTION("Selecting refreshes the dialog") {
    REQUIRE(s.tcl("magic::gencell_dialog x1 {} {} {}").ok());
    REQUIRE(s.tcl("magic::select cell x2").ok());
    CHECK(s.dialog().instance == "x2");
    CHECK(s.dialog().mode == pcell::DialogMode::edit_device);
    // Another select form leaves it.
    REQUIRE(s.tcl("magic::select clear").ok());
    CHECK(s.dialog().instance == "x2");
  }
  SECTION("a select inside a device procedure does not refresh") {
    REQUIRE(s.tcl("magic::gencell_dialog x1 {} {} {}").ok());
    REQUIRE(s.tcl("proc tpdk::sel_defaults {} {return {}}; proc tpdk::sel_convert {p} {return $p}; "
                  "proc tpdk::sel_check {p} {return $p}; proc tpdk::sel_draw {p} {magic::select cell x2}")
                .ok());
    REQUIRE(s.tcl("magic::gencell_makecell tpdk::sel").ok());
    CHECK(s.dialog().instance == "x1");
  }
}

TEST_CASE("toolkit menu edits announce a menu-model change", "[cap:pcell-toolkit][req:Toolkit menu hooks]") {
  BoundSession s;
  MenuCounter counter;
  s.editor.script.events.add(counter);
  REQUIRE(s.tcl("magic::add_toolkit_menu .layout1.pane.top \"Devices 1\" pdk1").ok());
  REQUIRE(s.tcl("magic::add_toolkit_command .layout1.pane.top \"nmos (MOSFET)\" "
                "\"magic::gencell tpdk::res\" pdk1")
              .ok());
  REQUIRE(s.tcl("magic::add_toolkit_separator .layout1.pane.top pdk1").ok());
  CHECK(counter.changes == 3);
  // A failed edit changes nothing.
  REQUIRE_FALSE(s.tcl("magic::add_toolkit_separator .layout1.pane.top pdk2").ok());
  CHECK(counter.changes == 3);
  s.editor.script.events.remove(counter);
}
