// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The panel layout model (panels_layout.hpp; board M2-Panels; M4 CARRY task
// 11.2): `panel` commands on the layout, floating panels kept inside the
// window, drag to dock with a drop-zone highlight, panel headers, edge
// strips at narrow widths, and the layout saved per user and restored at
// launch.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/platform/file_system.hpp"
#include "hefesto/shell/panels_layout.hpp"

using namespace hefesto::shell;
namespace platform = hefesto::platform;

namespace {

const LayoutRect kWindow{0.0F, 0.0F, 1600.0F, 1000.0F};

PanelCommandResult run(PanelLayout& layout, std::vector<std::string> args) {
  return run_panel_command(layout, args, kWindow);
}

DockGeometry geometry() {
  DockGeometry g;
  g.window = kWindow;
  g.left = LayoutRect{48.0F, 48.0F, 240.0F, 760.0F};
  g.right = LayoutRect{1340.0F, 48.0F, 260.0F, 760.0F};
  g.bottom = LayoutRect{48.0F, 808.0F, 1552.0F, 160.0F};
  g.canvas = LayoutRect{288.0F, 48.0F, 1052.0F, 760.0F};
  return g;
}

}  // namespace

TEST_CASE("panel commands collapse, dock, float and reset the layout",
          "[cap:hefesto-app-ux][req:Panel layout and focus cycling]") {
  PanelLayout layout = default_panel_layout();
  CHECK(layout.in(DockZone::left) == std::vector<std::string>{"layers"});
  CHECK(layout.in(DockZone::right) == std::vector<std::string>{"inspector", "drc"});
  CHECK(run(layout, {"layers"}).message == "shown");
  CHECK(run(layout, {"layers", "hide"}).message == "hidden");
  CHECK(layout.find("layers")->collapsed);
  CHECK(run(layout, {"layers", "toggle"}).message == "shown");

  // The spec's scenario: dock and reset.
  CHECK(run(layout, {"layers", "dock", "right"}).message == "docked right");
  CHECK(run(layout, {"inspector", "dock", "left"}).message == "docked left");
  CHECK(layout.in(DockZone::left) == std::vector<std::string>{"inspector"});
  CHECK(layout.in(DockZone::right) == std::vector<std::string>{"drc", "layers"});  // a new tab at the end
  const PanelCommandResult reset = run(layout, {"reset"});
  CHECK(reset.ok);
  CHECK(reset.message == "default layout");
  CHECK(reset.changed);
  CHECK(layout == default_panel_layout());

  // Any panel docks at the bottom too; bad words change nothing.
  CHECK(run(layout, {"console", "dock", "left"}).ok);
  CHECK(layout.find("console")->dock == DockZone::left);
  const PanelLayout before = layout;
  CHECK_FALSE(run(layout, {"console", "dock", "floating"}).ok);
  CHECK_FALSE(run(layout, {"nosuch", "show"}).ok);
  CHECK_FALSE(run(layout, {"layers", "dock"}).ok);
  CHECK_FALSE(run(layout, {"layers", "float", "1", "2"}).ok);
  CHECK_FALSE(run(layout, {"layers", "float", "a", "b", "c", "d"}).ok);
  CHECK_FALSE(run(layout, {"layers", "sideways"}).ok);
  CHECK(layout == before);
  CHECK(run(layout, {}).message == std::string(panel_command_usage()));
}

TEST_CASE("a floating panel stays inside the window and docks back where it came from",
          "[cap:hefesto-app-ux][req:Panel layout and focus cycling]") {
  PanelLayout layout = default_panel_layout();
  CHECK(run(layout, {"inspector", "float", "1500", "-40", "400", "500"}).message == "floating");
  const PanelPlacement& p = *layout.find("inspector");
  CHECK(p.dock == DockZone::floating);
  CHECK(p.home == DockZone::right);
  CHECK(p.floating == LayoutRect{1200.0F, 0.0F, 400.0F, 500.0F});  // moved inside the window
  // Too small or too large: clamped.
  CHECK(clamp_floating(LayoutRect{10.0F, 10.0F, 20.0F, 20.0F}, kWindow).w == kMinFloatingSize);
  CHECK(clamp_floating(LayoutRect{0.0F, 0.0F, 4000.0F, 20.0F}, kWindow).w == kWindow.w);
  // The header's Dock button returns it to the right dock.
  const PanelHeader h = panel_header(layout, "inspector", "Inspector");
  CHECK(h.floating);
  CHECK(h.accessible_name == "Inspector, floating");
  REQUIRE(h.buttons.size() == 1U);
  CHECK(h.buttons[0].command == "panel inspector dock right");
  CHECK(run(layout, {"inspector", "dock", "right"}).ok);
  CHECK(layout.find("inspector")->dock == DockZone::right);
  // Floating again without a rectangle reuses the last one.
  CHECK(run(layout, {"inspector", "float"}).ok);
  CHECK(layout.find("inspector")->floating == LayoutRect{1200.0F, 0.0F, 400.0F, 500.0F});
}

TEST_CASE("dragging a panel header highlights the drop zone and issues a panel command",
          "[cap:hefesto-app-ux][req:Panel layout and focus cycling]") {
  const DockGeometry g = geometry();
  PanelDrag drag("layers", 100.0F, 60.0F, g.left);
  CHECK_FALSE(drag.drop_command().has_value());
  // Over the right dock: it is the dashed drop zone.
  CHECK(drag.move(1400.0F, 300.0F, g) == DropTarget{DockZone::right, g.right});
  CHECK(drag.drop_command() == "panel layers dock right");
  // Over the bottom dock.
  CHECK(drag.move(700.0F, 900.0F, g).zone == DockZone::bottom);
  CHECK(drag.drop_command() == "panel layers dock bottom");
  // Over the canvas: floating, the window under the pointer where it was taken.
  const DropTarget& f = drag.move(700.0F, 300.0F, g);
  CHECK(f.zone == DockZone::floating);
  CHECK(f.highlight == LayoutRect{648.0F, 240.0F, 240.0F, 760.0F});  // kept inside the window
  CHECK(drag.drop_command() == "panel layers float 648 240 240 760");
  // Esc cancels: nothing is issued.
  drag.cancel();
  CHECK_FALSE(drag.drop_command().has_value());

  // An empty dock takes a band at the canvas edge.
  DockGeometry no_left = g;
  no_left.left = LayoutRect{};
  PanelDrag d2("inspector", 1400.0F, 60.0F, g.right);
  CHECK(d2.move(300.0F, 400.0F, no_left) == DropTarget{DockZone::left, LayoutRect{288.0F, 48.0F, kDockBand, 760.0F}});

  // The drop runs through run_panel_command like a typed command.
  PanelLayout layout = default_panel_layout();
  CHECK(run(layout, {"layers", "float", "648", "240", "240", "760"}).ok);
  CHECK(layout.find("layers")->dock == DockZone::floating);
}

TEST_CASE("headers name their state and collapsed docks become edge strips at narrow widths",
          "[cap:hefesto-app-ux][req:Panel layout and focus cycling]") {
  PanelLayout layout = default_panel_layout();
  PanelHeader h = panel_header(layout, "layers", "Layers", 24);
  CHECK(h.title == "Layers");
  CHECK(h.count == "24");
  CHECK(h.accessible_name == "Layers, expanded");
  REQUIRE(h.buttons.size() == 2U);
  CHECK(h.buttons[0].accessible_name == "Collapse Layers");
  CHECK(h.buttons[0].command == "panel layers hide");
  CHECK(h.buttons[1].command == "panel layers float");
  (void)run(layout, {"layers", "hide"});
  h = panel_header(layout, "layers", "Layers", 24);
  CHECK(h.accessible_name == "Layers, collapsed");
  CHECK(h.buttons[0].command == "panel layers show");

  // 1024 px: the right dock collapses to its edge strip first.
  layout = default_panel_layout();
  CHECK(canvas_width(layout, 1600.0F) == 1100.0F);
  const PanelLayout narrow = fit_to_window(layout, 1024.0F);
  CHECK(narrow.strip[1]);
  CHECK_FALSE(narrow.strip[0]);
  CHECK(canvas_width(narrow, 1024.0F) >= kMinCanvasWidth);
  const auto strips = edge_strips(narrow);
  REQUIRE(strips.size() == 1U);
  CHECK(strips[0].zone == DockZone::right);
  CHECK(strips[0].panels == std::vector<std::string>{"inspector", "drc"});
  // Narrower still: the left one too.
  const PanelLayout tiny = fit_to_window(layout, 700.0F);
  CHECK(tiny.strip[0]);
  CHECK(tiny.strip[1]);
  // Showing a panel of a strip opens the strip.
  PanelLayout opened = narrow;
  (void)run(opened, {"drc", "show"});
  CHECK_FALSE(opened.strip[1]);
}

TEST_CASE("the panel layout is saved per user and restored at launch",
          "[cap:hefesto-app-ux][req:Panel layout and focus cycling]") {
  platform::MemoryFileSystem fs;
  const platform::Path file = platform::Path("/home/user/.local/state/hefesto") / std::string(kPanelLayoutFile);

  // First launch: no file, the default layout.
  RestoredLayout first = restore_panel_layout(fs, file);
  CHECK_FALSE(first.from_file);
  CHECK(first.warning.empty());
  CHECK(first.layout == default_panel_layout());

  // The user rearranges and quits; the layout is saved.
  PanelLayout layout = first.layout;
  (void)run(layout, {"layers", "dock", "right"});
  (void)run(layout, {"inspector", "float", "100", "120", "300", "420"});
  (void)run(layout, {"console", "show"});
  layout.left_width = 300.0F;
  layout.strip[2] = true;
  REQUIRE(save_panel_layout(fs, file, layout).ok());

  // Next launch: restored as it was.
  const RestoredLayout again = restore_panel_layout(fs, file);
  CHECK(again.from_file);
  CHECK(again.layout == layout);

  // A panel added by a later version appears where the defaults have it;
  // a panel the version no longer has is dropped.
  PanelLayout newer = default_panel_layout();
  newer.panels.insert(newer.panels.begin() + 1, PanelPlacement{"cells", DockZone::left, DockZone::left, false, {}});
  const auto merged = parse_panel_layout(panel_layout_json(layout), newer);
  REQUIRE(merged.has_value());
  REQUIRE(merged->find("cells") != nullptr);
  CHECK(merged->find("cells")->dock == DockZone::left);
  PanelLayout older = default_panel_layout();
  std::erase_if(older.panels, [](const PanelPlacement& p) { return p.id == "keys"; });
  const auto trimmed = parse_panel_layout(panel_layout_json(layout), older);
  REQUIRE(trimmed.has_value());
  CHECK(trimmed->find("keys") == nullptr);

  // A damaged file: the defaults, with a warning.
  REQUIRE(fs.write_atomic(file, "{\"version\": 1, \"panels\": [{\"id\": 3}]}").ok());
  const RestoredLayout bad = restore_panel_layout(fs, file);
  CHECK_FALSE(bad.from_file);
  CHECK_FALSE(bad.warning.empty());
  CHECK(bad.layout == default_panel_layout());
  CHECK_FALSE(parse_panel_layout("not json", default_panel_layout()).has_value());
  CHECK_FALSE(parse_panel_layout("{\"version\": 2, \"panels\": []}", default_panel_layout()).has_value());
}
