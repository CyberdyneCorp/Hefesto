// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The 3D view's view models (hefesto/shell/view3d_models.hpp; m10-3d-view
// tasks 9.1, 9.2, 9.6): the layer stack panel and its sync with `see`, the
// commands of every control, focus routing, gestures and keys, picking
// through a fake resolver, the frame's DrawParams, the MeshWorker inputs,
// the mesh status and the frame-time readout. Headless: no GPU, no Raven.
// The `wind3d` client session is WIND3D's test fixture. Owner: SHELL.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "../view3d/client/client_fixture.hpp"
#include "hefesto/gui/wrapper.hpp"
#include "hefesto/platform/file_picker.hpp"
#include "hefesto/render/mesh3d_shading.hpp"
#include "hefesto/shell/input_router.hpp"
#include "hefesto/shell/menu_bar_model.hpp"
#include "hefesto/shell/view3d_models.hpp"
#include "hefesto/view/window.hpp"
#include "hefesto/view3d/client/scene.hpp"
#include "hefesto/view3d/stack/ghost.hpp"

using namespace hefesto::shell;
namespace view3d = hefesto::view3d;
namespace db = hefesto::db;
using view3d::test::ClientSession;

namespace {

const View3dLayerRow* find_row(const std::vector<View3dLayerRow>& rows, std::string_view name) {
  for (const View3dLayerRow& r : rows) {
    if (r.name == name) return &r;
  }
  return nullptr;
}

/// The panel's rows of the open window (layout-layer stack, as the view draws it).
std::vector<View3dLayerRow> panel(ClientSession& s, const std::vector<float>& alpha = {}) {
  const view3d::LayerStack stack = view3d::layout_stack(s.editor, true);
  return layer_rows(stack, s.window(), alpha);
}

/// The display bindings of the fixture's styles (a headless session has none).
void bind_styles(ClientSession& s) {
  s.editor.styles.emplace();
  auto& types = s.editor.styles->types.styles_of_type;
  types.resize(hefesto::tech::kMaxTileTypes);
  types[*s.editor.technology->find_type("poly").id] = {hefesto::display::kFixedStyleCount + 1};
  types[*s.editor.technology->find_type("metal1").id] = {hefesto::display::kFixedStyleCount + 20};
  types[*s.editor.technology->find_type("metal2").id] = {hefesto::display::kFixedStyleCount + 21};
}

/// Evaluates a Tcl line (a toolkit-path command, as the controls run it).
void tcl(ClientSession& s, const std::string& line) {
  const hefesto::cmd::CommandResult r = s.eval(line);
  INFO(line << " -> " << (r.diagnostics.empty() ? std::string{} : r.diagnostics.front().text));
  REQUIRE(r.ok());
}

/// Opens the 3D window on c3d with the fixture's styles bound, in
/// layout-layer mode (it opens in mask-layer mode: an output style is current).
void open(ClientSession& s) {
  bind_styles(s);
  s.ok("specialopen wind3d c3d");
  REQUIRE(s.window().mode == view3d::Mode::mask_layer);
  s.w3d("cif");
  REQUIRE(s.window().mode == view3d::Mode::layout_layer);
}

struct FakeResolver final : PickResolver {
  std::optional<view3d::PickTarget> target;
  int selects = 0;
  view3d::HighlightSet net_ids{{7, 8}};
  view3d::HighlightSet selection_ids{{3}};
  std::optional<view3d::PickTarget> resolve(const view3d::MeshModel& /*mesh*/, view3d::PickId id) override {
    return id == 42 ? target : std::nullopt;
  }
  hefesto::diag::Status select(hefesto::editor::EditorState& /*editor*/, const view3d::PickTarget& /*t*/) override {
    ++selects;
    return hefesto::diag::Done{};
  }
  view3d::HighlightSet from_selection(const view3d::MeshModel& /*mesh*/,
                                      const hefesto::editor::EditorState& /*editor*/) override {
    return selection_ids;
  }
  view3d::HighlightSet net(const view3d::MeshModel& /*mesh*/, const hefesto::editor::EditorState& /*editor*/,
                           const view3d::PickTarget& /*seed*/) override {
    return net_ids;
  }
};

}  // namespace

// --- layer stack panel -----------------------------------------------------------

TEST_CASE("the layer stack panel lists layers bottom to top with their micron ranges",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  ClientSession s;
  open(s);
  const std::vector<View3dLayerRow> rows = panel(s);
  REQUIRE(rows.size() == 3);
  CHECK(rows[0].bottom <= rows[1].bottom);
  CHECK(rows[1].bottom <= rows[2].bottom);
  const View3dLayerRow* metal1 = find_row(rows, "metal1");
  REQUIRE(metal1 != nullptr);
  CHECK(metal1->visible);
  CHECK(metal1->alpha == 1.0F);
  CHECK_FALSE(metal1->range_um.empty());
  CHECK(micron_range(view3d::PhysicalHeight{1.38, 0.35}) == "1.38–1.73");
  CHECK(micron_range(std::nullopt).empty());  // no source: blank, never a made-up height
}

TEST_CASE("a layer with no height source shows a blank range",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  view3d::StackLayer flat;
  flat.name = "MET9";
  flat.source = view3d::ValueSource::none;
  const view3d::LayerStack stack{view3d::Mode::mask_layer, {flat}, true, 0.01};
  view3d::WindowState window;
  window.mode = view3d::Mode::mask_layer;
  const std::vector<View3dLayerRow> rows = layer_rows(stack, window);
  REQUIRE(rows.size() == 1);
  CHECK(rows[0].range_um.empty());
}

TEST_CASE("panel visibility and see stay in sync both ways (Q-MISC-71)",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  ClientSession s;
  open(s);
  // `see` from the console hides metal2: the panel shows it hidden.
  tcl(s, ".magic3d see no metal2");
  CHECK_FALSE(find_row(panel(s), "metal2")->visible);
  CHECK(find_row(panel(s), "metal1")->visible);
  // The panel's Show runs `see metal2` on the same set.
  const std::string show = see_command(".magic3d", "metal2", true);
  CHECK(show == ".magic3d see metal2");
  tcl(s, show);
  const hefesto::tech::TileType metal2 = *s.editor.technology->find_type("metal2").id;
  CHECK_FALSE(s.window().visibility.layout_hidden.test(metal2));
  CHECK(find_row(panel(s), "metal2")->visible);
  // The panel's Hide edits `see`'s set too.
  tcl(s, see_command(".magic3d", "metal1", false));
  const hefesto::tech::TileType metal1 = *s.editor.technology->find_type("metal1").id;
  CHECK(s.window().visibility.layout_hidden.test(metal1));
  CHECK_FALSE(find_row(panel(s), "metal1")->visible);
  // And the renderer's hidden flags follow the set.
  const view3d::LayerStack stack = view3d::layout_stack(s.editor, true);
  const std::vector<bool> hidden = hidden_flags(stack, s.window());
  const std::optional<view3d::LayerId> m1 = stack.find(metal1, hefesto::display::kFixedStyleCount + 20);
  REQUIRE(m1);
  CHECK(hidden[*m1]);
}

TEST_CASE("isolate hides every other layer and Show all brings them back",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  ClientSession s;
  open(s);
  const std::vector<std::string> isolate = isolate_commands(".magic3d", "metal1");
  REQUIRE(isolate.size() == 2);
  CHECK(isolate[0] == ".magic3d see no *");
  CHECK(isolate[1] == ".magic3d see metal1");
  for (const std::string& line : isolate) tcl(s, line);
  for (const View3dLayerRow& r : panel(s)) CHECK(r.visible == (r.name == "metal1"));
  tcl(s, show_all_command(".magic3d"));
  for (const View3dLayerRow& r : panel(s)) CHECK(r.visible);
}

TEST_CASE("per-layer transparency is kept outside Magic's state and reaches DrawParams",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  ClientSession s;
  open(s);
  const view3d::LayerStack stack = view3d::layout_stack(s.editor, true);
  View3dOptions options;
  const View3dLayerRow metal1 = *find_row(panel(s), "metal1");
  CHECK(next_alpha(1.0F) == 0.5F);
  CHECK(next_alpha(0.5F) == 0.2F);
  CHECK(next_alpha(0.2F) == 1.0F);
  set_row_alpha(options, metal1, next_alpha(metal1.alpha), stack.size());
  CHECK(find_row(panel(s, options.alpha), "metal1")->alpha == 0.5F);
  CHECK(find_row(panel(s, options.alpha), "metal2")->alpha == 1.0F);
  const hefesto::render::DrawParams params = draw_params(FrameInputs{&s.window(), &stack, nullptr, nullptr, &options});
  REQUIRE(params.alpha.size() == stack.size());
  CHECK(params.alpha[metal1.layers.front()] == 0.5F);
  CHECK(s.window().visibility == view3d::VisibilitySets{});  // `see`'s sets are untouched
}

// --- the parity and view controls ------------------------------------------------

TEST_CASE("camera presets run view with the preset's angles",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  ClientSession s;
  open(s);
  CHECK(preset_command(".magic3d", view3d::Preset::top) == ".magic3d view 0 0 0");
  CHECK(preset_command(".magic3d", view3d::Preset::front) == ".magic3d view -90 0 0");
  CHECK(preset_command(".magic3d", view3d::Preset::side) == ".magic3d view -90 0 -90");
  for (const view3d::Preset p :
       {view3d::Preset::isometric, view3d::Preset::front, view3d::Preset::side, view3d::Preset::top}) {
    tcl(s, preset_command(".magic3d", p));
    const view3d::Angles a = view3d::preset(p);
    CHECK(std::abs(s.window().view.view_x - a.x) < 1e-3F);
    CHECK(std::abs(s.window().view.view_y - a.y) < 1e-3F);
    CHECK(std::abs(s.window().view.view_z - a.z) < 1e-3F);
  }
}

TEST_CASE("the perspective toggle sets the frame's projection only",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  ClientSession s;
  open(s);
  const view3d::LayerStack stack = view3d::layout_stack(s.editor, true);
  View3dOptions options;
  const view3d::ViewState before = s.window().view;
  CHECK(draw_params(FrameInputs{&s.window(), &stack, nullptr, nullptr, &options}).projection.kind ==
        view3d::Projection::orthographic);
  options.perspective = true;
  CHECK(draw_params(FrameInputs{&s.window(), &stack, nullptr, nullptr, &options}).projection.kind ==
        view3d::Projection::perspective);
  CHECK(s.window().view == before);  // not part of Magic's view values
}

TEST_CASE("the level stepper, the mode toggle and the cut box run the window's commands",
          "[cap:hefesto-3d-visualization][req:Magic 3D view parity]") {
  ClientSession s;
  open(s);
  CHECK(level_command(".magic3d", 1) == ".magic3d level up");
  tcl(s, level_command(".magic3d", 1));
  CHECK(s.window().level == 2);
  tcl(s, level_command(".magic3d", -1));
  tcl(s, level_command(".magic3d", -1));
  CHECK(s.window().level == 0);
  CHECK(cutbox_command(".magic3d", true) == ".magic3d cutbox box");
  tcl(s, cutbox_command(".magic3d", true));
  REQUIRE(s.window().cut_box.has_value());
  CHECK(*s.window().cut_box == db::Rect{0, 0, 100, 50});  // the layout box, in the rendered cell
  tcl(s, cutbox_command(".magic3d", false));
  CHECK_FALSE(s.window().cut_box.has_value());
  CHECK(mode_command(".magic3d") == ".magic3d cif");
  tcl(s, mode_command(".magic3d"));
  CHECK(s.window().mode == view3d::Mode::mask_layer);
  tcl(s, mode_command(".magic3d"));
  CHECK(s.window().mode == view3d::Mode::layout_layer);
  CHECK(close_command(".magic3d") == ".magic3d closewindow");
  tcl(s, close_command(".magic3d"));
  CHECK_FALSE(s.editor.view3d.has_window());
}

TEST_CASE("mask-layer mode shows the phase-2 notice and draws the layout mesh scaled by f",
          "[cap:hefesto-3d-visualization][req:Magic 3D view parity]") {
  ClientSession s;
  open(s);
  CHECK_FALSE(mask_mode_notice(s.window()).has_value());
  CHECK(xy_factor(s.window()) == 1.0F);
  CHECK(visibility_editable(s.window()));
  tcl(s, ".magic3d cif");  // an output style is current: mask-layer mode
  REQUIRE(s.window().mode == view3d::Mode::mask_layer);
  CHECK(mask_mode_notice(s.window()) == "Mask layers arrive in phase 2");
  CHECK(xy_factor(s.window()) == s.window().applied_factor);
  CHECK_FALSE(visibility_editable(s.window()));
  const view3d::LayerStack stack = view3d::layout_stack(s.editor, true);
  for (const bool h : hidden_flags(stack, s.window())) CHECK_FALSE(h);  // the mask set applies to P2's layers
  const View3dOptions options;
  const hefesto::render::DrawParams params = draw_params(FrameInputs{&s.window(), &stack, nullptr, nullptr, &options});
  CHECK(params.level == s.window().level);
  CHECK(params.stack == &stack);
}

TEST_CASE("the header names the cell, mode, level and what opened the tab",
          "[cap:hefesto-design-system][req:Screen patterns follow the reference mockups]") {
  view3d::WindowState w;
  w.cell = "inverter";
  w.toolkit_path = view3d::kRender3dToolkitPath;
  const View3dHeader h = view3d_header(w);
  CHECK(h.title == "inverter · 3D");
  CHECK(h.subtitle == "layout layers · level 1 · render3d opened this tab");
  w.toolkit_path = view3d::kDefaultToolkitPath;
  w.mode = view3d::Mode::mask_layer;
  w.level = 0;
  CHECK(view3d_header(w).subtitle == "mask layers · level 0 · .magic3d");
}

TEST_CASE("Options > 3D Display is enabled", "[cap:ui-gui-managers][req:Wrapper menus]") {
  CHECK(later_milestone("magic::render3d") == std::nullopt);
  const MenuBarView view = menu_bar_view(hefesto::gui::wrapper_menus(".layout1"), {}, {}, {});
  bool found = false;
  for (const MenuBarMenu& m : view.menus) {
    for (const MenuBarItem& i : m.items) {
      if (i.label != "3D Display") continue;
      found = true;
      CHECK(i.enabled);
      CHECK(i.tooltip.empty());
      CHECK(i.script == "magic::render3d");
    }
  }
  CHECK(found);
}

// --- focus routing ---------------------------------------------------------------

TEST_CASE("lines typed while the 3D view has the focus go to the window's path",
          "[cap:hefesto-3d-visualization][req:Magic 3D view parity]") {
  ClientSession s;
  open(s);
  const IsWindowCommand is_w3d = [&s](std::string_view word) {
    return s.registry.find(word, view3d::kWind3dClient) != nullptr;
  };
  const std::optional<std::string> focused = std::string{".magic3d"};
  CHECK(route_typed_line("view 0 0 30", focused, is_w3d) == ".magic3d view 0 0 30");
  CHECK(route_typed_line("  level up", focused, is_w3d) == ".magic3d level up");
  CHECK(route_typed_line("see no metal1", focused, is_w3d) == ".magic3d see no metal1");
  CHECK(route_typed_line("puts hello", focused, is_w3d) == "puts hello");      // Tcl
  CHECK(route_typed_line("paint metal1", focused, is_w3d) == "paint metal1");  // a layout command
  CHECK(route_typed_line("view 0 0 30", std::nullopt, is_w3d) == "view 0 0 30");  // the Layout tab
  // The routed line acts on the 3D window.
  tcl(s, route_typed_line("level 3", focused, is_w3d));
  CHECK(s.window().level == 3);
}

// --- gestures and keys -----------------------------------------------------------

TEST_CASE("mouse and trackpad map to camera gestures", "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  using Kind = view3d::Gesture::Kind;
  CHECK(drag_gesture(PointerButton::left, 0, 10.0F, 4.0F, 800, 600)->kind == Kind::orbit);
  CHECK(drag_gesture(PointerButton::left, kModAlt, 10.0F, 4.0F, 800, 600)->kind == Kind::pan);
  CHECK(drag_gesture(PointerButton::left, kModShift, 10.0F, 4.0F, 800, 600)->kind == Kind::pan);
  CHECK(drag_gesture(PointerButton::right, 0, 10.0F, 4.0F, 800, 600)->kind == Kind::pan);
  CHECK(drag_gesture(PointerButton::middle, 0, 10.0F, 4.0F, 800, 600)->kind == Kind::pan);
  CHECK_FALSE(drag_gesture(PointerButton::other, 0, 1.0F, 1.0F, 800, 600).has_value());
  const auto zoom = wheel_gesture(0.0, 1.0, false, 0, 800, 600);
  REQUIRE(zoom);
  CHECK(zoom->kind == Kind::zoom);
  CHECK(zoom->factor > 1.0F);
  CHECK(wheel_gesture(0.0, 1.0, false, kModShift, 800, 600)->zoom_z);
  CHECK(wheel_gesture(1.0, 1.0, true, 0, 800, 600)->kind == Kind::pan);          // two-finger scroll
  CHECK(wheel_gesture(0.0, 1.0, true, kModSuper, 800, 600)->kind == Kind::zoom);  // Command-scroll
  CHECK_FALSE(wheel_gesture(0.0, 0.0, false, 0, 800, 600).has_value());
  CHECK(pinch_gesture(1.25, 0)->factor == 1.25F);
  CHECK_FALSE(pinch_gesture(0.0, 0).has_value());
  // An orbit changes what `view dy 0 dx rel` would: view_x and view_z.
  view3d::ViewState v;
  view3d::apply(v, *drag_gesture(PointerButton::left, 0, 10.0F, 4.0F, 800, 600));
  CHECK(v.view_y == 0.0F);
  CHECK(v.view_x != 0.0F);
  CHECK(v.view_z != 0.0F);
}

TEST_CASE("keys in the 3D view run the wind3d bindings on the window's path",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  ClientSession s;
  open(s);  // W3Dcreate installs the wind3d key macros
  CHECK(key_macro(s.editor.macros, 'L', kModShift) == "level up");
  CHECK(key_macro(s.editor.macros, 'L', 0) == "level down");
  CHECK(key_macro(s.editor.macros, raven_key::kUp, 0) == "scroll 0 -0.25 0 rel");
  CHECK(key_macro(s.editor.macros, 'Z', kModShift) == "zoom 2.0 1 rel");
  CHECK_FALSE(key_macro(s.editor.macros, raven_key::kF1, 0).has_value());
  tcl(s, window_command(".magic3d", *key_macro(s.editor.macros, 'L', kModShift)));
  CHECK(s.window().level == 2);
}

// Review of PR #86 (finding 1): a button in a layout window makes that
// window current (Magic's TxSetPoint on every event), so with magic-compat
// ON its macros reach the layout window again after a command on the 3D
// window's path routed script commands there (N1, Q-MISC-82).
namespace {

void layout_button_after_3d_command(bool magic_compat) {
  ClientSession s(magic_compat);
  tcl(s, "macro Button1 {box 20 10 70 45}");
  s.ok("specialopen wind3d c3d .render.magic");
  tcl(s, ".render.magic level 1");
  InputRouter router(*s.dispatcher, s.editor);
  PointerContext at;
  at.state.point = db::Point{20, 10};
  at.state.pixel_area = db::Rect{20, 10, 20, 10};
  at.pixel = db::Point{20, 10};
  at.window_name = "layout1";
  CHECK(router.button(1, true, 0, at));
  CHECK(s.editor.box == db::Rect{20, 10, 70, 45});
  CHECK_FALSE(s.editor.view3d.script_target_3d);
}

}  // namespace

TEST_CASE("a layout button after a 3D window command reaches the layout window with magic-compat ON",
          "[cap:view-3d][req:3D view commands]") {
  layout_button_after_3d_command(true);
}

TEST_CASE("a layout button after a 3D window command reaches the layout window with magic-compat OFF",
          "[cap:view-3d][req:3D view commands]") {
  layout_button_after_3d_command(false);
}

TEST_CASE("a press that stays within the slop is a click, otherwise a drag",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  PointerTracker t;
  t.press(100.0F, 100.0F, PointerButton::left, 0);
  CHECK_FALSE(t.move(101.0F, 101.0F, 800, 600).has_value());
  const auto click = t.release(101.0F, 101.0F);
  REQUIRE(click);
  CHECK(click->first == 101.0F);
  t.press(100.0F, 100.0F, PointerButton::left, 0);
  const auto g = t.move(120.0F, 100.0F, 800, 600);
  REQUIRE(g);
  CHECK(g->kind == view3d::Gesture::Kind::orbit);
  CHECK_FALSE(t.release(120.0F, 100.0F).has_value());
  t.press(100.0F, 100.0F, PointerButton::right, 0);
  CHECK_FALSE(t.release(100.0F, 100.0F).has_value());  // only the left button picks
}

// --- picking ---------------------------------------------------------------------

TEST_CASE("a click picks a target, selects it in 2D and isolates its net",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  ClientSession s;
  open(s);
  const view3d::LayerStack stack = view3d::layout_stack(s.editor, true);
  const view3d::MeshModel mesh;
  FakeResolver resolver;
  view3d::PickTarget target;
  target.layer = 0;
  target.use_path = {"inv_1"};
  resolver.target = target;
  View3dPicking picking{resolver};
  // Before a pick: the 2D selection's instances are highlighted (2D -> 3D).
  const view3d::HighlightSet* h = picking.highlight(&mesh, s.editor);
  REQUIRE(h != nullptr);
  CHECK(h->ids == std::vector<view3d::PickId>{3});
  CHECK_FALSE(picking.toggle_isolate_net(&mesh, s.editor));  // nothing picked yet
  // The background picks nothing.
  CHECK_FALSE(picking.click(&mesh, view3d::kNoPick, s.editor, stack).has_value());
  CHECK(resolver.selects == 0);
  // A target: selected in 2D (3D -> 2D), with its caption.
  const auto caption = picking.click(&mesh, 42, s.editor, stack);
  REQUIRE(caption);
  CHECK(*caption == stack.layers()[0].name + " · picked in inv_1 · selected in 2D");
  CHECK(resolver.selects == 1);
  // Isolate selected net: the net's instances, until toggled off.
  CHECK(picking.toggle_isolate_net(&mesh, s.editor));
  CHECK(picking.net_isolated());
  CHECK(picking.highlight(&mesh, s.editor)->ids == std::vector<view3d::PickId>{7, 8});
  CHECK(picking.toggle_isolate_net(&mesh, s.editor));
  CHECK_FALSE(picking.net_isolated());
  // A stale id clears the picked target.
  CHECK_FALSE(picking.click(&mesh, 5, s.editor, stack).has_value());
  CHECK_FALSE(picking.picked().has_value());
}

// Review of PR #86 (finding 3): pick ids are model ordinals, so after a
// rebuild the isolated net is recomputed from the picked target against
// the new model instead of keeping the old model's ids.
TEST_CASE("an isolated net follows a rebuilt model's ids",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  ClientSession s;
  open(s);
  const view3d::LayerStack stack = view3d::layout_stack(s.editor, true);
  const view3d::MeshModel first;
  view3d::MeshModel rebuilt;
  rebuilt.chunks.emplace_back();  // an extra chunk before the net's instances
  struct ModelResolver final : PickResolver {
    const view3d::MeshModel* rebuilt_model = nullptr;
    view3d::PickTarget target;
    std::optional<view3d::PickTarget> resolve(const view3d::MeshModel& /*mesh*/, view3d::PickId /*id*/) override {
      return target;
    }
    hefesto::diag::Status select(hefesto::editor::EditorState& /*e*/, const view3d::PickTarget& /*t*/) override {
      return hefesto::diag::Done{};
    }
    view3d::HighlightSet net(const view3d::MeshModel& mesh, const hefesto::editor::EditorState& /*e*/,
                             const view3d::PickTarget& /*seed*/) override {
      return &mesh == rebuilt_model ? view3d::HighlightSet{{17, 18}} : view3d::HighlightSet{{7, 8}};
    }
  } resolver;
  resolver.rebuilt_model = &rebuilt;
  View3dPicking picking{resolver};
  REQUIRE(picking.click(&first, 42, s.editor, stack).has_value());
  REQUIRE(picking.toggle_isolate_net(&first, s.editor));
  CHECK(picking.highlight(&first, s.editor)->ids == std::vector<view3d::PickId>{7, 8});
  picking.model_changed();
  CHECK(picking.net_isolated());
  CHECK(picking.highlight(&rebuilt, s.editor)->ids == std::vector<view3d::PickId>{17, 18});
  REQUIRE(picking.picked().has_value());  // the target is a tile, not an id: kept
  // Toggling off still clears the net.
  CHECK(picking.toggle_isolate_net(&rebuilt, s.editor));
  CHECK_FALSE(picking.net_isolated());
}

TEST_CASE("the pick resolver with a scope maps a picked layer back to its tile type in the rendered cell",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  ClientSession s;
  open(s);
  const auto stack = std::make_shared<const view3d::LayerStack>(view3d::layout_stack(s.editor, true));
  view3d::MeshWorker worker;
  worker.build(build_inputs(s.editor, s.window(), stack, false));
  REQUIRE(worker.wait_idle(10000));
  const view3d::MeshHandle mesh = worker.current();
  REQUIRE(mesh != nullptr);
  const view3d::PickId id = view3d::encode_pick(*mesh, view3d::PickRef{0, view3d::PickKind::rect, 0});
  REQUIRE(id != view3d::kNoPick);
  PickResolver resolver;
  // Without a scope: the plain resolve, which cannot map the layer to a type.
  const auto plain = resolver.resolve(*mesh, id);
  REQUIRE(plain);
  CHECK(plain->cell == nullptr);
  // With the drawn stack, the rendered cell and the compat flag: INSPECT's overload.
  const view3d::PickContext scope{stack.get(), s.cell, true};
  resolver.set_scope(scope);
  const auto scoped = resolver.resolve(*mesh, id);
  REQUIRE(scoped);
  CHECK(scoped == view3d::resolve(*mesh, scope, id));
  CHECK(scoped->cell == s.cell);
  CHECK(scoped->type.has_value());
}

TEST_CASE("the frame's highlight is passed as DrawParams::highlight",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  ClientSession s;
  open(s);
  const view3d::LayerStack stack = view3d::layout_stack(s.editor, true);
  const view3d::HighlightSet set{{1, 2}};
  const View3dOptions options;
  CHECK(draw_params(FrameInputs{&s.window(), &stack, nullptr, &set, &options}).highlight == &set);
}

// --- mesh builds -----------------------------------------------------------------

TEST_CASE("the worker builds the window's mesh under the session's lock; the J1 option needs a full build",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  ClientSession s;
  open(s);
  const auto stack = std::make_shared<const view3d::LayerStack>(view3d::layout_stack(s.editor, true));
  const view3d::BuildInputs in = build_inputs(s.editor, s.window(), stack, false);
  CHECK(in.lock == &s.editor.database_lock);
  CHECK(in.cell == s.cell);
  CHECK(in.technology == s.editor.technology.get());
  CHECK(in.clip.area == db::Rect{-1, -1, 101, 51});  // the bounding box grown by 1 (Q-MISC-70)
  CHECK_FALSE(in.clip.cut_box);
  view3d::MeshWorker worker;
  worker.build(in);
  REQUIRE(worker.wait_idle(10000));
  REQUIRE(worker.current() != nullptr);
  CHECK(rectangle_count(*worker.current()) > 0);
  // What decides a full build: the J1 option, the clip area, the stack.
  const BuildKey base = build_key(s.editor, s.window(), *stack, false);
  CHECK(build_key(s.editor, s.window(), *stack, false) == base);
  CHECK_FALSE(build_key(s.editor, s.window(), *stack, true) == base);
  tcl(s, ".magic3d cutbox 0 0 40 20");
  const BuildKey clipped = build_key(s.editor, s.window(), *stack, false);
  CHECK_FALSE(clipped == base);
  CHECK(clipped.clipped);
  CHECK(clip_area(*s.cell, s.window()).area == db::Rect{0, 0, 40, 20});
  CHECK_FALSE(build_key(s.editor, s.window(), view3d::layout_stack(s.editor, false), false) == clipped);
}

TEST_CASE("a layout window's change areas for the rendered cell feed the worker",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  ClientSession s;
  hefesto::view::LayoutWindow window{1, s.cell};
  (void)window.take_redisplay();
  CHECK_FALSE(change_areas(window, s.cell).has_value());  // nothing pending
  window.mark_redisplay(db::Rect{10, 10, 20, 20}, false);
  const auto areas = change_areas(window, s.cell);
  REQUIRE(areas);
  CHECK_FALSE(areas->all);
  REQUIRE(areas->areas.size() == 1);
  CHECK(areas->areas[0] == db::Rect{10, 10, 20, 20});
  db::CellDef other{"other", s.editor.technology->plane_count()};
  CHECK_FALSE(change_areas(window, &other).has_value());  // another cell: not this window's root
  window.mark_redisplay_all();
  CHECK(change_areas(window, s.cell)->all);
}

// --- the board look and ghosted layers (Q-MISC-84, Q-MISC-85) ---------------------

namespace {

/// The fixture's technology with a well plane (nwell) and a via contact
/// between metal1 and metal2, so the classifier has conducting planes.
std::string ghost_tech() {
  std::string text{view3d::test::kTech};
  const auto replace = [&text](std::string_view from, std::string_view to) {
    const std::size_t at = text.find(from);
    REQUIRE(at != std::string::npos);
    text.replace(at, from.size(), to);
  };
  replace("planes\n    active,a\n", "planes\n    well,w\n    active,a\n");
  replace("types\n    active  polysilicon,poly\n",
          "types\n    well  nwell\n    active  polysilicon,poly\n    active  polycontact,pc\n    metal1  via1\n");
  replace("contact\nend\n", "contact\n    pc poly metal1\n    via1 metal1 metal2\nend\n");
  return text;
}

/// nwell, poly, metal1, via1 and metal2 bound to styles and painted on c3d.
void paint_ghost(ClientSession& s) {
  bind_styles(s);
  auto& types = s.editor.styles->types.styles_of_type;
  types[*s.editor.technology->find_type("nwell").id] = {hefesto::display::kFixedStyleCount + 5};
  types[*s.editor.technology->find_type("via1").id] = {hefesto::display::kFixedStyleCount + 22};
  s.paint("nwell", db::Rect{-10, -10, 110, 60});
  s.paint("via1", db::Rect{30, 20, 40, 30});
}

/// The window on c3d (paint_ghost) in layout-layer mode.
void open_ghost(ClientSession& s) {
  paint_ghost(s);
  s.ok("specialopen wind3d c3d");
  s.w3d("cif");
  REQUIRE(s.window().mode == view3d::Mode::layout_layer);
}

/// The panel's rows with the classifier's defaults and `options`' overrides.
std::vector<View3dLayerRow> ghost_panel(ClientSession& s, View3dOptions& options) {
  const view3d::LayerStack stack = view3d::layout_stack(s.editor, s.project.compat.magic_compat);
  const std::vector<bool> ghosted = view3d::ghosted_by_default(stack, *s.editor.technology);
  apply_default_alpha(options, stack, ghosted);
  return layer_rows(stack, s.window(), options.alpha, {}, ghosted);
}

}  // namespace

TEST_CASE("Wells ghosted by default: the nwell row starts at 20 % and conducting rows stay opaque, both modes",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    ClientSession s{compat, ghost_tech()};
    open_ghost(s);
    View3dOptions options;
    const std::vector<View3dLayerRow> rows = ghost_panel(s, options);
    const View3dLayerRow* nwell = find_row(rows, "nwell");
    REQUIRE(nwell != nullptr);
    CHECK(nwell->ghosted);
    CHECK(nwell->alpha == kGhostAlpha);
    for (const char* name : {"polysilicon", "metal1", "via1", "metal2"}) {
      const View3dLayerRow* row = find_row(rows, name);
      REQUIRE(row != nullptr);
      CHECK_FALSE(row->ghosted);
      CHECK(row->alpha == 1.0F);
    }
    // The ghosted alpha reaches the renderer; `see`'s sets are untouched.
    const view3d::LayerStack stack = view3d::layout_stack(s.editor, compat);
    const hefesto::render::DrawParams params =
        draw_params(FrameInputs{&s.window(), &stack, nullptr, nullptr, &options});
    REQUIRE(params.alpha.size() == stack.size());
    CHECK(params.alpha[nwell->layers.front()] == kGhostAlpha);
    CHECK(s.window().visibility == view3d::VisibilitySets{});
  }
}

TEST_CASE("User makes a ghosted layer opaque: the choice survives a rebuild of the stack and see is unaffected",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  ClientSession s{true, ghost_tech()};
  open_ghost(s);
  View3dOptions options;
  const view3d::LayerStack stack = view3d::layout_stack(s.editor, true);
  const View3dLayerRow nwell = *find_row(ghost_panel(s, options), "nwell");
  set_row_alpha(options, nwell, 1.0F, stack.size());
  CHECK(options.alpha_override.at("nwell") == 1.0F);
  // A rebuild (a new stack object, the defaults applied again) keeps it.
  s.paint("metal1", db::Rect{100, 0, 120, 20});
  CHECK(find_row(ghost_panel(s, options), "nwell")->alpha == 1.0F);
  // Hide and Solo still run `see`; the transparency stays.
  tcl(s, see_command(".magic3d", "nwell", false));
  const std::vector<View3dLayerRow> after = ghost_panel(s, options);
  const View3dLayerRow* hidden = find_row(after, "nwell");
  REQUIRE(hidden != nullptr);
  CHECK_FALSE(hidden->visible);
  CHECK(hidden->alpha == 1.0F);
  // A user's 20 % on an opaque layer is kept too.
  set_row_alpha(options, *find_row(ghost_panel(s, options), "metal1"), 0.2F, stack.size());
  CHECK(find_row(ghost_panel(s, options), "metal1")->alpha == 0.2F);
}

TEST_CASE("Wells ghosted in mask-layer mode too: the P1 tab draws the layout stack there",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  // Regression: the docs said mask-layer mode ghosts nothing, but the P1 tab
  // builds the layout-layer stack in both window modes (tab_stack), so the
  // wells it draws are ghosted as in layout-layer mode.
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    ClientSession s{compat, ghost_tech()};
    paint_ghost(s);
    s.ok("specialopen wind3d c3d");
    REQUIRE(s.window().mode == view3d::Mode::mask_layer);  // an output style is current
    const view3d::LayerStack stack = tab_stack(s.editor, compat);
    CHECK(stack.mode() == view3d::Mode::layout_layer);
    const std::vector<bool> ghosted = view3d::ghosted_by_default(stack, *s.editor.technology);
    View3dOptions options;
    apply_default_alpha(options, stack, ghosted);
    const std::vector<View3dLayerRow> rows = layer_rows(stack, s.window(), options.alpha, {}, ghosted);
    const View3dLayerRow* nwell = find_row(rows, "nwell");
    REQUIRE(nwell != nullptr);
    CHECK(nwell->ghosted);
    CHECK(nwell->alpha == kGhostAlpha);
    const View3dLayerRow* metal1 = find_row(rows, "metal1");
    REQUIRE(metal1 != nullptr);
    CHECK_FALSE(metal1->ghosted);
  }
}

TEST_CASE("a mask-layer stack (phase 2) has no ghosted defaults",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  std::vector<view3d::StackLayer> masks(2);
  masks[0].name = "NWELL";
  masks[0].mask_layer = 0;
  masks[1].name = "MET1";
  masks[1].mask_layer = 1;
  const view3d::LayerStack stack{view3d::Mode::mask_layer, masks, true, 0.01};
  ClientSession s{true, ghost_tech()};
  View3dOptions options;
  apply_default_alpha(options, stack, view3d::ghosted_by_default(stack, *s.editor.technology));
  CHECK(options.alpha == std::vector<float>{1.0F, 1.0F});
}

TEST_CASE("ghost alphas follow the drawn stack while a new stack is built",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  // Regression: the defaults were applied for the stack being built while
  // the frames and rows still used the drawn stack, whose layer ids differ.
  const auto named = [](const std::vector<std::string>& names) {
    std::vector<view3d::StackLayer> layers(names.size());
    for (std::size_t i = 0; i < names.size(); ++i) layers[i].name = names[i];
    return view3d::LayerStack{view3d::Mode::mask_layer, layers, true, 0.01};
  };
  const view3d::LayerStack first = named({"nwell", "metal1"});
  const view3d::LayerStack second = named({"metal1", "poly", "nwell"});
  View3dOptions options;
  GhostDefaults ghost;
  ghost.building(first, {true, false}, false, options);  // nothing drawn yet: at once
  CHECK(options.alpha == std::vector<float>{kGhostAlpha, 1.0F});
  CHECK(ghost.ghosted() == std::vector<bool>{true, false});
  ghost.drawn(first, options);
  ghost.building(second, {false, false, true}, true, options);  // `first` still drawn
  CHECK(options.alpha == std::vector<float>{kGhostAlpha, 1.0F});
  CHECK(ghost.ghosted() == std::vector<bool>{true, false});
  ghost.drawn(second, options);
  CHECK(options.alpha == std::vector<float>{1.0F, 1.0F, kGhostAlpha});
  CHECK(ghost.ghosted() == std::vector<bool>{false, false, true});
  ghost.clear();
  CHECK(ghost.ghosted().empty());
}

TEST_CASE("ghosted rows announce their transparency to screen readers",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  ClientSession s{true, ghost_tech()};
  open_ghost(s);
  View3dOptions options;
  const std::vector<View3dLayerRow> rows = ghost_panel(s, options);
  const View3dLayerRow& nwell = *find_row(rows, "nwell");
  CHECK(row_accessible_name(nwell).find(", shown, ghosted, 20 percent opaque") != std::string::npos);
  CHECK(alpha_button_name(nwell) == "Transparency of nwell, 20 percent, ghosted by default");
  const View3dLayerRow& metal1 = *find_row(rows, "metal1");
  CHECK(row_accessible_name(metal1).find("percent") == std::string::npos);
  CHECK(alpha_button_name(metal1) == "Transparency of metal1, 100 percent");
}

TEST_CASE("Background from the theme: the frame clears to the canvas token in both themes",
          "[cap:view-3d][req:Viewing transform and shading]") {
  ClientSession s;
  open(s);
  const view3d::LayerStack stack = view3d::layout_stack(s.editor, true);
  const View3dOptions options;
  for (const ThemeMode mode : {ThemeMode::dark, ThemeMode::light}) {
    const Rgba& canvas = theme_tokens(mode).canvas;
    const std::array<float, 3> expected{canvas.r, canvas.g, canvas.b};
    CHECK(view3d_background(mode) == expected);
    FrameInputs in{&s.window(), &stack, nullptr, nullptr, &options};
    in.background = view3d_background(mode);
    const hefesto::render::DrawParams params = draw_params(in);
    REQUIRE(params.background.has_value());
    CHECK(*params.background == expected);
  }
}

namespace {

/// The desktop frame of the open window and the face factors the
/// renderer's board light gives it (top, -x side, -y side, bottom).
struct BoardFrame {
  hefesto::render::DrawParams params;
  std::array<float, 4> factors{};
};
BoardFrame board_frame(ClientSession& s, const view3d::LayerStack& stack, const View3dOptions& options) {
  FrameInputs in{&s.window(), &stack, nullptr, nullptr, &options};
  in.background = view3d_background(ThemeMode::dark);
  BoardFrame f{draw_params(in), {}};
  const std::array<float, 3> light = hefesto::render::board_light(s.window().view.view_z);
  const std::array<std::array<float, 3>, 4> normals{{{0, 0, 1}, {-1, 0, 0}, {0, -1, 0}, {0, 0, -1}}};
  for (std::size_t i = 0; i < normals.size(); ++i) {
    f.factors[i] = hefesto::render::light_intensity(f.params.lighting, light, normals[i]);
  }
  return f;
}

/// The board look's per-face rule (view-3d "Viewing transform and shading"):
/// tops at the largest factor, each side at least 0.15 below it and the two
/// sides distinct, bottoms at the ambient, the canvas background.
void check_board_look(const BoardFrame& f) {
  REQUIRE(f.params.background.has_value());
  CHECK(*f.params.background == view3d_background(ThemeMode::dark));
  CHECK(f.factors[0] == 1.0F);
  CHECK(f.factors[1] <= f.factors[0] - 0.15F);
  CHECK(f.factors[2] <= f.factors[0] - 0.15F);
  CHECK(f.factors[1] != f.factors[2]);
  CHECK(f.factors[3] == hefesto::render::kBoardAmbient);
}

}  // namespace

// render::Lighting has only the board value since round 4, so no mode can
// select Magic's lighting; these check that each mode's desktop frame gets
// the board's face factors and the theme background, and that the two
// modes' frames match.
TEST_CASE("Board look with magic-compat ON: per-face board shading and the canvas background",
          "[cap:view-3d][req:Viewing transform and shading]") {
  ClientSession s{true};
  open(s);
  tcl(s, preset_command(".magic3d", view3d::Preset::isometric));
  const view3d::LayerStack stack = tab_stack(s.editor, true);
  REQUIRE(stack.magic_compat());
  const View3dOptions options;
  check_board_look(board_frame(s, stack, options));
}

TEST_CASE("Board look with magic-compat OFF: the same frame as with it ON",
          "[cap:view-3d][req:Viewing transform and shading]") {
  ClientSession on{true};
  ClientSession off{false};
  open(on);
  open(off);
  tcl(on, preset_command(".magic3d", view3d::Preset::isometric));
  tcl(off, preset_command(".magic3d", view3d::Preset::isometric));
  const view3d::LayerStack on_stack = tab_stack(on.editor, true);
  const view3d::LayerStack off_stack = tab_stack(off.editor, false);
  REQUIRE_FALSE(off_stack.magic_compat());
  const View3dOptions options;
  const BoardFrame a = board_frame(on, on_stack, options);
  const BoardFrame b = board_frame(off, off_stack, options);
  check_board_look(b);
  CHECK(a.factors == b.factors);
  CHECK(a.params.background == b.params.background);
  CHECK(a.params.lighting == b.params.lighting);
  CHECK(a.params.xy_scale == b.params.xy_scale);
}

namespace {

/// The mesh of the open window, built as the tab builds it.
struct DrawnMesh {
  std::shared_ptr<const view3d::LayerStack> stack;
  view3d::MeshHandle mesh;
};
DrawnMesh build_mesh(ClientSession& s) {
  DrawnMesh d;
  d.stack = std::make_shared<const view3d::LayerStack>(tab_stack(s.editor, s.project.compat.magic_compat));
  view3d::MeshWorker worker;
  worker.build(build_inputs(s.editor, s.window(), d.stack, false));
  REQUIRE(worker.wait_idle(10000));
  d.mesh = worker.current();
  REQUIRE(d.mesh != nullptr);
  return d;
}

CameraFrame frame_of(ClientSession& s, const DrawnMesh& d) {
  return CameraFrame{&s.window(), d.mesh.get(), d.stack.get(), {}, 800, 600};
}

/// Opens c3d through `render3d`, as the 3D tab and Options > 3D Display do.
void open_from_desktop(ClientSession& s) {
  bind_styles(s);
  CHECK(s.value("magic::render3d c3d").empty());
  REQUIRE(s.window().toolkit_path == view3d::kRender3dToolkitPath);
}

void check_isometric(const view3d::ViewState& v) {
  const view3d::Angles iso = view3d::preset(view3d::Preset::isometric);
  CHECK(v.view_x == iso.x);
  CHECK(v.view_y == iso.y);
  CHECK(v.view_z == iso.z);
}

}  // namespace

TEST_CASE("Isometric default from above: a window the desktop opens gets the framed isometric preset at s_z = 1",
          "[cap:view-3d][req:Default view]") {
  ClientSession s;
  open_from_desktop(s);
  s.w3d("cif");
  REQUIRE(s.window().mode == view3d::Mode::layout_layer);
  const DrawnMesh d = build_mesh(s);
  const CameraFrame frame = frame_of(s, d);
  const std::vector<std::string> lines = default_camera_commands(frame);
  REQUIRE(lines.size() == 3);
  CHECK(lines[0].rfind(".render.magic view ", 0) == 0);
  CHECK(lines[1].rfind(".render.magic scroll ", 0) == 0);
  CHECK(lines[2].rfind(".render.magic zoom ", 0) == 0);
  // What frame_view gives for the drawn layers at the board z scale.
  view3d::ViewState expected = s.window().view;
  const view3d::Angles iso = view3d::preset(view3d::Preset::isometric);
  expected.view_x = iso.x;
  expected.view_y = iso.y;
  expected.view_z = iso.z;
  expected.scale_z = kBoardScaleZ;
  const view3d::Extent3 box = stack_extent(d.mesh->region, *d.stack, layers_present(*d.mesh, d.stack->size()));
  expected = view3d::frame_view(expected, box, frame.width_px, frame.height_px);
  for (const std::string& line : lines) tcl(s, line);
  // `view`, `scroll` and `zoom` report what the user sees.
  check_isometric(s.window().view);
  CHECK(s.window().view.view_x != 0.0F);  // tilted, not the top view
  CHECK(s.window().view.scale_z == kBoardScaleZ);
  CHECK(s.window().view.scale_xy == expected.scale_xy);
  CHECK(s.window().view.trans_x == expected.trans_x);
  CHECK(s.window().view.trans_z == expected.trans_z);
}

TEST_CASE("Isometric default in mask-layer mode keeps the heights in proportion: s_z = f",
          "[cap:view-3d][req:Default view]") {
  // Regression: the opening camera set s_z = 1 in mask-layer mode, where x
  // and y are drawn scaled by f, so the stack was drawn f times flatter
  // than in layout-layer mode (sky130A opens in mask-layer mode).
  ClientSession s;
  open_from_desktop(s);
  REQUIRE(s.window().mode == view3d::Mode::mask_layer);
  const float f = xy_factor(s.window());
  REQUIRE(f > 1.0F);
  CHECK(board_scale_z(s.window()) == kBoardScaleZ * f);
  const DrawnMesh d = build_mesh(s);
  for (const std::string& line : default_camera_commands(frame_of(s, d))) tcl(s, line);
  check_isometric(s.window().view);
  CHECK(s.window().view.scale_z == kBoardScaleZ * f);
  view3d::WindowState layout = s.window();
  layout.mode = view3d::Mode::layout_layer;
  CHECK(board_scale_z(layout) == kBoardScaleZ);
}

TEST_CASE("the default camera applies once per opened window",
          "[cap:view-3d][req:Default view]") {
  ClientSession s;
  open_from_desktop(s);
  DefaultCameraLatch latch;
  REQUIRE(latch.take(true));
  // A later sync keeps the user's camera.
  tcl(s, ".render.magic view 10 0 20");
  CHECK_FALSE(latch.take(true));
  CHECK(s.window().view.view_x == 10.0F);
  // closewindow re-arms it: a reopened window gets the default again.
  tcl(s, close_command(".render.magic"));
  CHECK_FALSE(latch.take(s.editor.view3d.has_window()));
  open_from_desktop(s);
  CHECK(latch.take(true));
}

TEST_CASE("presets frame the drawn layers: view, scroll and zoom report frame_view's camera",
          "[cap:view-3d][req:Viewing transform and shading]") {
  ClientSession s;
  open(s);
  const auto shared = std::make_shared<const view3d::LayerStack>(view3d::layout_stack(s.editor, true));
  const view3d::LayerStack& stack = *shared;
  view3d::MeshWorker worker;
  worker.build(build_inputs(s.editor, s.window(), shared, false));
  REQUIRE(worker.wait_idle(10000));
  REQUIRE(worker.current() != nullptr);
  const view3d::MeshModel& mesh = *worker.current();
  const std::vector<bool> present = layers_present(mesh, stack.size());
  // Only metal1 and metal2 are painted: poly is not framed.
  const view3d::Extent3 box = stack_extent(mesh.region, stack, present);
  const view3d::DrawHeight m1 = stack.draw_height(*stack.find(*s.editor.technology->find_type("metal1").id,
                                                              hefesto::display::kFixedStyleCount + 20));
  const view3d::DrawHeight m2 = stack.draw_height(*stack.find(*s.editor.technology->find_type("metal2").id,
                                                              hefesto::display::kFixedStyleCount + 21));
  CHECK(box.z0 == std::min(static_cast<double>(m1.height), static_cast<double>(m2.height)));
  CHECK(box.z1 == std::max(static_cast<double>(m1.height) + static_cast<double>(m1.thickness),
                           static_cast<double>(m2.height) + static_cast<double>(m2.thickness)));
  CHECK(box.x0 == -1.0);
  CHECK(box.x1 == 101.0);
  view3d::ViewState camera = s.window().view;
  const view3d::Angles iso = view3d::preset(view3d::Preset::isometric);
  camera.view_x = iso.x;
  camera.view_y = iso.y;
  camera.view_z = iso.z;
  camera.scale_z = kBoardScaleZ;
  const view3d::ViewState framed = view3d::frame_view(camera, box, 800, 600);
  for (const std::string& line : camera_commands(".magic3d", framed)) tcl(s, line);
  CHECK(s.window().view.view_x == framed.view_x);
  CHECK(s.window().view.view_z == framed.view_z);
  CHECK(s.window().view.trans_x == framed.trans_x);
  CHECK(s.window().view.trans_z == framed.trans_z);
  CHECK(s.window().view.scale_xy == framed.scale_xy);
  CHECK(s.window().view.scale_z == kBoardScaleZ);
}

TEST_CASE("a script-opened window keeps the top-down default view outside the desktop tab",
          "[cap:view-3d][req:Default view]") {
  // Regression: the tab applied the isometric camera to any window, also
  // one a script opened with `specialopen wind3d`.
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    ClientSession s{compat};
    open(s);  // specialopen wind3d c3d: the default path .magic3d
    CHECK_FALSE(desktop_opened(s.window()));
    const DrawnMesh d = build_mesh(s);
    REQUIRE(frame_ready(frame_of(s, d)));
    CHECK(default_camera_commands(frame_of(s, d)).empty());
    CHECK(s.window().view.view_x == 0.0F);
    CHECK(s.window().view.view_y == 0.0F);
    CHECK(s.window().view.view_z == 0.0F);
    // The presets still frame it when the user asks.
    CHECK(preset_camera_commands(frame_of(s, d), view3d::Preset::front).size() == 3);
  }
}

TEST_CASE("the axis gizmo follows the view's rotation",
          "[cap:view-3d][req:Viewing transform and shading]") {
  const auto near = [](float a, float b) { return std::abs(a - b) < 1e-4F; };
  view3d::ViewState top;  // (0, 0, 0): x right, y up, z toward the viewer
  const std::array<AxisArrow, 3> t = axis_gizmo(top);
  CHECK(t[0].name == 'x');
  CHECK((near(t[0].dx, 1.0F) && near(t[0].dy, 0.0F)));
  CHECK((near(t[1].dx, 0.0F) && near(t[1].dy, -1.0F)));
  CHECK((near(t[2].dx, 0.0F) && near(t[2].dy, 0.0F)));
  // Scale and translation do not change it.
  view3d::ViewState scaled = top;
  scaled.scale_xy = 0.01F;
  scaled.trans_x = 500.0F;
  CHECK((near(axis_gizmo(scaled)[0].dx, 1.0F) && near(axis_gizmo(scaled)[1].dy, -1.0F)));
  // The isometric preset is seen from above: z points up the screen and x
  // and y spread to either side.
  view3d::ViewState iso;
  const view3d::Angles a = view3d::preset(view3d::Preset::isometric);
  iso.view_x = a.x;
  iso.view_y = a.y;
  iso.view_z = a.z;
  const std::array<AxisArrow, 3> g = axis_gizmo(iso);
  CHECK(g[2].dy < -0.5F);
  CHECK(std::abs(g[2].dx) < 1e-3F);
  CHECK(g[0].dx * g[1].dx < 0.0F);
}

TEST_CASE("the mesh status and the frame-time readout",
          "[cap:hefesto-design-system][req:Screen patterns follow the reference mockups]") {
  CHECK(grouped(1248) == "1,248");
  CHECK(grouped(1004944) == "1,004,944");
  CHECK(grouped(12) == "12");
  CHECK(mesh_status_text(MeshStatus{}) == "No mesh");
  CHECK(mesh_status_text(MeshStatus{true, 0.42, 0, false}) == "Building mesh · 42 %");
  CHECK(mesh_status_text(MeshStatus{false, 1.0, 1248, true}) == "Mesh ready · 1,248 rectangles");
  CHECK(mesh_status_text(MeshStatus{false, 1.0, 1, true}) == "Mesh ready · 1 rectangle");
  FrameTimer timer;
  CHECK(timer.text() == "—");
  timer.add(10.0);
  timer.add(30.0);
  CHECK(timer.average_ms() == 20.0);
  CHECK(timer.text() == "20.0 ms · 50 fps");
  for (int i = 0; i < 40; ++i) timer.add(5.0);
  CHECK(timer.samples() == FrameTimer::kWindow);
  CHECK(timer.average_ms() == 5.0);
}

// --- round 5: controls, layout at 1280 px, P2 actions (tasks 9.6, 11.5) ------------

namespace {

const View3dControl kAllControls[] = {
    View3dControl::cross_section, View3dControl::export_model,    View3dControl::orthographic,
    View3dControl::perspective,   View3dControl::isometric,       View3dControl::top,
    View3dControl::front,         View3dControl::side,            View3dControl::substrate,
    View3dControl::isolate_net,   View3dControl::span_connected_layers, View3dControl::frame_time,
    View3dControl::close_view,    View3dControl::show_all_layers, View3dControl::mask_layers,
    View3dControl::layout_layers, View3dControl::level_down,      View3dControl::level_up,
    View3dControl::cut_box};
static_assert(std::size(kAllControls) == kView3dControlCount);

bool ends_with(const std::string& s, std::string_view tail) {
  return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

/// Two mask layers with `render` heights in µm (mask-layer mode: physical = as
/// stored) and one with no height.
view3d::LayerStack two_layer_stack() {
  view3d::StackLayer poly;
  poly.name = "POLY";
  poly.height = 0.33F;
  poly.thickness = 0.18F;
  poly.source = view3d::ValueSource::render;
  view3d::StackLayer met1;
  met1.name = "MET1";
  met1.height = 1.38F;
  met1.thickness = 0.36F;
  met1.source = view3d::ValueSource::render;
  view3d::StackLayer flat;
  flat.name = "NOHEIGHT";
  return view3d::LayerStack{view3d::Mode::mask_layer, {poly, met1, flat}, true, 0.01};
}

}  // namespace

TEST_CASE("every 3D tab control has a screen-reader name and toggles announce their state",
          "[cap:hefesto-app-ux][req:Accessibility and localization]") {
  for (const bool on : {false, true}) {
    View3dControlState s;
    s.window_open = true;
    s.mask_mode = on;
    s.cut_box = on;
    s.perspective = on;
    s.span_connected_layers = on;
    s.frame_time = on;
    s.picked = true;
    s.net_isolated = on;
    s.section_on = on;
    s.substrate_on = on;
    s.substrate_available = true;
    s.export_available = true;
    for (const View3dControl c : kAllControls) {
      const View3dControlView v = control_view(c, s);
      INFO("control " << static_cast<int>(c) << " label " << v.label);
      CHECK_FALSE(v.label.empty());
      CHECK_FALSE(v.accessible_name.empty());
      CHECK_FALSE(v.tooltip.empty());
      if (v.on) {
        CHECK(v.accessible_name == view3d_toggle_name(v.label, *v.on));
      } else {
        CHECK_FALSE((ends_with(v.accessible_name, ", on") || ends_with(v.accessible_name, ", off")));
      }
    }
  }
  View3dControlState s;
  s.window_open = true;
  s.section_on = true;
  CHECK(control_view(View3dControl::cross_section, s).accessible_name == "Cross-section, on");
  CHECK(control_view(View3dControl::level_down, s).accessible_name == "Level down");  // the label is "−"
  CHECK(control_view(View3dControl::level_up, s).accessible_name == "Level up");
  CHECK(control_view(View3dControl::layout_layers, s).accessible_name == "Layout layers, on");
  CHECK(control_view(View3dControl::mask_layers, s).accessible_name == "Mask layers, off");
  CHECK(level_text(2) == "Level 2");
}

TEST_CASE("controls are disabled, and exposed so, when they cannot act",
          "[cap:hefesto-app-ux][req:Accessibility and localization]") {
  const View3dControlState closed;  // no window
  for (const View3dControl c : {View3dControl::cross_section, View3dControl::export_model, View3dControl::isometric,
                                View3dControl::side, View3dControl::close_view, View3dControl::cut_box,
                                View3dControl::level_up, View3dControl::mask_layers}) {
    CHECK_FALSE(control_view(c, closed).enabled);
  }
  View3dControlState open;
  open.window_open = true;
  CHECK_FALSE(control_view(View3dControl::export_model, open).enabled);  // export3d not registered
  open.export_available = true;
  CHECK(control_view(View3dControl::export_model, open).enabled);
  CHECK_FALSE(control_view(View3dControl::isolate_net, open).enabled);  // nothing picked
  // Without the renderer's substrate slab the toggle is off and disabled.
  CHECK_FALSE(control_view(View3dControl::substrate, open).enabled);
  CHECK(control_view(View3dControl::substrate, open).accessible_name == "Substrate, off");
  open.substrate_available = true;
  CHECK(control_view(View3dControl::substrate, open).accessible_name == "Substrate, on");  // default on
  open.visibility_editable = false;
  CHECK_FALSE(control_view(View3dControl::show_all_layers, open).enabled);
}

// Regression (round-4 Linux X11 capture at 1280 x 800, also seen on macOS):
// the four presets in one row ran past the side panel ("Side" clipped), and
// the toolbar strip was wider than the view (Cut box and the status cut off).
TEST_CASE("the side panel's button grid and the floating toolbar fit a 1280-px-wide window",
          "[cap:hefesto-design-system][req:Screen patterns follow the reference mockups]") {
  // Side panel: two columns, as the board's camera grid.
  constexpr float kGap = 6.0F;
  const float cell = grid_cell_width(side_content_width(), 2, kGap);
  CHECK(2.0F * cell + kGap <= side_content_width());
  // Widths measured in the round-4 1280 x 800 captures (logical px): the
  // four presets in one row with 4 px gaps took 264 px of the 248 available.
  CHECK(86.0F + 52.0F + 61.0F + 53.0F + 3.0F * 4.0F > side_content_width());
  for (const float label : {86.0F, 52.0F, 61.0F, 53.0F, 100.0F /* Orthographic */, 105.0F /* Close 3D view */}) {
    CHECK(label <= cell);
  }
  // Toolbar groups (mode pair, level stepper, cut box, status), measured in
  // the same captures; the 3D view of a 1280-px window is 430 px wide.
  const std::vector<float> groups{218.0F, 130.0F, 80.0F, 214.0F};
  constexpr float kStageAt1280 = 430.0F;
  const float available = kStageAt1280 - 2.0F * kView3dToolbarMargin;
  CHECK(218.0F + 130.0F + 80.0F + 214.0F + 3.0F * kView3dToolbarSpacing > available);  // one strip did not fit
  const std::vector<std::size_t> rows = flow_rows(groups, available, kView3dToolbarSpacing);
  CHECK(flow_width(groups, rows, kView3dToolbarSpacing) <= available);
  CHECK(rows == std::vector<std::size_t>{0, 0, 1, 1});
  // One row at 1600 px (the view is about 750 px wide).
  CHECK(flow_rows(groups, 750.0F - 2.0F * kView3dToolbarMargin, kView3dToolbarSpacing) ==
        std::vector<std::size_t>{0, 0, 0, 0});
  // A group wider than the view takes a row of its own.
  CHECK(flow_rows(std::vector<float>{50.0F, 500.0F, 50.0F}, 300.0F, 8.0F) == std::vector<std::size_t>{0, 1, 2});
  CHECK(flow_rows(std::vector<float>{}, 300.0F, 8.0F).empty());
}

TEST_CASE("Export glTF asks for a file and runs export3d on the window",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  view3d::WindowState window;
  window.cell = "inv_1";
  const hefesto::platform::FilePickerRequest request = export_request(window);
  CHECK(request.mode == hefesto::platform::FilePickerRequest::Mode::save);
  CHECK(request.initial_file == "inv_1.gltf");
  REQUIRE(request.file_types.size() == 3);
  CHECK(request.file_types[0].second == "*.gltf");
  CHECK(request.file_types[1].second == "*.glb");
  CHECK(request.file_types[2].second == "*.stl");
  CHECK(export_command(".render.magic", "/tmp/inv.glb", true) == ".render.magic export3d /tmp/inv.glb");
  CHECK(export_command(".render.magic", "/tmp/inv.glb", false) == "export3d /tmp/inv.glb");
  // A path with spaces or Tcl characters stays one word.
  CHECK(export_command(".render.magic", "/tmp/my cells/inv [1].gltf", true) ==
        ".render.magic export3d {/tmp/my cells/inv [1].gltf}");
  // The picker answers through the shell's plumbing (platform::FilePicker).
  hefesto::platform::ScriptedFilePicker picker;
  picker.push_answer(hefesto::platform::Path{"/tmp/out.stl"});
  const auto file = picker.pick(request);
  REQUIRE(file);
  CHECK(export_command(".render.magic", file->string(), true) == ".render.magic export3d /tmp/out.stl");
}

TEST_CASE("the cut line follows the layout box's long axis and two clicks make an axis-aligned line",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const view3d::Segment wide = box_cut_line(db::Rect{0, 0, 100, 40});
  CHECK(wide.a == db::Point{0, 20});
  CHECK(wide.b == db::Point{100, 20});
  const view3d::Segment tall = box_cut_line(db::Rect{-10, -30, 10, 70});
  CHECK(tall.a == db::Point{0, -30});
  CHECK(tall.b == db::Point{0, 70});
  const view3d::Segment line = axis_aligned_line(db::Point{5, 5}, db::Point{60, 12});
  CHECK(line.a == db::Point{5, 5});
  CHECK(line.b == db::Point{60, 5});
  CHECK(axis_aligned_line(db::Point{5, 5}, db::Point{8, -40}).b == db::Point{5, -40});
  CHECK(segment_length_um(wide, 0.01) == Catch::Approx(1.0));
}

TEST_CASE("the cross-section cuts the layout box in the rendered cell, else the cell",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  ClientSession s;
  open(s);
  REQUIRE(s.editor.box);  // the fixture boxes c3d
  const std::optional<db::Rect> boxed = section_area(s.editor, s.window());
  REQUIRE(boxed);
  CHECK(*boxed == *s.editor.box);
  s.editor.box.reset();
  s.window().cut_box.reset();
  const std::optional<db::Rect> whole = section_area(s.editor, s.window());
  REQUIRE(whole);
  CHECK(*whole == s.editor.cells.find(s.window().cell)->bbox());
}

TEST_CASE("Cross-section: the section panel draws each interval from bottom to top in microns",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const view3d::LayerStack stack = two_layer_stack();
  view3d::CrossSection cs;
  cs.intervals = {{0, 0.5, 1.0, 0.33, 0.51}, {1, 0.0, 2.0, 1.38, 1.74}, {0, 1.5, 2.0, 0.33, 0.51}};
  cs.no_height = {2};
  const SectionPanel panel = section_panel(cs, stack, 2.0, 200.0F, 87.0F);
  REQUIRE(panel.bars.size() == 3);
  // x: 100 px per µm; y: 87 px for the highest top, 1.74 µm (50 px per µm).
  CHECK(panel.bars[0].layer == 0);
  CHECK(panel.bars[0].x == Catch::Approx(50.0F));
  CHECK(panel.bars[0].w == Catch::Approx(50.0F));
  CHECK(panel.bars[0].h == Catch::Approx(0.18F * 50.0F).margin(1e-3));
  CHECK(panel.bars[0].y + panel.bars[0].h == Catch::Approx(87.0F - 0.33F * 50.0F).margin(1e-3));
  CHECK(panel.bars[1].y == Catch::Approx(0.0F).margin(1e-3));  // metal1's top is the panel's top
  CHECK(panel.no_height == std::vector<std::string>{"NOHEIGHT"});
  CHECK(panel.summary == "A–A′ 2.00 µm · 2 layers");
  CHECK(panel.accessible_name ==
        "Cross-section A–A′, 2.00 µm long; POLY 0.33–0.51 µm, 2 intervals; MET1 1.38–1.74 µm, 1 interval; "
        "no height: NOHEIGHT");
  // A hidden layer is left out of the panel.
  const SectionPanel hidden = section_panel(cs, stack, 2.0, 200.0F, 87.0F, {false, true, false});
  CHECK(hidden.bars.size() == 2);
  CHECK(hidden.top_um == Catch::Approx(0.51));
  // Nothing crosses the line and no µm scale.
  const SectionPanel empty = section_panel(view3d::CrossSection{}, stack, 0.0, 200.0F, 87.0F);
  CHECK(empty.bars.empty());
  CHECK(empty.summary == "A–A′ no µm scale · 0 layers");
  CHECK(empty.accessible_name == "Cross-section A–A′, length unknown, no layer crosses the line");
}

TEST_CASE("the cut line's clipping plane keeps the side away from the isometric viewer",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const hefesto::render::SectionPlane along_x = section_clip_plane(box_cut_line(db::Rect{0, 0, 100, 40}));
  CHECK(along_x.axis == hefesto::render::SectionPlane::Axis::y);
  CHECK(along_x.position == 20.0);
  CHECK(along_x.keep_greater);
  const hefesto::render::SectionPlane along_y = section_clip_plane(box_cut_line(db::Rect{-10, -30, 10, 70}));
  CHECK(along_y.axis == hefesto::render::SectionPlane::Axis::x);
  CHECK(along_y.position == 0.0);
  CHECK(along_y.keep_greater);
}

TEST_CASE("a ground point in mask-layer mode is divided back to root coordinates",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  CHECK(ground_to_root(db::Point{100, -40}, 1.0F) == db::Point{100, -40});
  CHECK(ground_to_root(db::Point{100, -40}, 10.0F) == db::Point{10, -4});
  CHECK(ground_to_root(db::Point{100, -40}, 0.0F) == db::Point{100, -40});  // not a valid factor: unchanged
}

TEST_CASE("Cross-section on the tab's mesh: the panel shows the layers the line crosses",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  // A real section (view3d::section, SECTION) of a one-layer model.
  view3d::StackLayer met1;
  met1.name = "MET1";
  met1.height = 1.38F;
  met1.thickness = 0.36F;
  met1.source = view3d::ValueSource::render;
  const view3d::LayerStack stack{view3d::Mode::mask_layer, {met1}, true, 0.01};
  view3d::MeshModel mesh;
  mesh.region = db::Rect{0, 0, 200, 100};
  view3d::Chunk chunk;
  chunk.area = mesh.region;
  chunk.rects.push_back(view3d::RectInstance{50.0F, 0.0F, 150.0F, 100.0F, 0});
  mesh.chunks.push_back(chunk);
  const view3d::Segment line = box_cut_line(mesh.region);
  const view3d::CrossSection cs = view3d::section(mesh, stack, line);
  REQUIRE(cs.intervals.size() == 1);
  const SectionPanel panel = section_panel(cs, stack, cs.length_um, 200.0F, 60.0F);
  REQUIRE(panel.bars.size() == 1);
  CHECK(panel.bars[0].x == Catch::Approx(50.0F).margin(1e-3));
  CHECK(panel.bars[0].w == Catch::Approx(100.0F).margin(1e-3));
  CHECK(panel.summary == "A–A′ 2.00 µm · 1 layer");
}
