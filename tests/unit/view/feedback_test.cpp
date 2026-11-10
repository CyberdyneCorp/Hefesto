// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Feedback areas and the `feedback` command (capability
// ui-feedback-and-highlights; Q-UI-23, Q-UI-24).

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <filesystem>
#include <string>

#include "hefesto/cmd/coordinates.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/polygon.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/view/diag_codes.hpp"
#include "hefesto/view/feedback.hpp"
#include "hefesto/view/highlights.hpp"
#include "view_session.hpp"

namespace db = hefesto::db;
namespace view = hefesto::view;
namespace display = hefesto::display;
using hefesto::test::all_text;
using hefesto::test::ViewSession;

namespace {

/// A session with cell `top` (0,0)-(100,100) shown in one window and edited.
struct FeedbackSession : ViewSession {
  db::CellDef* top = nullptr;
  view::LayoutWindow* win = nullptr;

  FeedbackSession() {
    top = cell("top", {0, 0, 100, 100});
    win = &window(top);
    editor.edit_cell = top;
  }

  void set_box(const db::Rect& r) { editor.box = r; }
};

std::filesystem::path temp_file(const std::string& stem) {
  return std::filesystem::path("/feedback") / (stem + ".txt");
}

/// The session's file system is in memory (PortableOptions::in_memory).
std::string read_file(ViewSession& s, const std::filesystem::path& p) {
  auto text = s.services.view().files.read(p);
  return text.ok() ? text.value() : std::string();
}

bool polygons_available() {
  const std::array<db::Point, 3> tri{db::Point{0, 0}, db::Point{10, 0}, db::Point{0, 10}};
  return db::decompose_polygon(tri).ok();
}

}  // namespace

// --- Feedback area model ---------------------------------------------------------

TEST_CASE("feedback areas round outward from scaled coordinates", "[cap:ui-feedback-and-highlights][req:Feedback area model]") {
  // Scenario "Scaled rounding": f = 20, (-15,5)-(25,41) -> (-1,0)-(2,3).
  CHECK(view::round_feedback_area({-15, 5, 25, 41}, 20) == db::Rect{-1, 0, 2, 3});
  CHECK(view::round_feedback_area({0, 0, 40, 40}, 20) == db::Rect{0, 0, 2, 2});
  CHECK(view::round_feedback_area({-40, -41, -20, -1}, 20) == db::Rect{-2, -3, -1, 0});
  // Clipped to 10 units inside the plane.
  const db::Rect clipped = view::round_feedback_area(db::whole_plane(), 1);
  CHECK(clipped.x0 == db::kPlaneMin + 10);
  CHECK(clipped.x1 == db::kPlaneMax - 10);

  FeedbackSession s;
  REQUIRE(s.editor.feedback.add(s.editor.windows, *s.top, {-15, 5, 25, 41}, 20, {}, "t"));
  const view::FeedbackArea& a = s.editor.feedback.areas().front();
  CHECK(a.root == s.top);
  CHECK(a.scale == 20);
  CHECK(a.scaled == db::Rect{-15, 5, 25, 41});
  CHECK(a.area == db::Rect{-1, 0, 2, 3});
}

TEST_CASE("feedback in a cell shown in no window is dropped", "[cap:ui-feedback-and-highlights][req:Feedback area model]") {
  FeedbackSession s;
  db::CellDef* orphan = s.cell("orphan", {0, 0, 10, 10});
  CHECK_FALSE(s.editor.feedback.add(s.editor.windows, *orphan, {0, 0, 1, 1}, 1, {}, "lost"));
  CHECK(s.editor.feedback.size() == 0);
}

TEST_CASE("feedback in a descendant goes to the displayed root with scaled offsets",
          "[cap:ui-feedback-and-highlights][req:Feedback area model]") {
  FeedbackSession s;
  db::CellDef* leaf = s.cell("leaf", {0, 0, 10, 10});
  db::PlaceOptions opts;
  opts.id = "leaf_0";
  opts.transform = db::translation(30, 40);
  auto placed = db::place(*s.top, *leaf, opts);
  REQUIRE(placed.ok());
  const auto root = view::find_feedback_root(*leaf, s.editor.windows);
  REQUIRE(root.has_value());
  CHECK(root->root == s.top);
  // f = 20: the translation (30,40) becomes (600,800) scaled units.
  REQUIRE(s.editor.feedback.add(s.editor.windows, *leaf, {0, 0, 20, 20}, 20, {}, "in leaf"));
  const view::FeedbackArea& a = s.editor.feedback.areas().front();
  CHECK(a.root == s.top);
  CHECK(a.scaled == db::Rect{600, 800, 620, 820});
  CHECK(a.area == db::Rect{30, 40, 31, 41});
}

TEST_CASE("consecutive feedback areas with the same text share it, numbered in order",
          "[cap:ui-feedback-and-highlights][req:Feedback area model]") {
  FeedbackSession s;
  auto& fb = s.editor.feedback;
  REQUIRE(fb.add(s.editor.windows, *s.top, {0, 0, 1, 1}, 1, {}, "same"));
  REQUIRE(fb.add(s.editor.windows, *s.top, {2, 2, 3, 3}, 1, {}, "same"));
  REQUIRE(fb.add(s.editor.windows, *s.top, {4, 4, 5, 5}, 1, {}, "other"));
  REQUIRE(fb.size() == 3);
  CHECK(fb.areas()[0].text == fb.areas()[1].text);  // one shared instance
  CHECK(fb.areas()[1].text != fb.areas()[2].text);
  CHECK(fb.areas()[2].message() == "other");
}

// --- Clearing feedback ---------------------------------------------------------------

TEST_CASE("feedback clear removes all or the areas containing a text", "[cap:ui-feedback-and-highlights][req:Clearing feedback]") {
  FeedbackSession s;
  for (const char* t : {"width error", "spacing error", "width error"}) {
    REQUIRE(s.editor.feedback.add(s.editor.windows, *s.top, {0, 0, 4, 4}, 1, {}, t));
  }
  // Scenario "Selective clear".
  REQUIRE(s.run("feedback", {"clear", "width"}).ok());
  REQUIRE(s.editor.feedback.size() == 1);
  CHECK(s.editor.feedback.areas()[0].message() == "spacing error");
  // The removed areas are erased from the screen.
  REQUIRE(s.editor.feedback.add(s.editor.windows, *s.top, {50, 50, 60, 60}, 1, {}, "width again"));
  CHECK(s.editor.feedback.clear(std::string_view("width"), &s.editor.highlights) == 1);
  REQUIRE(s.editor.highlights.pending(s.top) != nullptr);
  CHECK(s.editor.highlights.pending(s.top)->front() == db::Rect{50, 50, 60, 60});
  s.editor.highlights.redraw(s.editor);
  REQUIRE(s.editor.highlights.frame(s.win->id()) != nullptr);
  CHECK(s.editor.highlights.frame(s.win->id())->erase.size() == 1);
  REQUIRE(s.run("feedback", {"clear"}).ok());
  CHECK(s.editor.feedback.size() == 0);
}

// --- feedback command -------------------------------------------------------------------

TEST_CASE("feedback add over the box, count and find cycling", "[cap:ui-feedback-and-highlights][req:feedback command]") {
  FeedbackSession s;
  s.set_box({0, 0, 4, 4});
  REQUIRE(s.run("feedback", {"add", "a"}).ok());
  s.set_box({10, 10, 20, 20});
  REQUIRE(s.run("feedback", {"add", "b", "medium"}).ok());
  REQUIRE(s.editor.feedback.size() == 2);
  CHECK(s.editor.feedback.areas()[0].area == db::Rect{0, 0, 4, 4});
  CHECK(s.editor.feedback.areas()[0].style.index == view::kStylePaleHighlights);  // default: pale
  CHECK(s.editor.feedback.areas()[1].style.index == view::kStyleMediumHighlights);

  const auto count = s.run("feedback", {"count"});
  REQUIRE(count.ok());
  CHECK(count.value.to_string() == "2");

  // Scenario "Find cycles": areas 1, 2 and then 1.
  CHECK(all_text(s.run("feedback", {"find"})) == "Feedback #1: a");
  CHECK(s.editor.box == db::Rect{0, 0, 4, 4});
  CHECK(all_text(s.run("feedback", {"find"})) == "Feedback #2: b");
  CHECK(s.editor.box == db::Rect{10, 10, 20, 20});
  CHECK(all_text(s.run("feedback", {"find"})) == "Feedback #1: a");

  const auto out_of_range = s.run("feedback", {"find", "5"});
  REQUIRE(out_of_range.ok());
  REQUIRE(out_of_range.diagnostics.size() == 2);
  CHECK(out_of_range.diagnostics[0].code == view::codes::kFeedbackRange);
  CHECK(out_of_range.diagnostics[0].text == "Sorry, but only feedback areas 1-2 exist.");
  CHECK(out_of_range.diagnostics[1].text == "Feedback #1: a");
}

TEST_CASE("feedback find, why and clear with no areas or arguments", "[cap:ui-feedback-and-highlights][req:feedback command]") {
  FeedbackSession s;
  CHECK(all_text(s.run("feedback", {"find"})) == "There are no feedback areas right now.");
  CHECK(all_text(s.run("feedback")) ==
        "Wrong number of arguments for \"feedback\" command.\nType \":feedback help\" for help.");
  CHECK(all_text(s.run("feedback", {"count", "x"})) ==
        "Wrong number of arguments for \"feedback\" command.\nType \":feedback help\" for help.");
  const auto bad = s.run("feedback", {"bogus"});
  CHECK_FALSE(bad.ok());
  CHECK(bad.diagnostics.back().code == view::codes::kFeedback);
  CHECK(bad.diagnostics.back().text ==
        "bogus isn't a valid feedback option.  Try one of:\n    add        find\n    clear      help\n"
        "    count      save\n    save");
  // Unique prefixes and case-insensitive matching (Magic's Lookup).
  CHECK(s.run("feedback", {"CO"}).ok());
  const auto help = s.run("feedback", {"h"});
  CHECK(all_text(help).starts_with("Feedback commands have the form \"feedback option\",\nwhere option is one of:\n"
                                   "add text [style] [points...]"));
}

TEST_CASE("feedback why prints each text under the box once", "[cap:ui-feedback-and-highlights][req:feedback command]") {
  FeedbackSession s;
  auto& fb = s.editor.feedback;
  REQUIRE(fb.add(s.editor.windows, *s.top, {0, 0, 4, 4}, 1, {}, "first"));
  REQUIRE(fb.add(s.editor.windows, *s.top, {50, 50, 60, 60}, 1, {}, "far"));
  REQUIRE(fb.add(s.editor.windows, *s.top, {1, 1, 2, 2}, 1, {}, "second"));
  REQUIRE(fb.add(s.editor.windows, *s.top, {2, 2, 3, 3}, 1, {}, "first"));
  s.set_box({0, 0, 10, 10});
  CHECK(all_text(s.run("feedback", {"why"})) == "first\nsecond");
}

TEST_CASE("feedback add style argument rules", "[cap:ui-feedback-and-highlights][req:feedback command]") {
  FeedbackSession s;
  s.set_box({0, 0, 4, 4});
  const auto unknown = s.run("feedback", {"add", "x", "bogus"});
  CHECK_FALSE(unknown.ok());
  CHECK(unknown.diagnostics.back().text ==
        "bogus isn't a valid display style.  Try one of:\n    dotted, pale, medium, solid, outline,\n"
        "    or a long name from the .dstyle file");
  CHECK(s.editor.feedback.size() == 0);

  // A numeric third argument starts the point list: (5,1) is a point.
  REQUIRE(s.run("feedback", {"add", "x", "5", "1"}).ok());
  REQUIRE(s.editor.feedback.size() == 1);
  const view::FeedbackArea& p = s.editor.feedback.areas()[0];
  CHECK(p.scale == view::kFeedbackMagnify);
  CHECK(p.scaled == db::Rect{100, 20, 100, 20});
  CHECK(p.style.index == view::kStyleOutlineHighlights);  // pale has no outline: outline highlights

  // An odd number of point coordinates is a usage error.
  CHECK(all_text(s.run("feedback", {"add", "x", "solid", "1", "2", "3"})).starts_with("Wrong number of arguments"));
  // Unique prefix of a named style.
  REQUIRE(s.run("feedback", {"add", "y", "so"}).ok());
  CHECK(s.editor.feedback.areas().back().style.index == view::kStyleSolidHighlights);
}

TEST_CASE("feedback add accepts technology style long names", "[cap:ui-feedback-and-highlights][req:feedback command]") {
  // Scenario "Technology style long name accepted" (Q-UI-23).
  FeedbackSession s;
  s.editor.styles.emplace();
  s.editor.styles->table.styles.resize(61);
  s.editor.styles->table.styles[60].long_name = "mystyle";
  s.set_box({0, 0, 4, 4});
  const auto r = s.run("feedback", {"add", "note", "mystyle"});
  REQUIRE(r.ok());
  CHECK(r.diagnostics.empty());
  REQUIRE(s.editor.feedback.size() == 1);
  CHECK(s.editor.feedback.areas()[0].style.index == 60);
  CHECK(s.editor.feedback.areas()[0].area == db::Rect{0, 0, 4, 4});
}

TEST_CASE("feedback add with two points is a line; angled lines carry the side flags",
          "[cap:ui-feedback-and-highlights][req:feedback command]") {
  FeedbackSession s;
  REQUIRE(s.run("feedback", {"add", "x", "outline", "0", "0", "10", "10"}).ok());
  REQUIRE(s.run("feedback", {"add", "y", "outline", "0", "10", "10", "0"}).ok());
  REQUIRE(s.run("feedback", {"add", "z", "solid", "0", "0", "10", "0"}).ok());
  const auto& a = s.editor.feedback.areas();
  REQUIRE(a.size() == 3);
  CHECK(a[0].scaled == db::Rect{0, 0, 200, 200});
  CHECK(a[0].style == view::FeedbackStyle{view::kStyleOutlineHighlights, false, true, false});
  CHECK(a[1].style == view::FeedbackStyle{view::kStyleOutlineHighlights, false, true, true});
  // A horizontal line has no side flag; solid has no outline: outline highlights.
  CHECK(a[2].style == view::FeedbackStyle{view::kStyleOutlineHighlights});
}

TEST_CASE("feedback add with a polygon splits it into rectangles and triangles",
          "[cap:ui-feedback-and-highlights][req:feedback command]") {
  if (!polygons_available()) {
    SKIP("db::decompose_polygon is EDIT's (task 3.5) and not implemented in this tree yet");
  }
  FeedbackSession s;
  REQUIRE(s.run("feedback", {"add", "p", "solid", "0", "0", "10", "0", "10", "10", "0", "10"}).ok());
  REQUIRE(s.editor.feedback.size() == 1);
  CHECK(s.editor.feedback.areas()[0].area == db::Rect{0, 0, 10, 10});
  REQUIRE(s.run("feedback", {"add", "t", "pale", "0", "0", "10", "0", "0", "10"}).ok());
  REQUIRE(s.editor.feedback.size() == 2);
  CHECK(s.editor.feedback.areas()[1].style.diagonal);
}

TEST_CASE("feedback save writes i-suffixed boxes and escaped texts", "[cap:ui-feedback-and-highlights][req:feedback command]") {
  FeedbackSession s;
  s.set_box({0, 0, 4, 4});
  REQUIRE(s.run("feedback", {"add", "bad \"x\"", "medium"}).ok());
  REQUIRE(s.run("feedback", {"add", "line", "outline", "0", "0", "10", "10"}).ok());
  const auto file = temp_file("hefesto-feedback");
  REQUIRE(s.run("feedback", {"save", file.string()}).ok());
  // Scenario "Save quoting"; an angled line's style is not one of the five names.
  CHECK(read_file(s, file) == "box 0i 0i 4i 4i\nfeedback add \"bad \\\"x\\\"\" medium\n"
                           "box 0i 0i 10i 10i\nfeedback add \"line\"\n");
}

TEST_CASE("saved feedback boxes replay at internal units under any units setting",
          "[cap:ui-feedback-and-highlights][req:feedback command]") {
  // Scenario "Saved feedback replays under other units": lambda = 2 internal
  // units and lambda input; the saved `4i` still means 4 internal units.
  hefesto::cmd::CoordinateUnits units;
  units.internal_per = 2;
  units.lambda_per = 1;
  units.default_unit = hefesto::cmd::DefaultUnit::lambda;
  const auto four = hefesto::cmd::parse_coordinate("4i", units);
  REQUIRE(four.ok());
  CHECK(four.value() == 4.0);
  const auto unsuffixed = hefesto::cmd::parse_coordinate("4", units);
  REQUIRE(unsuffixed.ok());
  CHECK(unsuffixed.value() == 8.0);  // what Magic's suffix-less file would have meant
}

// --- Feedback display ------------------------------------------------------------------------

TEST_CASE("feedback is drawn as lines, triangles, crosses or boxes", "[cap:ui-feedback-and-highlights][req:Feedback display]") {
  FeedbackSession s;
  auto& fb = s.editor.feedback;
  // Scenario "Diagonal line feedback": a single line from (0,0) to (10,10).
  REQUIRE(s.run("feedback", {"add", "x", "outline", "0", "0", "10", "10"}).ok());
  REQUIRE(fb.add(s.editor.windows, *s.top, {20, 20, 30, 30}, 1, {view::kStyleSolidHighlights, true, true, true}, "tri"));
  REQUIRE(fb.add(s.editor.windows, *s.top, {40, 40, 40, 40}, 1, {view::kStyleOutlineHighlights}, "pt"));
  REQUIRE(fb.add(s.editor.windows, *s.top, {50, 50, 60, 60}, 1, {view::kStylePaleHighlights}, "box"));
  display::HighlightScene scene;
  view::draw_feedback_highlights(view::HighlightContext{s.editor, *s.win, s.win->mapping().surface}, scene);
  REQUIRE(scene.items.size() == 4);
  CHECK(scene.items[0].shape == display::HighlightShape::line);
  CHECK(scene.items[0].line == display::SceneSegment{{0, 0}, {10, 10}});
  CHECK(scene.items[1].shape == display::HighlightShape::triangle);
  CHECK(scene.items[1].diagonal == db::Diagonal::nw_se);
  CHECK(scene.items[1].right_side);
  CHECK(scene.items[2].shape == display::HighlightShape::cross);
  CHECK(scene.items[3].shape == display::HighlightShape::box);
  CHECK(scene.items[3].style == view::kStylePaleHighlights);
}

TEST_CASE("feedback converts to pixels with an extra division by its scale",
          "[cap:ui-feedback-and-highlights][req:Feedback display]") {
  view::ViewMapping m = view::ViewMapping::fit({0, 0, 99, 99}, {0, 0, 100, 100});
  view::FeedbackArea a;
  a.scale = 20;
  a.scaled = db::Rect{0, 0, 200, 400};  // (0,0)-(10,20) layout units
  const auto px = view::feedback_screen_rect(a, m);
  REQUIRE(px.has_value());
  CHECK(*px == view::ScreenRect{m.screen_x(0), m.screen_y(0), m.screen_x(10), m.screen_y(20)});
  // Clipped to the window surface except for diagonal styles.
  a.scaled = db::Rect{-20000, 0, 200, 200};
  const auto clipped = view::feedback_screen_rect(a, m);
  REQUIRE(clipped.has_value());
  CHECK(clipped->x0 == m.screen_x(m.surface.x0));
  a.style.diagonal = true;
  const auto diagonal = view::feedback_screen_rect(a, m);
  REQUIRE(diagonal.has_value());
  CHECK(diagonal->x0 < clipped->x0);
  // Out of the window altogether.
  a.style.diagonal = false;
  a.scaled = db::Rect{100000, 100000, 100020, 100020};
  CHECK_FALSE(view::feedback_screen_rect(a, m).has_value());
}

TEST_CASE("new feedback is displayed at the next update", "[cap:ui-feedback-and-highlights][req:Feedback display]") {
  FeedbackSession s;
  REQUIRE(s.editor.feedback.add(s.editor.windows, *s.top, {0, 0, 4, 4}, 1, {}, "new", &s.editor.highlights));
  CHECK(s.editor.highlights.pending(s.top) != nullptr);
  s.editor.highlights.redraw(s.editor);
  const view::HighlightFrame* frame = s.editor.highlights.frame(s.win->id());
  REQUIRE(frame != nullptr);
  bool found = false;
  for (const auto& it : frame->scene.items) {
    found = found || (it.shape == display::HighlightShape::box && it.rect == db::Rect{0, 0, 4, 4});
  }
  CHECK(found);
}

// --- Feedback producers ----------------------------------------------------------------------

TEST_CASE("feedback producers have their default styles", "[cap:ui-feedback-and-highlights][req:Feedback producers]") {
  namespace ps = view::producer_style;
  CHECK(ps::kExtraction.index == view::kStylePaleHighlights);
  CHECK(ps::kExtractionHierarchical.index == view::kStyleMediumHighlights);
  CHECK(ps::kMaskGeneration.index == view::kStylePaleHighlights);
  CHECK(ps::kAntenna.index == view::kStylePaleHighlights);
  CHECK(ps::kPaintError.index == view::kStyleMediumHighlights);
  CHECK(ps::kNetlist.index == view::kStylePaleHighlights);
  // Tile-based displays take the diagonal flags from the tile.
  db::TileView tile;
  tile.split = true;
  tile.diagonal = db::Diagonal::ne_sw;
  tile.right_side = true;
  CHECK(view::tile_feedback_style(view::kStyleSolidHighlights, tile) ==
        view::FeedbackStyle{view::kStyleSolidHighlights, true, true, false});
  CHECK(view::tile_feedback_style(4, db::TileView{}) == view::FeedbackStyle{4});

  // Scenario "Extraction warning": a pale area over the problem in a shown cell.
  FeedbackSession s;
  REQUIRE(s.editor.feedback.add(s.editor.windows, *s.top, {5, 5, 9, 9}, 1, ps::kExtraction, "warning"));
  CHECK(s.editor.feedback.areas()[0].style.index == view::kStylePaleHighlights);
}

TEST_CASE("feedback add and count in scripting mode", "[cap:cmd-misc-edit][req:Feedback command]") {
  // Scenario "Add and count".
  FeedbackSession s;
  s.set_box({0, 0, 4, 4});
  REQUIRE(s.run("feedback", {"add", "check me"}).ok());
  const auto count = s.run("feedback", {"count"});
  REQUIRE(count.ok());
  CHECK(count.value.to_string() == "1");
  // Feedback areas are not part of any cell.
  CHECK(s.top->labels().empty());
  CHECK(s.top->elements().empty());
}
