// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Change-driven redisplay, grow, specialopen and instance-addressed expand
// (capability ui-windows-and-views).

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/view/commands.hpp"
#include "hefesto/view/diag_codes.hpp"
#include "hefesto/view/window.hpp"

namespace cmd = hefesto::cmd;
namespace db = hefesto::db;
namespace tech = hefesto::tech;
namespace view = hefesto::view;

namespace {

db::CellUse* add_use(db::CellDef& parent, db::CellDef& child, const std::string& id, const db::Transform& t,
                     const db::Rect& bbox) {
  auto use = std::make_unique<db::CellUse>();
  use->id = id;
  use->def = &child;
  use->parent = &parent;
  use->transform = t;
  use->bbox = bbox;
  db::CellUse* raw = use.get();
  parent.uses().push_back(std::move(use));
  child.instances().push_back(raw);
  return raw;
}

tech::TypeMask mask_of(tech::TileType t) {
  return tech::TypeMask::of(t);
}

struct Recorder final : view::ViewObserver {
  std::vector<std::pair<view::WindowId, view::Change>> seen;
  void on_window_changed(view::WindowId id, view::Change change) override { seen.emplace_back(id, change); }
};

struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;

  Session() {
    REQUIRE(view::register_view_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
  }

  cmd::CommandResult run(std::string name, std::vector<std::string> args = {}) {
    cmd::Invocation inv;
    inv.name = std::move(name);
    inv.args = std::move(args);
    return dispatcher->execute(inv);
  }

  db::CellDef* cell(const std::string& name, const db::Rect& bbox) {
    db::CellDef* def = editor.cells.create(name, editor.technology->plane_count()).value();
    def->set_bboxes(bbox, bbox);
    def->flags().available = true;
    return def;
  }
};

std::string last_text(const cmd::CommandResult& r) {
  return r.diagnostics.empty() ? std::string() : r.diagnostics.back().text;
}

}  // namespace

#define REDISPLAY "[cap:ui-windows-and-views][req:Change-driven redisplay]"

TEST_CASE("a change in the root marks every window showing it", REDISPLAY) {
  db::CellDef top("top", 8);
  db::CellDef other("other", 8);
  view::WindowManager wm;
  auto* w0 = wm.open(&top, {0, 0, 99, 99}).value();
  auto* w1 = wm.open(&top, {0, 0, 99, 99}).value();
  auto* w2 = wm.open(&other, {0, 0, 99, 99}).value();
  (void)w0->take_redisplay();
  w0->move_to({0, 0, 100, 100});
  wm.area_changed(top, {10, 10, 20, 20}, mask_of(tech::kFirstUserType));
  CHECK(w0->pending_redisplay().areas == std::vector<db::Rect>{{10, 10, 20, 20}});
  CHECK(w1->pending_redisplay().areas == std::vector<db::Rect>{{10, 10, 20, 20}});
  CHECK(w2->pending_redisplay().empty());
  // WindSurfaceToScreen of the area, inside the screen area.
  const view::ViewMapping& m = w0->mapping();
  REQUIRE(w0->pending_redisplay().screen.size() == 1);
  CHECK(w0->pending_redisplay().screen[0] ==
        view::ScreenRect{m.screen_x(10), m.screen_y(10), m.screen_x(20), m.screen_y(20)});
  // A zero-width area changes nothing (DBWAreaChanged returns at once).
  wm.area_changed(other, {5, 0, 5, 10}, std::nullopt);
  CHECK(w2->pending_redisplay().empty());
  // take_redisplay hands the areas over once.
  CHECK(w0->take_redisplay().areas.size() == 1);
  CHECK(w0->pending_redisplay().empty());
}

TEST_CASE("a change in a child reaches only windows where its instance is expanded", REDISPLAY) {
  db::CellDef top("top", 8);
  db::CellDef inv("inv", 8);
  inv.set_bboxes({0, 0, 10, 10}, {0, 0, 10, 10});
  db::CellUse* u1 = add_use(top, inv, "u1", db::translation(100, 0), {100, 0, 110, 10});
  view::WindowManager wm;
  auto* shut = wm.open(&top, {0, 0, 99, 99}).value();
  auto* open = wm.open(&top, {0, 0, 99, 99}).value();
  open->set_expanded(*u1, true);
  wm.area_changed(inv, {1, 2, 3, 4}, mask_of(tech::kFirstUserType));
  CHECK(open->pending_redisplay().areas == std::vector<db::Rect>{{101, 2, 103, 4}});
  CHECK(shut->pending_redisplay().empty());
}

TEST_CASE("a window where no changed layer is visible skips the change", REDISPLAY) {
  db::CellDef top("top", 8);
  view::WindowManager wm;
  auto* hidden = wm.open(&top, {0, 0, 99, 99}).value();
  auto* shown = wm.open(&top, {0, 0, 99, 99}).value();
  const auto metal1 = static_cast<tech::TileType>(tech::kFirstUserType + 1);
  tech::TypeMask visible = hidden->visible_types();
  visible.set(metal1, false);
  hidden->set_visible_types(visible);
  wm.area_changed(top, {0, 0, 4, 4}, mask_of(metal1));
  CHECK(hidden->pending_redisplay().empty());
  CHECK_FALSE(shown->pending_redisplay().empty());
  // nullopt (labels may have changed) always counts.
  wm.area_changed(top, {0, 0, 4, 4}, std::nullopt);
  CHECK_FALSE(hidden->pending_redisplay().empty());
}

TEST_CASE("an array maps a small change per element and a large one to the span", REDISPLAY) {
  db::CellDef top("top", 8);
  db::CellDef bit("bit", 8);
  bit.set_bboxes({0, 0, 10, 10}, {0, 0, 10, 10});
  db::CellUse* arr = add_use(top, bit, "a", db::kIdentity, {0, 0, 30, 10});
  arr->array = db::ArrayInfo{0, 2, 0, 0, 10, 0};
  view::WindowManager wm;
  auto* w = wm.open(&top, {0, 0, 99, 99}).value();
  w->set_expanded(*arr, true);
  wm.area_changed(bit, {1, 1, 3, 3}, mask_of(tech::kFirstUserType));
  CHECK(w->take_redisplay().areas == std::vector<db::Rect>{{1, 1, 3, 3}, {11, 1, 13, 3}, {21, 1, 23, 3}});
  wm.area_changed(bit, {0, 0, 8, 3}, mask_of(tech::kFirstUserType));  // wider than half the cell
  CHECK(w->take_redisplay().areas == std::vector<db::Rect>{{0, 0, 28, 3}});
}

TEST_CASE("label changes grow the screen area by the label extent", REDISPLAY) {
  db::CellDef top("top", 8);
  view::WindowManager wm;
  auto* w = wm.open(&top, {0, 0, 99, 99}).value();
  w->move_to({0, 0, 100, 100});
  w->include_label_extent({-3, -2, 5, 4});
  w->include_label_extent({-1, -1, 1, 1});  // never shrinks
  CHECK(w->label_extent() == view::ScreenRect{-3, -2, 5, 4});
  wm.area_changed(top, {10, 10, 20, 20}, mask_of(tech::kFirstUserType));
  wm.area_changed(top, {10, 10, 20, 20}, std::nullopt);
  const view::Redisplay r = w->take_redisplay();
  REQUIRE(r.screen.size() == 2);
  CHECK(r.screen[1].x0 == r.screen[0].x0 - 3);
  CHECK(r.screen[1].y1 == r.screen[0].y1 + 4);
}

TEST_CASE("redisplay is performed at the update, not while suspended", REDISPLAY) {
  db::CellDef top("top", 8);
  view::WindowManager wm;
  Recorder rec;
  auto* w = wm.open(&top, {0, 0, 99, 99}).value();
  wm.add_observer(rec);
  wm.area_changed(top, {0, 0, 4, 4}, std::nullopt);
  CHECK(rec.seen.empty());  // nothing during the command
  wm.set_display_suspended(true);
  wm.update_display();
  CHECK(rec.seen.empty());
  wm.set_display_suspended(false);
  wm.update_display();
  REQUIRE(rec.seen.size() == 1);
  CHECK(rec.seen[0] == std::pair{w->id(), view::Change::content});
  wm.redisplay_all();
  CHECK(w->pending_redisplay().all);
  wm.area_changed(top, {0, 0, 4, 4}, std::nullopt);
  CHECK(w->pending_redisplay().areas.empty());  // the whole window covers it
  wm.remove_observer(rec);
}

TEST_CASE("the tracker redraws everything after commands that do not report", REDISPLAY) {
  CHECK(view::reports_own_redisplay("paint"));
  CHECK(view::reports_own_redisplay("zoom"));
  CHECK_FALSE(view::reports_own_redisplay("load"));
  db::CellDef top("top", 8);
  view::WindowManager wm;
  auto* w = wm.open(&top, {0, 0, 99, 99}).value();
  (void)w->take_redisplay();
  view::RedisplayTracker tracker(wm);
  cmd::Invocation inv;
  inv.name = "paint";
  tracker.on_command(inv, cmd::CommandResult::success());
  CHECK(w->pending_redisplay().empty());
  inv.name = "load";
  tracker.on_command(inv, cmd::CommandResult::success());
  CHECK(w->pending_redisplay().all);
  (void)w->take_redisplay();
  inv.name = "paint";  // an edit that failed part-way
  cmd::CommandResult failed = cmd::CommandResult::success();
  failed.status = cmd::Status::failed;
  tracker.on_command(inv, failed);
  CHECK(w->pending_redisplay().all);
}

TEST_CASE("redraw marks every window; see marks its window", REDISPLAY) {
  Session s;
  db::CellDef* top = s.cell("top", {0, 0, 10, 10});
  auto* w0 = s.editor.windows.open(top, {0, 0, 99, 99}).value();
  auto* w1 = s.editor.windows.open(top, {0, 0, 99, 99}).value();
  REQUIRE(s.run("redraw").ok());
  CHECK(w0->take_redisplay().all);
  CHECK(w1->take_redisplay().all);
  REQUIRE(s.run("see", {"no", "labels"}).ok());
  CHECK(w1->take_redisplay().all);  // the current window
  CHECK(w0->pending_redisplay().empty());
}

#define GROW "[cap:ui-windows-and-views][req:specialopen, over, underneath and grow]"

TEST_CASE("grow toggles full screen and back to the old frame and depth", GROW) {
  Session s;
  db::CellDef* top = s.cell("top", {0, 0, 10, 10});
  view::WindowManager& wm = s.editor.windows;
  auto* back = wm.open_frame(top, {10, 10, 210, 210}).value();
  REQUIRE(wm.open_frame(top, {300, 10, 500, 210}).ok());
  wm.set_current(back->id());
  const db::Rect shown = back->mapping().surface;
  REQUIRE(s.run("grow").ok());
  CHECK(back->full_screen());
  CHECK(back->frame() == view::kNullScreen);
  CHECK(back->mapping().screen == view::inner_area(view::kNullScreen, back->chrome(), wm.chrome_metrics()));
  std::vector<view::WindowId> order;
  wm.for_each([&order](view::LayoutWindow& w) { order.push_back(w.id()); });
  CHECK(order.front() == back->id());  // on top
  CHECK(view::ViewMapping::fit(back->mapping().screen, shown).surface == back->mapping().surface);
  REQUIRE(s.run("grow").ok());
  CHECK_FALSE(back->full_screen());
  CHECK(back->frame() == view::ScreenRect{10, 10, 210, 210});
  order.clear();
  wm.for_each([&order](view::LayoutWindow& w) { order.push_back(w.id()); });
  CHECK(order.back() == back->id());  // its old depth
  REQUIRE(s.run("closewindow").ok());
  REQUIRE(s.run("closewindow").ok());
  CHECK(last_text(s.run("grow")) == "Point to a window first.");
}

TEST_CASE("specialopen opens layout windows and lists the window types", GROW) {
  Session s;
  db::CellDef* c = s.cell("c", {0, 0, 10, 10});
  s.editor.edit_cell = c;
  REQUIRE(s.run("specialopen", {"10", "20", "400", "300", "layout", "c"}).ok());
  view::LayoutWindow* w = s.editor.windows.current();
  REQUIRE(w != nullptr);
  CHECK(w->root() == c);
  CHECK(w->frame() == view::ScreenRect{10, 20, 400, 300});
  // Width and height raised to the minimum window size.
  REQUIRE(s.run("specialopen", {"0", "0", "5", "5", "la"}).ok());
  const view::ChromeMetrics& m = s.editor.windows.chrome_metrics();
  CHECK(s.editor.windows.current()->frame() ==
        view::ScreenRect{0, 0, 6 * m.border + 3 * m.scrollbar + 25, m.caption + 4 * m.border + 3 * m.scrollbar + 25});
  REQUIRE(s.run("specialopen", {"layout"}).ok());  // 300x300 centred
  CHECK(s.editor.windows.size() == 3);

  for (const std::vector<std::string>& args : std::vector<std::vector<std::string>>{
           {}, {"*window"}, {"foo"}, {"1", "2", "layout"}, {"1", "2", "x", "4", "layout"}, {""}}) {
    const auto r = s.run("specialopen", args);
    REQUIRE_FALSE(r.ok());
    REQUIRE(r.diagnostics.size() == 2);
    CHECK(r.diagnostics[0].text == "Usage: specialopen [leftx bottomy rightx topy] type [args]\nValid window types are:");
    CHECK(r.diagnostics[1].code == view::codes::kUsage);
    CHECK(r.diagnostics[1].text == "\tnetlist\n\tcolor\n\tlayout");
  }
  const auto color = s.run("specialopen", {"color", "12"});
  REQUIRE_FALSE(color.ok());
  CHECK(color.diagnostics.back().code == view::codes::kClient);
  CHECK(s.editor.windows.size() == 3);
}

#define EXPAND "[cap:ui-windows-and-views][req:Instance-addressed expansion]"

TEST_CASE("expand instance acts on one instance by path, without the box", EXPAND) {
  Session s;
  db::CellDef* top = s.cell("top", {0, 0, 200, 200});
  db::CellDef* mid = s.cell("mid", {0, 0, 50, 50});
  db::CellDef* leaf = s.cell("leaf", {0, 0, 50, 50});
  db::CellUse* l1 = add_use(*mid, *leaf, "L1", db::kIdentity, {0, 0, 50, 50});  // covers its parent
  db::CellUse* x1 = add_use(*top, *mid, "X1", db::translation(100, 100), {100, 100, 150, 150});
  db::CellUse* arr = add_use(*top, *mid, "A", db::kIdentity, {0, 0, 50, 100});
  arr->array = db::ArrayInfo{0, 0, 0, 1, 0, 50};
  auto* other = s.editor.windows.open(top, {0, 0, 99, 99}).value();
  auto* w = s.editor.windows.open(top, {0, 0, 99, 99}).value();
  (void)w->take_redisplay();
  s.editor.box.reset();

  REQUIRE(s.run("expand", {"instance", "X1"}).ok());
  CHECK(w->is_expanded(*x1));
  CHECK_FALSE(w->is_expanded(*l1));      // one level
  CHECK_FALSE(other->is_expanded(*x1));  // this window only
  CHECK_FALSE(s.editor.box.has_value());
  CHECK(w->take_redisplay().areas == std::vector<db::Rect>{{100, 100, 150, 150}});
  CHECK(other->pending_redisplay().empty());

  REQUIRE(s.run("expand", {"instance", "X1/L1"}).ok());
  CHECK(w->is_expanded(*l1));
  REQUIRE(s.run("expand", {"instance", "A[0,1]/L1", "tog"}).ok());  // subscripts name the array
  CHECK_FALSE(w->is_expanded(*l1));                                   // toggled back
  REQUIRE(s.run("unexpand", {"instance", "X1"}).ok());
  CHECK_FALSE(w->is_expanded(*x1));
  REQUIRE(s.run("expand", {"instance", "X1", "t"}).ok());
  CHECK(w->is_expanded(*x1));

  const auto missing = s.run("expand", {"instance", "X1/nope"});
  REQUIRE_FALSE(missing.ok());
  CHECK(missing.diagnostics.back().code == view::codes::kInstance);
  CHECK(last_text(missing) == "Instance X1/nope not found in this window.");
  CHECK(s.run("unexpand", {"instance", "X1", "toggle"}).diagnostics.back().code == view::codes::kUsage);
  // Magic's forms are unchanged: `expand instance` alone is a bad mode.
  CHECK(s.run("expand", {"instance"}).diagnostics.back().code == view::codes::kUsage);
}
