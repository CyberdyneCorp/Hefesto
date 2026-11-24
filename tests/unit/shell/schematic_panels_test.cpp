// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The schematic editor's panels (schematic_panels.hpp; M8 task 13.2): the
// command lines each control issues and how the results are read back.
// Headless: the lines are checked as text (EDIT and CMD implement them).

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <set>
#include <string>
#include <vector>

#include "hefesto/shell/schematic_panels.hpp"

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
  action.request_cancel();
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
