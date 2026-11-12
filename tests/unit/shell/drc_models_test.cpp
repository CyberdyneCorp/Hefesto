// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The toolkit-free view models of the M3 DRC screens (boards M3-DRC-Status
// and DRC-Browser): the chip and status-bar states, the chip menu, stepping,
// the results browser's virtual rows, the inspector, the `drc why` popover
// and the job-completion notice.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>
#include <vector>

#include "hefesto/shell/drc_models.hpp"

using namespace hefesto;
using namespace hefesto::shell;

namespace {

DrcStatusInput idle_input(std::size_t errors) {
  DrcStatusInput in;
  in.cell = "ota_5t";
  in.has_rules = true;
  in.checked = true;
  in.error_tiles = errors;
  in.style = "drc(full)";
  return in;
}

DrcGroup group(std::string text, std::size_t areas, bool ignored = false) {
  DrcGroup g;
  g.text = std::move(text);
  for (std::size_t i = 0; i < areas; ++i) {
    const auto x = static_cast<db::Coord>(10 * i);
    g.areas.push_back(db::Rect{x, 0, x + 5, 5});
  }
  g.ignored = ignored;
  return g;
}

UnitScale lambda_scale() {
  return UnitScale{1, 1, std::nullopt};
}

}  // namespace

TEST_CASE("the DRC chip shows each state with a shape and a word", "[cap:hefesto-app-ux][req:Main window layout]") {
  SECTION("N errors: the root's error tiles, grouped, in the singular for one") {
    const DrcStatusView v = drc_status_view(idle_input(12408));
    CHECK(v.state == DrcState::errors);
    CHECK(v.chip.label() == "■ 12,408 DRC errors");
    CHECK(v.status_bar.label() == "■ DRC 12,408 errors · drc(full)");
    CHECK(v.accessible_name == "DRC: 12,408 errors, style drc(full). Open the DRC panel");
    CHECK(v.tooltip == "12,408 error tiles in ota_5t · style drc(full) · ! next error");
    CHECK(drc_status_view(idle_input(1)).chip.label() == "■ 1 DRC error");
  }
  SECTION("clean") {
    const DrcStatusView v = drc_status_view(idle_input(0));
    CHECK(v.state == DrcState::clean);
    CHECK(v.chip.label() == "● DRC clean");
    CHECK(v.status_bar.label() == "● DRC 0 errors · drc(full)");
  }
  SECTION("checking: no count while busy") {
    DrcStatusInput in = idle_input(3);
    in.busy = true;
    const DrcStatusView v = drc_status_view(in);
    CHECK(v.state == DrcState::checking);
    CHECK(v.chip.label() == "◆ DRC checking…");
    CHECK(v.status_bar.label() == "◆ DRC checking… · drc(full)");
  }
  SECTION("not run: no cell, no rules, or not checked since opened") {
    DrcStatusInput in = idle_input(0);
    in.checked = false;
    CHECK(drc_status_view(in).chip.label() == "◆ DRC not run");
    in = idle_input(0);
    in.cell.clear();
    CHECK(drc_state(in) == DrcState::not_run);
    in = idle_input(0);
    in.has_rules = false;
    CHECK(drc_state(in) == DrcState::not_run);
  }
  SECTION("off: the last count stays in the status bar") {
    DrcStatusInput in = idle_input(12);
    in.background_on = false;
    const DrcStatusView v = drc_status_view(in);
    CHECK(v.chip.label() == "◆ DRC off");
    CHECK(v.status_bar.label() == "◆ DRC off · 12 errors, last check");
    // Magic's paint marks nothing under `drc off` (CmdPaint checks DRCBackGround),
    // so the tooltip must not promise that edits are marked.
    CHECK(v.tooltip ==
          "Background DRC is off (drc off): painted areas are not marked for checking; Check now runs drc check; "
          "drc catchup");
  }
  SECTION("precedence off > checking > not run > N errors > clean") {
    DrcStatusInput in = idle_input(5);
    in.busy = true;
    in.checked = false;
    in.background_on = false;
    CHECK(drc_state(in) == DrcState::off);
    in.background_on = true;
    CHECK(drc_state(in) == DrcState::checking);
    in.busy = false;
    CHECK(drc_state(in) == DrcState::not_run);
    in.checked = true;
    CHECK(drc_state(in) == DrcState::errors);
  }
  SECTION("every state carries a glyph (never color only) and an announcement") {
    for (const bool off : {false, true}) {
      for (const bool busy : {false, true}) {
        for (const std::size_t n : {std::size_t{0}, std::size_t{7}}) {
          DrcStatusInput in = idle_input(n);
          in.background_on = !off;
          in.busy = busy;
          const DrcStatusView v = drc_status_view(in);
          CHECK(v.chip.label().substr(0, v.chip.label().find(' ')) == status_glyph(v.chip.kind));
          CHECK_FALSE(v.announcement.empty());
          CHECK(v.accessible_name.starts_with("DRC: "));
        }
      }
    }
  }
}

TEST_CASE("the chip menu issues the DRC commands of Magic's Drc menu", "[cap:hefesto-app-ux][req:Main window layout]") {
  DrcMenuInput in{true, false, true, {"drc(fast)", "drc(full)"}, "drc(full)"};
  const auto menu = drc_chip_menu(in);
  REQUIRE(menu.size() == 7);
  CHECK(menu[0].command == "panel drc show");
  CHECK(menu[1].command == "select top cell; drc check; drc catchup");
  CHECK(menu[2].command == "drc find; findbox zoom");
  CHECK(menu[3].command == "drc why");
  CHECK(menu[4].command == "drc off");
  CHECK(menu[4].checked);
  CHECK(menu[5].command == "drc style drc(fast)");
  CHECK_FALSE(menu[5].checked);
  CHECK(menu[6].checked);
  in.background_on = false;
  in.busy = true;
  const auto off = drc_chip_menu(in);
  CHECK(off[4].command == "drc on");
  CHECK_FALSE(off[1].enabled);  // no Check now while checking
}

TEST_CASE("Prev and Next issue drc find with an explicit index, wrapping",
          "[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]") {
  DrcStepper s{std::nullopt, 12};
  CHECK(s.label() == "– of 12");
  // Q-DRC-15: the first Next goes to #1 in both magic-compat settings.
  CHECK(s.next_command() == "drc find 1; findbox zoom");
  CHECK(s.prev_command() == "drc find 12; findbox zoom");
  s.k = 3;
  CHECK(s.label() == "#3 of 12");
  CHECK(s.next_command() == "drc find 4; findbox zoom");
  CHECK(s.prev_command() == "drc find 2; findbox zoom");
  s.k = 12;
  CHECK(s.next_command() == "drc find 1; findbox zoom");
  s.k = 1;
  CHECK(s.prev_command() == "drc find 12; findbox zoom");
  CHECK_FALSE(DrcStepper{std::nullopt, 0}.enabled());
  CHECK(DrcStepper{std::nullopt, 0}.label() == "0 errors");
}

TEST_CASE("the last drc find's area and why-texts are read from its output",
          "[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]") {
  const auto r = parse_find_report({"Error area #1:", "a", "Error area #3:", "Metal1 spacing < 0.14um (met1.2)", "b"});
  REQUIRE(r);
  CHECK(r->index == 3);
  CHECK(r->whys == std::vector<std::string>{"Metal1 spacing < 0.14um (met1.2)", "b"});
  CHECK_FALSE(parse_find_report({"There are no errors in top."}));
  CHECK(is_drc_option("drc", {"find", "3"}, "find"));
  CHECK(is_drc_option("drc", {"fi"}, "find"));
  CHECK_FALSE(is_drc_option("drc", {"listall", "why"}, "why"));
  CHECK(is_drc_option("drc", {"listall", "why"}, "why", true));
  CHECK_FALSE(is_drc_option("box", {"find"}, "find"));
}

TEST_CASE("browser actions are drc commands with quoted why-texts",
          "[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]") {
  CHECK(command_word("met1.2") == "met1.2");
  CHECK(command_word("Metal1 spacing < 0.14um") == "{Metal1 spacing < 0.14um}");
  CHECK(command_word("a}b") == "\"a}b\"");
  CHECK(drc_ignore_command("met1.2") == "drc ignore met1.2");
  CHECK(drc_ignore_command("poly width") == "drc ignore {poly width}");
  CHECK(drc_style_command("drc(full)") == "drc style drc(full)");
  CHECK(drc_rule_tag("Metal1 spacing < 0.14um (met1.2)") == "met1.2");
  CHECK(drc_rule_tag("no tag") == "no tag");
  CHECK(group_thousands(200000) == "200,000");
  CHECK(drc_style_menu({"a", "b"}, "b")[1].checked);
  CHECK(drc_style_menu({"a", "b"}, "b")[0].command == "drc style a");
}

TEST_CASE("the browser groups errors by why-text with counts, ignored rules apart",
          "[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]") {
  DrcBrowserModel m;
  m.set_units(lambda_scale());
  m.set_list({group("Local interconnect spacing < 0.17um (li.3)", 6), group("Metal1 spacing < 0.14um (met1.2)", 9),
              group("poly width < 0.15um (poly.1a)", 2, true)});
  // Default sort: by count, descending (Magic's order is unspecified).
  REQUIRE(m.row_count() == 4);
  CHECK(m.cell_text(0, 0) == "▸ Metal1 spacing < 0.14um (met1.2)");
  CHECK(m.cell_text(0, 1) == "■ 9");
  CHECK(m.cell_text(1, 0) == "▸ Local interconnect spacing < 0.17um (li.3)");
  CHECK(m.row(2).kind == DrcRowKind::ignored_heading);
  CHECK(m.cell_text(2, 0) == "IGNORED · 1 · Clear ignores ⏎");
  CHECK(m.cell_text(3, 0) == "⊘ poly width < 0.15um (poly.1a)");
  CHECK(m.cell_text(3, 1) == "◆ 2");
  CHECK(m.summary(12) == "■ 12 error tiles · 15 areas · 2 why-texts · 1 ignored");
  CHECK(m.row_accessible_name(0) == "Metal1 spacing < 0.14um (met1.2), 9 areas, collapsed");

  SECTION("a group expands to its areas in discovery order; an area moves the box and zooms") {
    CHECK_FALSE(m.command(0));
    REQUIRE(m.toggle(0));
    CHECK(m.row_count() == 13);
    CHECK(m.cell_text(0, 0).starts_with("▾ "));
    CHECK(m.cell_text(3, 0) == "    3   20 0 25 5");
    CHECK(m.command(3) == "box values 20i 0i 25i 5i; findbox zoom");
    CHECK(m.row_at(db::Rect{21, 1, 22, 2}) == std::size_t{3});
    CHECK(m.command(11) == "drc ignore none");  // the Ignored heading clears the ignores
    // A new list keeps the expansion of the same why-text.
    m.set_list({group("Metal1 spacing < 0.14um (met1.2)", 2)});
    CHECK(m.row_count() == 3);
  }
  SECTION("sort by text, either way; filter by text") {
    m.sort(DrcSortKey::text, false);
    CHECK(m.cell_text(0, 0) == "▸ Local interconnect spacing < 0.17um (li.3)");
    m.sort(DrcSortKey::count, false);
    CHECK(m.cell_text(0, 0) == "▸ Local interconnect spacing < 0.17um (li.3)");
    m.set_filter("METAL");
    CHECK(m.row_count() == 1);
  }
}

TEST_CASE("the found error highlights its group row while the group is collapsed",
          "[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]") {
  // DRC-Browser board note 6: the rows overlapping the box are highlighted;
  // a collapsed group stands for its areas.
  DrcBrowserModel m;
  m.set_units(lambda_scale());
  DrcGroup spacing{"Metal1 spacing < 0.14um (met1.2)", {{0, 0, 5, 5}, {100, 0, 105, 5}, {200, 0, 205, 5}}, false};
  DrcGroup width{"Metal1 width < 0.14um (met1.1)", {{300, 0, 305, 5}}, false};
  DrcGroup poly{"poly width < 0.15um (poly.1a)", {{400, 0, 405, 5}}, true};
  m.set_list({spacing, width, poly});
  REQUIRE(m.row_count() == 4);  // spacing, width, Ignored heading, poly
  CHECK(m.row_at(db::Rect{101, 1, 102, 2}) == std::size_t{0});  // collapsed spacing group
  CHECK(m.row_at(db::Rect{301, 1, 302, 2}) == std::size_t{1});  // collapsed width group
  CHECK(m.row_at(db::Rect{401, 1, 402, 2}) == std::size_t{3});  // ignored groups never expand
  CHECK_FALSE(m.row_at(db::Rect{50, 50, 60, 60}));
  // Expanded, the area row is highlighted, not the group row.
  REQUIRE(m.toggle(0));
  CHECK(m.row_at(db::Rect{101, 1, 102, 2}) == std::size_t{2});
  CHECK(m.row_at(db::Rect{301, 1, 302, 2}) == std::size_t{4});
}

TEST_CASE("the canvas tag shows #k and the why-text while the box stays on the found error",
          "[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]") {
  const DrcFindReport found{3, {"Metal1 spacing < 0.14um (met1.2)", "Metal1 width < 0.14um (met1.1)"}};
  const db::Rect area{10, 20, 30, 40};
  const auto tag = drc_canvas_tag(found, area, area);
  REQUIRE(tag);
  CHECK(tag->index == "#3");
  CHECK(tag->why == "Metal1 spacing < 0.14um (met1.2)");  // the first why-text, as on the board
  CHECK(tag->area == area);
  // Gone once the box moves, before any find, or without a box.
  CHECK_FALSE(drc_canvas_tag(found, area, db::Rect{0, 0, 1, 1}));
  CHECK_FALSE(drc_canvas_tag(std::nullopt, area, area));
  CHECK_FALSE(drc_canvas_tag(found, area, std::nullopt));
  CHECK_FALSE(drc_canvas_tag(found, std::nullopt, area));
  // A find that printed no why-text still tags the area with its index.
  const auto bare = drc_canvas_tag(DrcFindReport{1, {}}, area, area);
  REQUIRE(bare);
  CHECK(bare->index == "#1");
  CHECK(bare->why.empty());
}

TEST_CASE("200,000 errors are virtual rows of the DRC browser",
          "[cap:hefesto-app-ux][req:Desktop platform integration through Raven]") {
  DrcBrowserModel m;
  m.set_units(lambda_scale());
  std::vector<DrcGroup> groups;
  groups.push_back(group("Metal1 spacing < 0.14um (met1.2)", 150000));
  groups.push_back(group("li.3", 50000));
  m.set_list(std::move(groups));
  CHECK(m.area_count() == 200000);
  const auto start = std::chrono::steady_clock::now();
  REQUIRE(m.toggle(0));
  REQUIRE(m.toggle(m.row_count() - 1));
  CHECK(m.row_count() == 200002);
  // Any row is answered on demand (no widget per row); the table asks only for visible ones.
  CHECK(m.cell_text(200001, 0).starts_with("    50000   "));
  m.sort(DrcSortKey::count, false);
  CHECK(m.cell_text(0, 0) == "▾ li.3");
  const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  CHECK(ms < 2000.0);
}

TEST_CASE("the browser state: running, not checked, stale, clean, populated",
          "[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]") {
  DrcBrowserModel m;
  CHECK(drc_browser_state(DrcState::not_run, m, false) == DrcBrowserState::not_checked);
  CHECK(m.summary(0) == "◆ 0 error tiles");  // not checked: never the clean shape
  CHECK(drc_browser_state(DrcState::checking, m, false) == DrcBrowserState::running);
  m.set_list({group("poly", 1, true)});
  CHECK(drc_browser_state(DrcState::clean, m, false) == DrcBrowserState::clean);  // ignored only
  m.set_list({group("met1", 1)});
  CHECK(drc_browser_state(DrcState::errors, m, false) == DrcBrowserState::populated);
  CHECK(drc_browser_state(DrcState::errors, m, true) == DrcBrowserState::stale);
  CHECK(drc_browser_state(DrcState::checking, m, true) == DrcBrowserState::running);
  CHECK(drc_browser_notice(DrcBrowserState::stale, "top", "s")->kind == StatusKind::warning);
  CHECK(drc_browser_notice(DrcBrowserState::clean, "top", "s")->title == "No DRC errors in top");
  CHECK_FALSE(drc_browser_notice(DrcBrowserState::populated, "top", "s"));
  CHECK(recheck_label(DrcBrowserState::not_checked) == "Check");
  CHECK(recheck_label(DrcBrowserState::stale) == "Re-check");
}

TEST_CASE("the inspector shows the error area under the box", "[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]") {
  DrcInspectorInput in;
  in.error_tiles = 12;
  CHECK(drc_inspector_view(in).title == "DRC ERROR AREA");
  CHECK_FALSE(drc_inspector_view(in).empty.empty());
  in.found = DrcFindReport{3, {"Metal1 spacing < 0.14um (met1.2)", "li.3"}};
  in.box = db::Rect{10, 20, 30, 50};
  in.cell = "ota_5t";
  in.style = "drc(full)";
  in.scale = lambda_scale();
  const DrcInspectorView v = drc_inspector_view(in);
  CHECK(v.title == "DRC ERROR AREA #3 OF 12");
  CHECK(v.headline == "■ Metal1 spacing < 0.14um (met1.2)");
  CHECK(v.whys == std::vector<std::string>{"■ li.3"});
  REQUIRE(v.rows.size() == 4);
  CHECK(v.rows[1].second == "10 20 30 50");
  CHECK(v.rows[2].second == "20 × 30 lambda");
  for (const DeferredRow& d : drc_deferred_rows()) CHECK(d.tooltip.find(d.milestone) != std::string::npos);
}

TEST_CASE("drc why opens a popover with its why-texts", "[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]") {
  const auto p = drc_why_popover("drc", {"why"}, {"Metal1 spacing < 0.14um (met1.2)", "li.3"});
  REQUIRE(p);
  CHECK(p->rows.size() == 2);
  CHECK_FALSE(p->clean);
  CHECK(drc_why_popover("drc", {"why"}, {"No errors found."})->clean);
  CHECK_FALSE(drc_why_popover("drc", {"listall", "why"}, {}));
  CHECK_FALSE(drc_why_popover("drc", {"find"}, {}));
}

TEST_CASE("a long check finished while unfocused posts a notice unless turned off",
          "[cap:hefesto-app-ux][req:Desktop platform integration through Raven]") {
  DrcJobInput in{48.2, false, true, "ota_5t", 12, "drc(full)"};
  const auto n = drc_job_notice(in);
  REQUIRE(n);
  CHECK(n->title == "DRC finished · ota_5t");
  CHECK(n->body == "■ 12 errors · drc(full) · 48 s");
  in.error_tiles = 0;
  CHECK(drc_job_notice(in)->body == "● No DRC errors · drc(full) · 48 s");
  in.window_active = true;
  CHECK_FALSE(drc_job_notice(in));
  in.window_active = false;
  in.seconds = 1.0;
  CHECK_FALSE(drc_job_notice(in));  // not a long job
  in.seconds = 10.0;
  in.enabled = notifications_enabled(std::string("off"));
  CHECK_FALSE(drc_job_notice(in));
  CHECK(notifications_enabled(std::nullopt));
  CHECK(notifications_enabled(std::string("on")));
}

TEST_CASE("the inspector's Design rules nearby card shows the DRC result under the box",
          "[cap:hefesto-app-ux][req:Main window layout]") {
  // Regression (Desktop Alpha): the card still read "Arrives with DRC (M3)".
  const NearbyRulesView errors = nearby_rules_view(DrcState::errors, 3, true);
  CHECK(errors.kind == StatusKind::error);
  CHECK(errors.status == "3 DRC error tiles under the box");
  CHECK(errors.later == "Rule margins arrive with the agent (M11)");
  CHECK(nearby_rules_view(DrcState::errors, 1, false).status == "1 DRC error tile in the cell");
  const NearbyRulesView clean = nearby_rules_view(DrcState::errors, 0, true);
  CHECK(clean.kind == StatusKind::success);
  CHECK(clean.status == "No DRC errors under the box");
  CHECK(nearby_rules_view(DrcState::not_run, 0, true).status == "DRC not run · Check now");
  CHECK(nearby_rules_view(DrcState::off, 0, true).status == "DRC off · drc on to check");
  CHECK(nearby_rules_view(DrcState::checking, 0, true).kind == StatusKind::info);
  for (DrcState st : {DrcState::off, DrcState::checking, DrcState::not_run, DrcState::clean, DrcState::errors}) {
    CHECK(nearby_rules_view(st, 0, true).status.find("M3") == std::string::npos);
  }
}

TEST_CASE("magic::drcmanager create shows the DRC panel, Magic's DRC manager",
          "[cap:hefesto-app-ux][req:Main window layout]") {
  // Regression (Desktop Alpha): `magic::drcmanager create` showed nothing.
  using R = DrcManagerRequest;
  CHECK(drc_manager_request("magic::drcmanager", {"create"}, std::nullopt) == R::show);
  CHECK(drc_manager_request("drcmanager", {"create"}, std::nullopt) == R::show);
  CHECK(drc_manager_request("::magic::drcmanager", {"create"}, std::string_view{"1"}) == R::show);
  // Options › DRC Manager unchecked: `set ::Opts(drcmgr) 0 ; magic::drcmanager create` withdraws it.
  CHECK(drc_manager_request("magic::drcmanager", {"create"}, std::string_view{"0"}) == R::hide);
  CHECK(drc_manager_request("magic::drcmanager", {}, std::nullopt) == R::none);  // update
  CHECK(drc_manager_request("magic::drcmanager", {"update"}, std::nullopt) == R::none);
  CHECK(drc_manager_request("magic::cellmanager", {"create"}, std::nullopt) == R::none);
}
