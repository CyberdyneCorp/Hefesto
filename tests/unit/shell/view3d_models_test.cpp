// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The 3D view's view models (hefesto/shell/view3d_models.hpp; m10-3d-view
// tasks 9.1, 9.2, 9.6): the layer stack panel and its sync with `see`, the
// commands of every control, focus routing, gestures and keys, picking
// through a fake resolver, the frame's DrawParams, the MeshWorker inputs,
// the mesh status and the frame-time readout. Headless: no GPU, no Raven.
// The `wind3d` client session is WIND3D's test fixture. Owner: SHELL.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "../view3d/client/client_fixture.hpp"
#include "hefesto/gui/wrapper.hpp"
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

/// The window on c3d with nwell, poly, metal1, via1 and metal2 bound to styles.
void open_ghost(ClientSession& s) {
  bind_styles(s);
  auto& types = s.editor.styles->types.styles_of_type;
  types[*s.editor.technology->find_type("nwell").id] = {hefesto::display::kFixedStyleCount + 5};
  types[*s.editor.technology->find_type("via1").id] = {hefesto::display::kFixedStyleCount + 22};
  s.paint("nwell", db::Rect{-10, -10, 110, 60});
  s.paint("via1", db::Rect{30, 20, 40, 30});
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

TEST_CASE("no default ghosting in mask-layer mode",
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

#ifdef HEFESTO_TEST_RENDER_ROUND4  // until RENDER m10-3d-round4-render is merged
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

TEST_CASE("Board look with magic-compat ON: the desktop tab draws with the board lighting",
          "[cap:view-3d][req:Viewing transform and shading]") {
  ClientSession s{true};
  open(s);
  const view3d::LayerStack stack = view3d::layout_stack(s.editor, true);
  REQUIRE(stack.magic_compat());
  const View3dOptions options;
  CHECK(draw_params(FrameInputs{&s.window(), &stack, nullptr, nullptr, &options}).lighting ==
        hefesto::render::Lighting::board);
}

TEST_CASE("Board look with magic-compat OFF: the desktop tab draws with the same board lighting",
          "[cap:view-3d][req:Viewing transform and shading]") {
  ClientSession s{false};
  open(s);
  const view3d::LayerStack stack = view3d::layout_stack(s.editor, false);
  REQUIRE_FALSE(stack.magic_compat());
  const View3dOptions options;
  CHECK(draw_params(FrameInputs{&s.window(), &stack, nullptr, nullptr, &options}).lighting ==
        hefesto::render::Lighting::board);
}

#endif  // RENDER round 4

TEST_CASE("Isometric default from above: opening the tab runs the framed isometric preset once per window",
          "[cap:view-3d][req:Viewing transform and shading]") {
  ClientSession s;
  open(s);
  DefaultCameraLatch latch;
  const auto apply_default = [&s] {
    view3d::ViewState camera = s.window().view;
    const view3d::Angles iso = view3d::preset(view3d::Preset::isometric);
    camera.view_x = iso.x;
    camera.view_y = iso.y;
    camera.view_z = iso.z;
    camera.scale_xy *= 0.5F;  // stands for the framing's zoom
    camera.trans_x = -48.0F;
    const std::vector<std::string> lines = camera_commands(".magic3d", camera);
    REQUIRE(lines.size() == 3);
    CHECK(lines[0].rfind(".magic3d view ", 0) == 0);
    CHECK(lines[1].rfind(".magic3d scroll ", 0) == 0);
    CHECK(lines[2].rfind(".magic3d zoom ", 0) == 0);
    for (const std::string& line : lines) tcl(s, line);
    return camera;
  };
  REQUIRE(latch.take(true));
  const view3d::ViewState framed = apply_default();
  // `view`, `scroll` and `zoom` report what the user sees.
  CHECK(s.window().view.view_x == framed.view_x);
  CHECK(s.window().view.view_z == framed.view_z);
  CHECK(s.window().view.trans_x == framed.trans_x);
  CHECK(s.window().view.scale_xy == framed.scale_xy);
  CHECK(s.window().view.scale_z == framed.scale_z);
  // A later sync keeps the user's camera.
  tcl(s, ".magic3d view 10 0 20");
  CHECK_FALSE(latch.take(true));
  CHECK(s.window().view.view_x == 10.0F);
  // closewindow re-arms it: a reopened window gets the default again.
  tcl(s, close_command(".magic3d"));
  CHECK_FALSE(latch.take(s.editor.view3d.has_window()));
  open(s);
  CHECK(latch.take(true));
}

TEST_CASE("a script-opened window keeps the top-down default view outside the desktop tab",
          "[cap:view-3d][req:Default view]") {
  ClientSession s;
  open(s);
  CHECK(s.window().view.view_x == 0.0F);
  CHECK(s.window().view.view_y == 0.0F);
  CHECK(s.window().view.view_z == 0.0F);
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
