// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The schematic editor's panels (schematic_panels.hpp; M8 task 13.2): the
// command lines each control issues and how the results are read back.
// Headless: the lines are checked as text (EDIT and CMD implement them).

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <set>
#include <string>
#include <vector>

#include "../schem/edit/edit_fixture.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/pcell/framework/tcl_list.hpp"
#include "hefesto/shell/schematic_panels.hpp"
#include "hefesto/ui/state.hpp"

using namespace hefesto::shell;
using namespace std::chrono_literals;

TEST_CASE("the properties panel reads with getprop and writes with setprop",
          "[cap:schematic-editing][req:Property editing]") {
  PropertiesPanel panel;
  CHECK_FALSE(panel.load_command().has_value());
  CHECK(panel.title().empty());

  panel.show_instance("XM2");
  CHECK(panel.title() == "XM2");
  CHECK(panel.load_command() == "schematic getprop instance XM2");
  panel.set_properties("name=XM2 W=2 L=0.15 model=\"pfet 01v8\" spice_ignore");
  REQUIRE(panel.rows().size() == 5);
  CHECK(panel.rows()[1].key == "W");
  CHECK(panel.rows()[1].value == "2");
  CHECK(panel.rows()[3].value == "pfet 01v8");  // decoded
  CHECK_FALSE(panel.rows()[4].has_value);
  CHECK(panel.set_command("W", "2.5") == "schematic setprop instance XM2 W 2.5");
  CHECK(panel.set_command("model", "a b") == "schematic setprop instance XM2 model {a b}");
  CHECK(panel.set_command("value", "") == "schematic setprop instance XM2 value {}");
  CHECK(panel.remove_command("W") == "schematic setprop instance XM2 W");
  CHECK_FALSE(panel.set_command("", "x").has_value());

  panel.show_wire(3);
  CHECK(panel.title() == "Wire 3");
  CHECK(panel.rows().empty());
  CHECK(panel.load_command() == "schematic getprop wire 3");
  CHECK(panel.set_command("lab", "OUT") == "schematic setprop wire 3 lab OUT");

  panel.show_schematic();
  CHECK(panel.load_command() == "schematic getprop schematic K");
  CHECK_FALSE(panel.set_command("k", "v").has_value());  // the global records are edited as text
  panel.clear();
  CHECK(panel.target() == PropertyTarget::none);
}

namespace {

// Runs a panel line ("schematic <option> ...") through the session's `schematic`.
hefesto::cmd::CommandResult run_panel_line(hefesto::test::SchemSession& s, const std::string& line,
                                     hefesto::cmd::Origin origin = hefesto::cmd::Origin::gui) {
  const auto words = hefesto::pcell::split_tcl_list(line);
  REQUIRE(words.ok());
  REQUIRE(!words.value().empty());
  REQUIRE(words.value().front() == "schematic");
  return s.schematic({words.value().begin() + 1, words.value().end()}, origin);
}

}  // namespace

// schematic-editing "Property editing", paragraph "Field view" (M8 round 8).
TEST_CASE("the field view shows one row per attribute and runs one setprop line per change",
          "[cap:schematic-editing][req:Property editing]") {
  PropertiesPanel panel;
  CHECK_FALSE(panel.editable());
  CHECK_FALSE(panel.field_command("value", "2k", false).has_value());  // nothing shown

  panel.show_instance("R1");
  CHECK(panel.editable());
  // An attribute given twice shows once, at its first place, with the value
  // getprop returns (the first one).
  panel.set_properties("name=R1 value=1k m=2 value=3k");
  REQUIRE(panel.rows().size() == 3);
  CHECK(panel.rows()[1].key == "value");
  CHECK(panel.rows()[1].value == "1k");
  CHECK(panel.rows()[2].key == "m");

  CHECK(panel.field_command("value", "2k", false) == "schematic setprop instance R1 value 2k");
  CHECK(panel.field_command("value", "", false) == "schematic setprop instance R1 value");  // cleared: removed
  CHECK(panel.field_command("footprint", "0603 smd", false) == "schematic setprop instance R1 footprint {0603 smd}");
  // "Apply to all selected".
  CHECK(panel.field_command("value", "10k", true) == "schematic setprop selection value 10k");
  CHECK(panel.field_command("value", "", true) == "schematic setprop selection value");
  // Keys the property string cannot hold.
  for (const char* key : {"", "a b", "a=b", "a\"b", "a\\b"}) {
    INFO(key);
    CHECK_FALSE(panel.field_command(key, "1", false).has_value());
  }

  // The `name` row renames the instance shown; the panel follows its new name.
  CHECK(panel.renames("name", "R7", false));
  CHECK_FALSE(panel.renames("name", "R7", true));
  CHECK_FALSE(panel.renames("name", "", false));
  CHECK_FALSE(panel.renames("value", "R7", false));
  panel.follow_rename("R7");
  CHECK(panel.title() == "R7");
  CHECK(panel.rows().empty());  // read again
  CHECK(panel.load_command() == "schematic getprop instance R7");

  panel.show_wire(4);
  CHECK(panel.field_command("lab", "OUT", false) == "schematic setprop wire 4 lab OUT");
  CHECK_FALSE(panel.renames("name", "x", false));
  panel.show_schematic();  // the global records: past M8 (Q-SCHEM-68, approved 2026-11-28)
  CHECK_FALSE(panel.editable());
  CHECK_FALSE(panel.field_command("k", "v", false).has_value());
}

// Scenario "Field view edits through setprop": the panel's lines run through
// the real `schematic` command, in both modes of xschem-compat.
TEST_CASE("Field view edits through setprop", "[cap:schematic-editing][req:Property editing]") {
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << on);
    hefesto::test::SchemSession s;
    s.project().compat.xschem_compat = on;
    s.folder().write("r.sch",
                     "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {}\nV {}\nS {}\nE {}\n"
                     "C {res.sym} 0 0 0 0 {name=R1 value=1k footprint=\"0603 smd\"}\n");
    REQUIRE(s.schematic({"open", s.folder().str("r.sch")}).ok());

    PropertiesPanel panel;
    panel.show_instance("R1");
    const auto read = [&] {
      const auto load = panel.load_command();
      REQUIRE(load.has_value());
      const auto result = run_panel_line(s, *load, hefesto::cmd::Origin::internal);
      REQUIRE(result.ok());
      panel.set_properties(result.value.to_string());
    };
    const auto change = [&](const std::string& key, const std::string& value) {
      const auto line = panel.field_command(key, value, false);
      REQUIRE(line.has_value());
      REQUIRE(run_panel_line(s, *line).ok());
      read();  // refreshed after the command
      return *line;
    };

    read();
    REQUIRE(panel.rows().size() == 3);
    CHECK(panel.rows()[0].key == "name");
    CHECK(panel.rows()[0].value == "R1");
    CHECK(panel.rows()[1].key == "value");
    CHECK(panel.rows()[1].value == "1k");
    CHECK(panel.rows()[2].key == "footprint");
    CHECK(panel.rows()[2].value == "0603 smd");

    CHECK(change("value", "2k") == "schematic setprop instance R1 value 2k");
    CHECK(change("footprint", "") == "schematic setprop instance R1 footprint");
    // schematic-file-format "Property strings", setting rule 5: removing the
    // last attribute keeps the separator before it (the scenario text omits
    // that blank: reported to the spec team).
    CHECK(hefesto::test::active(s).instances[0].props == "name=R1 value=2k ");
    REQUIRE(panel.rows().size() == 2);
    CHECK(panel.rows()[1].value == "2k");

    // Logged, one line per change.
    const std::vector<std::string> history = s.log().history();
    CHECK(std::count(history.begin(), history.end(), "schematic setprop instance R1 value 2k") == 1);
    CHECK(std::count(history.begin(), history.end(), "schematic setprop instance R1 footprint") == 1);
    // Two undo steps.
    CHECK(hefesto::test::run(s, {"undo"}) == "1");
    CHECK(hefesto::test::run(s, {"undo"}) == "1");
    CHECK(hefesto::test::active(s).instances[0].props == "name=R1 value=1k footprint=\"0603 smd\"");
    CHECK(hefesto::test::run(s, {"undo"}) == "0");
  }
}

TEST_CASE("the hierarchy breadcrumb follows the hierarchy path", "[cap:schematic-editing][req:Hierarchy path]") {
  HierarchyBreadcrumb crumbs;
  crumbs.set_top("tb_inverter.sch");
  crumbs.set_path(".");
  CHECK(crumbs.depth() == 0);
  CHECK(crumbs.text() == "tb_inverter.sch");
  CHECK(crumbs.click_commands(0).empty());

  crumbs.set_path(".x1.xinv[2].");
  REQUIRE(crumbs.depth() == 2);
  const auto all = crumbs.crumbs();
  REQUIRE(all.size() == 3);
  CHECK(all[1].label == "x1");
  CHECK(all[2].label == "xinv[2]");
  CHECK(crumbs.text() == "tb_inverter.sch › x1 › xinv[2]");
  // Dots inside brackets stay in the element.
  crumbs.set_path(".x[a.b].");
  CHECK(crumbs.crumbs().back().label == "x[a.b]");
}

TEST_CASE("descend and ascend through the breadcrumb", "[cap:schematic-editing][req:Hierarchy navigation]") {
  HierarchyBreadcrumb crumbs;
  crumbs.set_top("top.sch");
  crumbs.set_path(".x1.x2.");
  CHECK(HierarchyBreadcrumb::path_command() == "schematic hierarchy path");
  CHECK(HierarchyBreadcrumb::descend_command("xinv[3:0]") == "schematic descend {xinv[3:0]}");
  CHECK(HierarchyBreadcrumb::ascend_command() == "schematic ascend");
  CHECK(crumbs.click_commands(0) == std::vector<std::string>{"schematic ascend", "schematic ascend"});
  CHECK(crumbs.click_commands(1) == std::vector<std::string>{"schematic ascend"});
  CHECK(crumbs.click_commands(2).empty());
  CHECK(crumbs.click_commands(9).empty());
}

TEST_CASE("net highlighting commands and the highlighted set", "[cap:schematic-editing][req:Net highlighting]") {
  CHECK(NetHighlight::net_command("IN") == "schematic hilight net IN");
  CHECK(NetHighlight::net_command("data[3:0]") == "schematic hilight net {data[3:0]}");
  CHECK(NetHighlight::instance_command("x1") == "schematic hilight instance x1");
  CHECK(NetHighlight::selection_command() == "schematic hilight selection");
  CHECK(NetHighlight::clear_command(false) == "schematic hilight clear");
  CHECK(NetHighlight::clear_command(true) == "schematic hilight clear all");
  CHECK(NetHighlight::list_command() == "schematic hilight list");
  NetHighlight lit;
  lit.set_list("IN OUT {a b}");
  CHECK(lit.nets() == std::set<std::string>{"IN", "OUT", "a b"});
  lit.set_list("");
  CHECK(lit.nets().empty());
}

// Regression (round 5): `schematic hilight list` returns `{net <name>
// <colour>}` and `{instance <name> <colour>}` triples; the panel took each
// triple for one net name, so no wire was ever drawn highlighted.
TEST_CASE("the highlighted set is read from hilight list's triples", "[cap:schematic-editing][req:Net highlighting]") {
  NetHighlight lit;
  lit.set_list("{net OUT 0} {net {data[3:0]} 2} {instance R1 1}");
  CHECK(lit.nets() == std::set<std::string>{"OUT", "data[3:0]"});
  CHECK(lit.instances() == std::set<std::string>{"R1"});
  CHECK(lit.colour("OUT", false) == 0);
  CHECK(lit.colour("data[3:0]", false) == 2);
  CHECK(lit.colour("R1", true) == 1);
  CHECK_FALSE(lit.colour("R1", false).has_value());
  lit.set_list("{net}");  // not a triple: a net name
  CHECK(lit.nets() == std::set<std::string>{"net"});
  lit.set_list("{unbalanced");  // not a list
  CHECK(lit.nets().empty());
  CHECK(lit.instances().empty());
}

TEST_CASE("a command result becomes the panels' outcome", "[cap:schematic-command][req:Return values and errors in scripts]") {
  hefesto::cmd::CommandResult failed;
  failed.status = hefesto::cmd::Status::failed;
  failed.diagnostics.push_back(hefesto::diag::Diagnostic{hefesto::diag::Severity::warning, "SCHEM-NET-002", "w", {}, {}});
  failed.diagnostics.push_back(hefesto::diag::Diagnostic{hefesto::diag::Severity::error, "SCHEM-NET-004", "short", {}, {}});
  failed.diagnostics.push_back(hefesto::diag::Diagnostic{hefesto::diag::Severity::debug, "X-Y-1", "d", {}, {}});
  const CommandOutcome out = outcome_of(failed);
  CHECK_FALSE(out.ok());
  CHECK(out.message == "short");
  CHECK(out.diagnostics == std::vector<std::string>{"warning: SCHEM-NET-002: w", "error: SCHEM-NET-004: short"});
  MessagesPanel messages;
  messages.add(out);
  CHECK(messages.summary() == "1 error · 1 warning");

  hefesto::cmd::CommandResult ok;
  ok.value = hefesto::cmd::Value{"R1 a b 1k"};
  CHECK(outcome_of(ok).ok());
  CHECK(outcome_of(ok).output == "R1 a b 1k");
}

TEST_CASE("the messages panel keeps each command's diagnostics",
          "[cap:schematic-command][req:Return values and errors in scripts]") {
  const Message m = parse_message("error: SCHEM-NET-004: net OUT shorted to IN");
  CHECK(m.severity == MessageSeverity::error);
  CHECK(m.code == "SCHEM-NET-004");
  CHECK(m.text == "net OUT shorted to IN");
  CHECK(parse_message("warning: symbol x.sym not found").code.empty());
  CHECK(parse_message("plain text").text == "plain text");
  CHECK(parse_message("plain text").severity == MessageSeverity::info);

  MessagesPanel panel;
  CHECK(panel.summary() == "No messages");
  CommandOutcome failed;
  failed.status = OutcomeStatus::failed;
  failed.message = "schematic: unsaved changes";
  panel.add(failed);
  CommandOutcome warned;
  warned.diagnostics = {"warning: SCHEM-LIB-001: found through .", "warning: SCHEM-LIB-001: again"};
  panel.add(warned);
  CHECK(panel.messages().size() == 3);
  CHECK(panel.count(MessageSeverity::error) == 1);
  CHECK(panel.summary() == "1 error · 2 warnings");
  CommandOutcome many;
  many.diagnostics.assign(MessagesPanel::kMaxMessages + 5, "info: x");
  panel.add(many);
  CHECK(panel.messages().size() == MessagesPanel::kMaxMessages);
  panel.clear();
  CHECK(panel.messages().empty());
}

// Regression: the card's Interrupt only cancelled the console interpreter,
// so the netlist run (which polls the editor's pending Interrupt) went on
// and wrote its file.
TEST_CASE("the netlist Interrupt sets the editor's pending Interrupt the run polls",
          "[cap:schematic-command][req:Subcommand table]") {
  hefesto::editor::EditorState editor;
  NetlistAction action;
  CHECK_FALSE(interrupt_schematic(action, editor));  // no run: the ordinary Interrupt
  CHECK(hefesto::ui::state(editor).interrupt_pending.exchange(false));

  (void)action.start({}, "inv.sch", NetlistAction::Clock::now());
  CHECK(interrupt_schematic(action, editor));
  CHECK(action.cancel_requested());
  CHECK(hefesto::ui::state(editor).interrupt_pending.load());
  hefesto::ui::state(editor).interrupt_pending = false;
  CHECK_FALSE(interrupt_schematic(action, editor));  // sent once
  CHECK_FALSE(hefesto::ui::state(editor).interrupt_pending.load());
}

TEST_CASE("the netlist action: command, Interrupt card and result", "[cap:schematic-command][req:Subcommand table]") {
  CHECK(netlist_command({}) == "schematic netlist -hier");
  CHECK(netlist_command({"out dir/inv.spice", NetlistMode::flat, true}) ==
        "schematic netlist {out dir/inv.spice} -flat -lvs");
  CHECK(netlist_command({"", NetlistMode::top, false}) == "schematic netlist -top");

  NetlistAction action;
  MessagesPanel messages;
  const auto t0 = NetlistAction::Clock::time_point{} + 100s;
  const auto line = action.start({"inv.spice", NetlistMode::hier, false}, "inv.sch", t0);
  CHECK(line == "schematic netlist inv.spice -hier");
  CHECK_FALSE(action.enabled());
  CHECK_FALSE(action.start({}, "inv.sch", t0).has_value());  // one run at a time
  CHECK_FALSE(action.card(t0 + 1s).has_value());               // before kLongRunning
  const auto card = action.card(t0 + 3s);
  REQUIRE(card.has_value());
  CHECK(card->title == "Netlisting inv.sch · 3 s");
  CHECK(card->detail == "schematic netlist inv.spice -hier");
  CHECK(card->action == "Interrupt");
  CHECK(action.request_cancel());        // the window issues the script interrupt
  CHECK_FALSE(action.request_cancel());  // once: a second press sends nothing
  CHECK(action.cancel_requested());
  CHECK(action.card(t0 + 4s)->detail == "Stopping: no file is written");

  CommandOutcome ok;
  ok.output = "/work/inv.spice";
  action.finish(ok, messages);
  CHECK(action.state() == NetlistAction::State::done);
  CHECK(action.status() == "Netlist written: inv.spice");
  CHECK_FALSE(action.card(t0 + 9s).has_value());
  CHECK(action.enabled());

  REQUIRE(action.start({}, "inv.sch", t0).has_value());
  CommandOutcome text;
  text.output = "** sch_path: /work/inv.sch\nR1 a b 1k\n.end\n";
  action.finish(text, messages);
  CHECK(action.status() == "Netlist ready (3 lines)");
  CHECK(action.text() == text.output);

  REQUIRE(action.start({}, "inv.sch", t0).has_value());
  CommandOutcome bad;
  bad.status = OutcomeStatus::failed;
  bad.message = "netlist failed";
  bad.diagnostics = {"error: SCHEM-CMD-005: netlist failed"};
  action.finish(bad, messages);
  CHECK(action.state() == NetlistAction::State::failed);
  CHECK(action.status() == "Netlist failed: netlist failed");
  CHECK(messages.count(MessageSeverity::error) == 1);
  action.finish(bad, messages);  // not running: ignored
  CHECK(messages.count(MessageSeverity::error) == 1);
}

// Regression (round 8 capture review, CI run 46155486775): during a netlist
// run the frames the desktop's progress handler drew never repainted the
// netlist bar, so its Interrupt card was never shown however long the run.
TEST_CASE("the frames drawn during a netlist run repaint the netlist bar",
          "[cap:schematic-command][req:Subcommand table]") {
  hefesto::script::ProgressReporting progress;
  std::vector<std::string> calls;
  progress.handler = [&calls](const hefesto::script::ScriptProgress& p) { calls.emplace_back(p.done ? "done" : "frame"); };
  {
    const NetlistProgressRepaint repaint(progress, [&calls] { calls.emplace_back("repaint"); });
    progress.handler(hefesto::script::ScriptProgress{});
    hefesto::script::ScriptProgress last;
    last.done = true;
    progress.handler(last);
  }
  CHECK(calls == std::vector<std::string>{"repaint", "frame", "done"});
  // Restored after the run.
  calls.clear();
  progress.handler(hefesto::script::ScriptProgress{});
  CHECK(calls == std::vector<std::string>{"frame"});

  // No front end drawing frames: nothing is installed.
  hefesto::script::ProgressReporting none;
  {
    const NetlistProgressRepaint repaint(none, [&calls] { calls.emplace_back("repaint"); });
    CHECK_FALSE(static_cast<bool>(none.handler));
  }
  CHECK_FALSE(static_cast<bool>(none.handler));
}

TEST_CASE("the top bar's Netlist button runs the same netlist action as the netlist bar",
          "[cap:schematic-command][req:Subcommand table]") {
  NetlistAction action;
  MessagesPanel messages;

  const NetlistButtonView none = netlist_button(action, false);
  CHECK(none.label == "Netlist");
  CHECK_FALSE(none.enabled);
  CHECK(none.tooltip.find("no schematic open") != std::string::npos);
  CHECK(netlist_button(action, true).enabled);

  // No document: nothing runs, a message says why.
  std::vector<std::string> issued;
  const auto sink = [&issued](const std::string& line) { issued.push_back(line); };
  CHECK_FALSE(run_netlist(action, false, "inv.sch", sink, messages));
  CHECK(issued.empty());
  REQUIRE(messages.messages().size() == 1);
  CHECK(messages.messages().back().text == "netlist: no schematic open");

  // The sink runs the line; the view's observer finishes the action with the result.
  const auto observed = [&](const std::string& line) {
    issued.push_back(line);
    CHECK_FALSE(netlist_button(action, true).enabled);  // disabled while it runs
    CommandOutcome ok;
    ok.output = "R1 a b 1k\n.end\n";
    action.finish(ok, messages);
  };
  CHECK(run_netlist(action, true, "inv.sch", observed, messages));
  CHECK(issued == std::vector<std::string>{"schematic netlist -hier"});
  CHECK(action.state() == NetlistAction::State::done);
  CHECK(action.status() == "Netlist ready (2 lines)");

  // A line that never reached the dispatcher ends the run as failed.
  CHECK(run_netlist(action, true, "inv.sch", sink, messages));
  CHECK(action.state() == NetlistAction::State::failed);
  CHECK(action.status() == "Netlist failed: the netlist command did not run");
  CHECK(netlist_button(action, true).enabled);

  // One run at a time: a second start while running issues nothing.
  const auto nested = [&](const std::string& line) {
    issued.push_back(line);
    CHECK_FALSE(run_netlist(action, true, "inv.sch", sink, messages));
  };
  issued.clear();
  CHECK(run_netlist(action, true, "inv.sch", nested, messages));
  CHECK(issued.size() == 1);
}

// M8 round 7: the `tab` command, a second way to the Schematic tab (the view
// tabs give way in a narrow top bar, so the 1024 px macOS CI screen could
// not reach it).
TEST_CASE("the tab command names the editor view to show", "[cap:schematic-editing][req:Desktop and touch interaction]") {
  const TabRequest query = parse_tab_command({});
  CHECK(query.ok);
  CHECK_FALSE(query.show.has_value());
  for (const EditorTab tab : {EditorTab::layout, EditorTab::schematic, EditorTab::view3d}) {
    const TabRequest r = parse_tab_command({std::string(editor_tab_name(tab))});
    CHECK(r.ok);
    REQUIRE(r.show.has_value());
    CHECK(*r.show == tab);
  }
  CHECK(editor_tab_name(EditorTab::view3d) == "3d");
  CHECK_FALSE(parse_tab_command({"waveforms"}).ok);  // arrives in M9
  CHECK_FALSE(parse_tab_command({"Schematic"}).ok);
  CHECK_FALSE(parse_tab_command({"schematic", "extra"}).ok);
}
