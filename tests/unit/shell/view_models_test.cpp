// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The toolkit-free view models of the Desktop-Layout screen: what the top
// bar, layer palette, hierarchy, inspector and status bar show.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "hefesto/shell/view_models.hpp"

using namespace hefesto;
using namespace hefesto::shell;

namespace {

display::ResolvedStyle style(float r, float g, float b, float a, display::FillKind fill) {
  display::ResolvedStyle s;
  s.r = r;
  s.g = g;
  s.b = b;
  s.a = a;
  s.fill = fill;
  return s;
}

display::LayerSwatch swatch(tech::TileType type, std::string name, std::string plane,
                            std::vector<display::ResolvedStyle> styles) {
  display::LayerSwatch s;
  s.type = type;
  s.name = std::move(name);
  s.plane = std::move(plane);
  s.styles = std::move(styles);
  return s;
}

std::vector<display::LayerSwatch> palette() {
  return {swatch(9, "nwell", "well", {style(0.18F, 0.49F, 0.34F, 0.5F, display::FillKind::outline)}),
          swatch(10, "poly", "active", {style(0.88F, 0.32F, 0.31F, 1.0F, display::FillKind::stipple)}),
          swatch(11, "metal1", "metal1", {style(0.31F, 0.55F, 0.91F, 0.8F, display::FillKind::solid)}),
          swatch(12, "viali", "metal1", {style(0.79F, 0.81F, 0.84F, 1.0F, display::FillKind::cross)})};
}

db::CellUse* add_use(db::CellDef& parent, db::CellDef& child, std::string id) {
  auto use = std::make_unique<db::CellUse>();
  use->id = std::move(id);
  use->def = &child;
  use->parent = &parent;
  parent.uses().push_back(std::move(use));
  return parent.uses().back().get();
}

}  // namespace

TEST_CASE("layer rows take their colors from the PDK display styles",
          "[cap:hefesto-design-system][req:Layer colors come from the PDK, not the theme]") {
  const std::vector<LayerRow> rows = layer_rows(palette(), tech::TypeMask::all(), "");
  REQUIRE(rows.size() == 4);
  CHECK(rows[0].name == "nwell");
  CHECK(rows[0].dashed);  // outline-only style: dashed swatch, like the mockup's well
  CHECK(rows[1].stippled);
  CHECK(rows[3].crossed);  // contact cross
  // The hue is the PDK's; only the opacity is reduced for the swatch fill.
  CHECK(rows[2].outline.r == 0.31F);
  CHECK(rows[2].outline.g == 0.55F);
  CHECK(rows[2].outline.b == 0.91F);
  CHECK(rows[2].outline.a == 1.0F);
  CHECK(rows[2].fill.b == 0.91F);
  CHECK(rows[2].fill.a < 1.0F);
  const ThemeTokens& dark = theme_tokens(ThemeMode::dark);
  CHECK_FALSE((rows[2].outline.r == dark.accent.r && rows[2].outline.g == dark.accent.g));
}

TEST_CASE("layer filter matches name or plane, case-insensitively", "[cap:hefesto-app-ux][req:Main window layout]") {
  CHECK(layer_rows(palette(), tech::TypeMask::all(), "METAL").size() == 2);  // metal1 and viali (plane metal1)
  CHECK(layer_rows(palette(), tech::TypeMask::all(), "poly").size() == 1);
  CHECK(layer_rows(palette(), tech::TypeMask::all(), "zzz").empty());
}

TEST_CASE("hidden layers say so in text, not only by color",
          "[cap:hefesto-design-system][req:Status is never color-only]") {
  tech::TypeMask visible = tech::TypeMask::all();
  visible.clear_type(11);
  const std::vector<LayerRow> rows = layer_rows(palette(), visible, "metal1");
  REQUIRE_FALSE(rows.empty());
  const LayerRow& metal1 = rows.front();
  CHECK_FALSE(metal1.visible);
  CHECK(metal1.meta() == "hidden");
  CHECK(metal1.accessible_name() == "metal1, plane metal1, hidden");
  CHECK(metal1.toggle_command() == "see metal1");
  LayerRow shown = metal1;
  shown.visible = true;
  CHECK(shown.meta() == "metal1");
  CHECK(shown.toggle_command() == "see no metal1");
}

TEST_CASE("hierarchy rows follow the mockup's tree", "[cap:hefesto-app-ux][req:Main window layout]") {
  db::CellDef top("inverter_demo", 4);
  db::CellDef nfet("nfet_01v8", 4);
  db::CellDef pfet("pfet_01v8_hvt", 4);
  db::CellDef via("via1_2x1", 4);
  db::CellDef leaf("leaf", 4);
  add_use(nfet, leaf, "L1");  // nfet has children: expandable
  add_use(pfet, leaf, "L2");
  db::CellUse* x1 = add_use(top, nfet, "X1");
  add_use(top, pfet, "X2");
  for (int i = 0; i < 4; ++i) add_use(top, via, "V" + std::to_string(i));
  db::CellUse* arr = add_use(top, via, "A1");
  arr->array = db::ArrayInfo{0, 3, 0, 1, 10, 10};

  const auto collapsed = hierarchy_rows(top, [](const db::CellUse&) { return false; });
  REQUIRE(collapsed.size() == 5);
  CHECK(collapsed[0].text == "▾ inverter_demo");
  CHECK(collapsed[0].depth == 0);
  CHECK_FALSE(collapsed[0].expandable);  // the root is always expanded
  CHECK(collapsed[1].text == "▸ nfet_01v8 (X1)");
  CHECK(collapsed[1].depth == 1);
  CHECK(collapsed[1].expandable);
  CHECK(collapsed[1].use == x1);
  CHECK(collapsed[2].text == "▸ pfet_01v8_hvt (X2)");
  CHECK(collapsed[3].text == "▸ via1_2x1 ×4");  // leaf instances of one definition grouped
  CHECK(collapsed[3].use == nullptr);
  CHECK(collapsed[3].expandable);  // a leaf still has contents to show
  CHECK(collapsed[3].boxes.size() == 4);
  CHECK(collapsed[4].text == "▸ via1_2x1 (A1) [4×2]");  // arrays keep their own row

  const auto expanded = hierarchy_rows(top, [x1](const db::CellUse& u) { return &u == x1; });
  REQUIRE(expanded.size() == 6);
  CHECK(expanded[1].text == "▾ nfet_01v8 (X1)");
  CHECK(expanded[2].text == "▸ leaf (L1)");
  CHECK(expanded[2].depth == 2);

  CHECK(hierarchy_rows(top, nullptr, 3).size() == 3);  // row limit
}

TEST_CASE("hierarchy rows expand and unexpand through core commands on the box",
          "[cap:ui-windows-and-views][req:Per-window expansion state]") {
  db::CellDef top("top", 4);
  db::CellDef mid("mid", 4);
  db::CellDef leaf("leaf", 4);
  db::CellUse* inner = add_use(mid, leaf, "L1");
  inner->bbox = db::Rect{0, 0, 10, 20};
  db::CellUse* x1 = add_use(top, mid, "X1");
  x1->bbox = db::Rect{100, 200, 150, 260};
  x1->transform = db::Transform{1, 0, 100, 0, 1, 200};  // translate by (100, 200)

  const auto collapsed = hierarchy_rows(top, [](const db::CellUse&) { return false; });
  REQUIRE(collapsed.size() == 2);
  CHECK(collapsed[0].toggle_command().empty());  // the root is always expanded
  // Expanding opens one level: the child (L1, at 100,200 in root coordinates) is unexpanded again.
  CHECK(collapsed[1].toggle_command() ==
        "box values 100i 200i 150i 260i; expand surround; box values 100i 200i 110i 220i; unexpand surround");
  CHECK(collapsed[1].toggle_command(db::Rect{0, 0, 5, 5}) ==
        "box values 100i 200i 150i 260i; expand surround; box values 100i 200i 110i 220i; unexpand surround; "
        "box values 0i 0i 5i 5i");

  const auto open = hierarchy_rows(top, [x1](const db::CellUse& u) { return &u == x1; });
  REQUIRE(open.size() == 3);
  CHECK(open[1].toggle_command() == "box values 100i 200i 150i 260i; unexpand surround");
  // Nested rows carry their box in root coordinates.
  REQUIRE(open[2].boxes.size() == 1);
  CHECK(open[2].boxes[0] == db::Rect{100, 200, 110, 220});
  CHECK(open[2].toggle_command() == "box values 100i 200i 110i 220i; expand surround");

  // A grouped row toggles each of its instances.
  db::CellUse* l2 = add_use(top, leaf, "L2");
  l2->bbox = db::Rect{0, 0, 10, 20};
  db::CellUse* l3 = add_use(top, leaf, "L3");
  l3->bbox = db::Rect{20, 0, 30, 20};
  const auto grouped = hierarchy_rows(top, [](const db::CellUse&) { return false; });
  REQUIRE(grouped.size() == 3);
  CHECK(grouped[2].text == "▸ leaf ×2");
  CHECK(grouped[2].toggle_command() ==
        "box values 0i 0i 10i 20i; expand surround; box values 20i 0i 30i 20i; expand surround");
}

TEST_CASE("top bar shows cell, PDK, version and magic-compat", "[cap:hefesto-app-ux][req:Main window layout]") {
  HeaderInput in;
  CHECK(header_text(in).title == "No cell loaded");
  CHECK(header_text(in).subtitle == "minimum · magic-compat on");
  in.cell = "inverter_demo";
  in.technology = "sky130A";
  in.pdk = "sky130A";
  in.pdk_version = "1.0.471";
  const HeaderText text = header_text(in);
  CHECK(text.title == "inverter_demo");
  CHECK(text.subtitle == "sky130A · 1.0.471 · magic-compat on");
  in.magic_compat = false;
  CHECK(header_text(in).subtitle == "sky130A · 1.0.471 · magic-compat off");

  CHECK(short_tech_version({"version 1.0.493-0-g0fe599b"}) == "1.0.493");
  CHECK(short_tech_version({" 1.0.471 "}) == "1.0.471");
  CHECK_FALSE(short_tech_version({}).has_value());
}

TEST_CASE("later-milestone controls are present, disabled and labelled", "[cap:hefesto-app-ux][req:Main window layout]") {
  REQUIRE(deferred_view_tabs().size() == 3);
  REQUIRE(deferred_actions().size() == 3);
  for (const DeferredControl& c : deferred_actions()) CHECK(c.tooltip.find("arrives in M") != std::string_view::npos);
  const auto& tools = tool_rail();
  REQUIRE(tools.size() == 8);
  CHECK(tools.front().label == "Box tool");
  CHECK(tools.front().enabled);
  CHECK(tools.back().label == "Zoom to fit");
  std::set<std::string_view> names;
  for (const ToolButton& t : tools) {
    names.insert(t.label);
    CHECK_FALSE(t.label.empty());  // icon-only buttons carry accessible names
    CHECK_FALSE(t.icon.empty());
  }
  CHECK(names.size() == tools.size());
}

TEST_CASE("M1 status chips pair shape and text", "[cap:hefesto-design-system][req:Status is never color-only]") {
  CHECK(drc_chip_m1().label() == "◆ DRC not run");
  CHECK(lvs_chip_m1().label() == "◆ LVS not run");
  CHECK(StatusChip{StatusKind::success, "DRC clean"}.label() == "● DRC clean");
  CHECK(status_for_severity("error") == StatusKind::error);
  CHECK(status_for_severity("warning") == StatusKind::warning);
  CHECK(status_for_severity("note") == StatusKind::info);
  CHECK(diagnostic_line(StatusKind::error, "CELL-READ-001 error: x") == "■ CELL-READ-001 error: x");
}

TEST_CASE("lengths are shown in lambda until a micron scale exists", "[cap:hefesto-app-ux][req:Main window layout]") {
  const UnitScale lambda{2, 1, std::nullopt};  // 2 internal units per lambda
  CHECK(format_length(24.0, lambda) == "12");
  CHECK(format_length(1.0, lambda) == "0.5");
  CHECK(lambda.unit_name() == "lambda");
  const UnitScale microns{1, 1, 0.005};
  CHECK(format_length(233.0, microns) == "1.165");
  CHECK(microns.unit_name() == "um");
}

TEST_CASE("the micron scale comes from the technology's cifoutput style", "[cap:hefesto-app-ux][req:Main window layout]") {
  CHECK_FALSE(microns_per_lambda(std::nullopt));
  // sky130A: `scalefactor 10 nanometers` -> 10 with expander 10: 0.01 um per lambda.
  tech::CifGridInfo sky130;
  sky130.scale_factor = 10;
  sky130.expander = 10;
  REQUIRE(microns_per_lambda(sky130));
  CHECK(*microns_per_lambda(sky130) == 0.01);
  // scmos-style `scalefactor 100` (centimicrons): 1 um per lambda.
  tech::CifGridInfo centi;
  centi.scale_factor = 100;
  CHECK(*microns_per_lambda(centi) == 1.0);
  const UnitScale scale{2, 1, microns_per_lambda(sky130)};  // magscale 1 2
  CHECK(format_length(276.0, scale) == "1.380");  // sky130_fd_sc_hd__inv_1 width
}

TEST_CASE("status bar shows cursor, box, edit cell, units and DRC", "[cap:hefesto-app-ux][req:Main window layout]") {
  StatusBarInput in;
  in.scale = UnitScale{1, 1, 0.005};
  in.cursor = db::Point{233, 296};
  in.box = db::Rect{0, 0, 34, 330};
  in.edit_cell = "inverter_demo";
  in.message = "load inverter_demo\nsecond line";
  const StatusBarText text = status_bar_text(in);
  CHECK(text.cursor == "x 1.165  y 1.480 um");
  CHECK(text.box == "box 0.170 × 1.650");
  CHECK(text.edit == "edit: inverter_demo");
  CHECK(text.snap == "snap: lambda · units: um");
  CHECK(text.drc == "◆ DRC not run");
  CHECK(text.message == "load inverter_demo");
  CHECK(text.problems.empty());
  in.snap = "internal";
  in.problems = StatusChip{StatusKind::error, "1 error"};
  const StatusBarText snapped = status_bar_text(in);
  CHECK(snapped.snap == "snap: internal · units: um");
  CHECK(snapped.problems == "■ 1 error");
  const StatusBarText none = status_bar_text(StatusBarInput{});
  CHECK(none.edit == "edit: (none)");
  CHECK(none.box == "box –");
}

TEST_CASE("inspector shows the box, else the edit cell", "[cap:hefesto-app-ux][req:Main window layout]") {
  const UnitScale scale{1, 1, 0.005};
  db::CellDef cell("inverter_demo", 4);
  const InspectorView box = inspector_view(db::Rect{0, 0, 34, 330}, &cell, scale);
  CHECK(box.section == "BOX");
  CHECK(box.subtitle == "in edit cell inverter_demo");
  REQUIRE(box.rows.size() == 4);
  CHECK(box.rows[0].key == "Width");
  CHECK(box.rows[0].value == "0.170 um");
  const InspectorView c = inspector_view(std::nullopt, &cell, scale);
  CHECK(c.section == "CELL");
  CHECK(c.title == "inverter_demo");
  CHECK(inspector_view(std::nullopt, nullptr, scale).title == "Nothing loaded");
}

TEST_CASE("problems outside the command bar are counted and shown with their shape",
          "[cap:hefesto-design-system][req:Status is never color-only]") {
  std::vector<std::string> panel;
  std::vector<std::string> status;
  ProblemSink sink([&](StatusKind, const std::string& line, const std::string& message) {
    panel.push_back(line);
    status.push_back(message);
  });
  CHECK_FALSE(sink.tally().chip());

  sink.emit(diag::Diagnostic{diag::Severity::info, "APP-INFO-001", "loaded", {}, {}});
  REQUIRE(panel.size() == 1);
  CHECK(panel[0].starts_with("◆ "));
  CHECK(status[0].empty());  // info leaves the status message alone
  CHECK_FALSE(sink.tally().chip());

  sink.emit(diag::Diagnostic{diag::Severity::warning, "STY-WARN-001", "style 99 is not defined\nmore", {}, {}});
  CHECK(status[1].starts_with("▲ "));
  CHECK(status[1].find('\n') == std::string::npos);  // first line only
  REQUIRE(sink.tally().chip());
  CHECK(sink.tally().chip()->label() == "▲ 1 warning");

  sink.emit(diag::Diagnostic{diag::Severity::error, "STY-OPEN-001", "Couldn't open color map file", {}, {}});
  CHECK(status[2].starts_with("■ "));
  CHECK(status[2].find("Couldn't open color map file") != std::string::npos);
  CHECK(sink.tally().chip()->label() == "■ 1 error · ▲ 1 warning");
  CHECK(sink.tally().chip()->kind == StatusKind::error);
  sink.emit(diag::Diagnostic{diag::Severity::error, "STY-OPEN-001", "again", {}, {}});
  CHECK(sink.tally().errors() == 2);
  CHECK(sink.tally().chip()->label() == "■ 2 errors · ▲ 1 warning");
}

namespace {

/// A fixed-width measure: every code point is 10 units wide.
float monospace(std::string_view s) {
  float n = 0.0F;
  for (const char c : s) n += (static_cast<unsigned char>(c) & 0xC0U) != 0x80U ? 10.0F : 0.0F;
  return n;
}

}  // namespace

// Regression: the tree only clipped the tail, which hid the ×N count and the
// instance id, so a group read as a single instance.
TEST_CASE("hierarchy rows keep their suffix visible", "[cap:hefesto-app-ux][req:Main window layout]") {
  db::CellDef top("top", 4);
  db::CellDef nand("sky130_fd_sc_hd__nand2_2", 4);
  db::CellDef conb("sky130_fd_sc_hd__conb_1", 4);
  add_use(top, nand, "N1");
  add_use(top, nand, "N2");
  add_use(top, conb, "C1");
  const auto rows = hierarchy_rows(top, [](const db::CellUse&) { return false; });
  REQUIRE(rows.size() == 3);
  CHECK(rows[1].suffix == " ×2");
  CHECK(rows[1].name_part() == "▸ sky130_fd_sc_hd__nand2_2");
  CHECK(rows[2].suffix == " (C1)");

  const HierarchyRowLayout narrow = hierarchy_row_layout(rows[1], 150.0F, monospace);
  // Regression: the tail ellipsis kept only the shared library prefix
  // ("▸ sky130_fd…"); the middle goes now, so the distinct tail shows.
  CHECK(narrow.name == "▸ s…_nand2_2");  // 12 code points + the 3-code-point suffix fit 150
  CHECK(narrow.suffix_x == 120.0F);
  const HierarchyRowLayout wide = hierarchy_row_layout(rows[1], 1000.0F, monospace);
  CHECK(wide.name == "▸ sky130_fd_sc_hd__nand2_2");
  CHECK(wide.suffix_x == monospace(wide.name));
  CHECK(wide.suffix == " ×2");

  // Regression: a long instance id ellipsized the whole name to "…"; the
  // name now keeps half the row and the id is ellipsized instead.
  db::CellDef top2("top2", 4);
  add_use(top2, conb, "sky130_fd_sc_hd__conb_1_0");
  const auto long_id = hierarchy_rows(top2, [](const db::CellUse&) { return false; });
  const HierarchyRowLayout l = hierarchy_row_layout(long_id[1], 200.0F, monospace);
  CHECK(monospace(l.name) >= 90.0F);
  CHECK(l.name.starts_with("▸ "));
  CHECK(l.name.ends_with("conb_1"));
  CHECK(monospace(l.name) + monospace(l.suffix) <= 200.0F);
  CHECK(l.suffix.find("…") != std::string::npos);
  CHECK(l.suffix.ends_with("_1_0)"));  // the distinct end of the id stays
}

TEST_CASE("sky130 rows that share the library prefix stay distinct when narrow",
          "[cap:hefesto-app-ux][req:Main window layout]") {
  CHECK(ellipsize_middle("sky130_fd_sc_hd__nand2_1", 120.0F, monospace) == "sky…_nand2_1");
  CHECK(ellipsize_middle("sky130_fd_sc_hd__nor2_1", 120.0F, monospace) == "sky…__nor2_1");
  CHECK(ellipsize_middle("inv_1", 120.0F, monospace) == "inv_1");
  CHECK(ellipsize_middle("abcdef", 5.0F, monospace) == "…");
  CHECK(ellipsize_middle("", 0.0F, monospace).empty());
}

// Regression: a group with only some instances expanded showed ▸ like a
// fully collapsed one, disagreeing with the canvas.
TEST_CASE("a partly expanded group says so", "[cap:hefesto-app-ux][req:Main window layout]") {
  db::CellDef top("top", 4);
  db::CellDef inv("inv_2", 4);
  db::CellUse* a = add_use(top, inv, "A");
  add_use(top, inv, "B");
  const auto partial = hierarchy_rows(top, [a](const db::CellUse& u) { return &u == a; });
  REQUIRE(partial.size() == 2);
  CHECK(partial[1].partial);
  CHECK_FALSE(partial[1].expanded);
  CHECK(partial[1].text == "▸ inv_2 ×2 · 1 open");
  CHECK(partial[1].accessible_name() == "▸ inv_2 ×2 · 1 open, partly expanded");
  const auto all = hierarchy_rows(top, [](const db::CellUse&) { return true; });
  CHECK(all[1].text == "▾ inv_2 ×2");
  CHECK_FALSE(all[1].partial);
  CHECK(all[1].accessible_name() == "▾ inv_2 ×2, expanded");
  const auto none = hierarchy_rows(top, [](const db::CellUse&) { return false; });
  CHECK(none[1].accessible_name() == "▸ inv_2 ×2, collapsed");
  CHECK(none[0].accessible_name() == "▾ top, expanded");
}

TEST_CASE("ellipsize and wrap keep whole code points", "[cap:hefesto-app-ux][req:Main window layout]") {
  CHECK(ellipsize("abc", 30.0F, monospace) == "abc");
  CHECK(ellipsize("abcdef", 40.0F, monospace) == "abc…");
  CHECK(ellipsize("■ abcdef", 40.0F, monospace) == "■ a…");
  CHECK(ellipsize("abc", 5.0F, monospace) == "…");
  CHECK(wrap_columns("error: X at /a/b/c/d", 12, 2) ==
        std::vector<std::string>{"error: X at ", "  /a/b/c/d"});
  CHECK(wrap_columns("abcdefghijklmnop", 10, 0) == std::vector<std::string>{"abcdefghij", "klmnop"});
  CHECK(wrap_columns("a\nb", 10, 2) == std::vector<std::string>{"a", "  b"});
  CHECK(wrap_columns("", 10, 2) == std::vector<std::string>{""});
  CHECK(wrap_to_width("one two three", 80.0F, monospace) == std::vector<std::string>{"one two ", "three"});
}

// Regression: with a technology but no display styles the Layers panel told
// the user to load a technology.
TEST_CASE("layer panel empty text names the cause", "[cap:hefesto-app-ux][req:Main window layout]") {
  CHECK(layer_empty_hint("minimum", false) == "Load a technology with display styles");
  CHECK(layer_empty_hint("", false) == "Load a technology with display styles");
  CHECK(layer_empty_hint("sky130A", false) == "No display styles found for sky130A (see Diagnostics)");
  CHECK(layer_empty_hint("sky130A", true) == "No layer matches the filter");
}

// Regression: toggles, checkboxes and the status bar carried no state for
// screen readers, and problems were never announced.
TEST_CASE("accessible names carry state and problems are announced", "[cap:hefesto-app-ux][req:Accessibility and localization]") {
  CHECK(toggle_accessible_name("Box tool", true) == "Box tool, pressed");
  CHECK(toggle_accessible_name("Console and diagnostics", false) == "Console and diagnostics, not pressed");
  CHECK(checkbox_accessible_name("Accept the PDK license", true) == "Accept the PDK license, checked");
  CHECK(checkbox_accessible_name("Accept the PDK license", false) == "Accept the PDK license, not checked");

  const auto error = problem_announcement(StatusKind::error, "■ PDK-INSTALL-004: not installed");
  REQUIRE(error);
  CHECK(error->text == "Error: PDK-INSTALL-004: not installed");
  CHECK(error->assertive);
  const auto warning = problem_announcement(StatusKind::warning, "▲ styles missing");
  REQUIRE(warning);
  CHECK(warning->text == "Warning: styles missing");
  CHECK_FALSE(warning->assertive);
  CHECK_FALSE(problem_announcement(StatusKind::info, "◆ ready"));
  CHECK_FALSE(problem_announcement(StatusKind::error, ""));

  StatusBarText text;
  text.message = "■ error: APP-FILE-001";
  text.problems = "■ 1 error";
  text.drc = "◆ DRC not run";
  CHECK(status_bar_accessible_name(text) == "Status: ■ error: APP-FILE-001. ■ 1 error. ◆ DRC not run");
  CHECK(status_bar_accessible_name(StatusBarText{}) == "Status");
}

// Regression: after `load` of another cell the desktop kept drawing (and
// inspecting) the previous cell's box. Magic hides a box whose root differs.
TEST_CASE("the box shows only in the window of its root cell", "[cap:hefesto-app-ux][req:Main window layout]") {
  db::CellDef old_cell("sky130_fd_sc_hd__dfxtp_1", 4);
  db::CellDef new_cell("sky130_fd_sc_hd__inv_1", 4);
  const std::optional<db::Rect> box = db::Rect{-19, -24, 755, 296};
  CHECK(visible_box(box, &old_cell, &old_cell) == box);
  CHECK_FALSE(visible_box(box, &old_cell, &new_cell).has_value());
  CHECK(visible_box(box, nullptr, &new_cell) == box);  // no root recorded: the edit cell's
  CHECK_FALSE(visible_box(std::nullopt, &new_cell, &new_cell).has_value());
}

TEST_CASE("a command line's status uses one format for raised problems",
          "[cap:hefesto-design-system][req:Status is never color-only]") {
  // Regression: a failed line read "■ Unrecognized layer…" while a problem
  // raised by a successful line read "■ error: CODE: …".
  const LineStatus error{StatusKind::error, "■ error: DB-PAINT-001: Unrecognized layer: q"};
  CHECK(command_line_status(false, "Unrecognized layer: q", error).text == error.text);
  CHECK(command_line_status(true, "ok", error).text == error.text);
  const LineStatus warning{StatusKind::warning, "▲ warning: X-1: careful"};
  CHECK(command_line_status(true, "ok", warning).kind == StatusKind::warning);
  const LineStatus failed = command_line_status(false, "failed", warning);  // a warning never hides the failure
  CHECK(failed.kind == StatusKind::error);
  CHECK(failed.text == "■ failed");
  CHECK(command_line_status(true, "ok", std::nullopt).text == "● ok");
}

TEST_CASE("the top bar hides groups only as far as needed",
          "[cap:hefesto-design-system][req:Screen patterns follow the reference mockups]") {
  // Regression: at the 1024 px minimum window the PDKs button was cut off and
  // the later buttons were not visible.
  const std::vector<float> groups = {290.0F, 230.0F, 330.0F};  // deferred actions, chips, view tabs
  CHECK(top_bar_hidden_groups(1600.0F, 1500.0F, groups) == 0);
  CHECK(top_bar_hidden_groups(1440.0F, 1500.0F, groups) == 1);
  CHECK(top_bar_hidden_groups(1024.0F, 1500.0F, groups) == 2);
  CHECK(top_bar_hidden_groups(600.0F, 1500.0F, groups) == 3);  // never more than the groups
  CHECK(top_bar_hidden_groups(1024.0F, 1024.0F, groups) == 0);
}
