// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The schematic canvas view model (schematic_canvas.hpp; M8 task 13.2): the
// read-only scene of a document with its symbols placed, the highlighted
// wires and instances, and the viewport's pan, zoom and fit.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "hefesto/schem/session.hpp"
#include "hefesto/shell/schematic_canvas.hpp"

using namespace hefesto::shell;
namespace schem = hefesto::schem;
using Catch::Approx;
using schem::Point;

namespace {

// A symbol with one line from (0,-30) to (0,30), a pin box at the top and
// the text `@name`.
schem::Symbol resistor() {
  schem::Document doc;
  doc.kind = schem::FileKind::symbol;
  doc.lines.push_back(schem::LineRecord{4, Point{0, -30}, Point{0, 30}, {}});
  doc.boxes.push_back(schem::BoxRecord{schem::kPinLayer, Point{-2.5, -32.5}, Point{2.5, -27.5}, "name=P dir=inout"});
  doc.texts.push_back(schem::TextRecord{"@name @value", Point{10, 0}, 0, 0, 0.2, 0.2, {}});
  return schem::make_symbol("r.sym", "/lib/r.sym", std::move(doc));
}

schem::Document two_resistors() {
  schem::Document doc;
  doc.instances.push_back(schem::InstanceRecord{"r.sym", Point{0, 0}, 0, 0, "name=R1 value=1k"});
  doc.instances.push_back(schem::InstanceRecord{"r.sym", Point{100, 0}, 1, 0, "name=R2"});
  doc.wires.push_back(schem::WireRecord{Point{0, 30}, Point{100, 30}, "lab=OUT"});
  doc.wires.push_back(schem::WireRecord{Point{0, -30}, Point{-50, -30}, {}});
  doc.texts.push_back(schem::TextRecord{"title", Point{-60, -60}, 0, 0, 0.4, 0.4, "layer=8"});
  return doc;
}

}  // namespace

TEST_CASE("the canvas draws wires, placed symbols and texts read-only",
          "[cap:schematic-editing][req:Desktop and touch interaction]") {
  const schem::Symbol symbol = resistor();
  const schem::Document doc = two_resistors();
  const schem::Document before = doc;
  const SchematicScene scene = build_scene(doc, [&symbol](const schem::InstanceRecord&) -> const schem::Symbol& {
    return symbol;
  });
  CHECK(doc.instances == before.instances);  // read-only
  CHECK(doc.wires == before.wires);
  CHECK(doc.texts == before.texts);
  CHECK(doc.modified == before.modified);

  REQUIRE(scene.instances.size() == 2);
  CHECK(scene.instances[0].name == "R1");
  CHECK(scene.instances[1].name == "R2");
  CHECK_FALSE(scene.instances[0].placeholder);

  // 2 wires + one symbol line per instance.
  REQUIRE(scene.segments.size() == 4);
  CHECK(scene.segments[0].source == SceneSource::wire);
  CHECK(scene.segments[2].source == SceneSource::symbol);
  // R2 is rotated once (90 degrees): its vertical line becomes horizontal.
  const SceneSegment& r2 = scene.segments[3];
  CHECK(r2.index == 1);
  CHECK(r2.a.y == Approx(r2.b.y));
  CHECK(r2.a.x != Approx(r2.b.x));

  // The pin box is a filled pin polygon.
  REQUIRE(scene.polygons.size() == 2);
  CHECK(scene.polygons[0].source == SceneSource::pin);
  CHECK(scene.polygons[0].filled);

  // Texts: the document's own, then each instance's with @tokens replaced
  // (an attribute the instance does not set stays as written).
  REQUIRE(scene.texts.size() == 3);
  CHECK(scene.texts[0].text == "title");
  CHECK(scene.texts[0].layer == 8);
  CHECK(scene.texts[1].text == "R1 1k");
  CHECK(scene.texts[2].text == "R2 @value");

  CHECK(scene.box.x1 <= -60.0);
  CHECK(scene.box.x2 >= 130.0);
  CHECK(scene.instance_at(Point{0, 0}) == 0);
  CHECK(scene.instance_at(Point{500, 500}) == static_cast<std::size_t>(-1));
}

TEST_CASE("highlighted nets and instances are marked in the scene", "[cap:schematic-editing][req:Net highlighting]") {
  const schem::Symbol symbol = resistor();
  const auto lookup = [&symbol](const schem::InstanceRecord&) -> const schem::Symbol& { return symbol; };
  const SchematicScene plain = build_scene(two_resistors(), lookup);
  CHECK_FALSE(plain.segments[0].highlighted);
  const SchematicScene lit = build_scene(two_resistors(), lookup, SceneHighlight{{"OUT"}, {"R2"}});
  CHECK(lit.segments[0].highlighted);       // lab=OUT
  CHECK_FALSE(lit.segments[1].highlighted);  // no lab
  CHECK_FALSE(lit.instances[0].highlighted);
  CHECK(lit.instances[1].highlighted);
}

TEST_CASE("the session lookup draws the system library's symbols",
          "[cap:schematic-symbols-and-libraries][req:System library]") {
  hefesto::platform::LocalFileSystem fs;
  const std::string devices = std::string{HEFESTO_SOURCE_DIR} + "/assets/schematic/xschem_library/devices";
  const hefesto::compat::CompatSettings compat;
  schem::Session session;
  session.files = &fs;
  session.compat = &compat;
  session.search_path = schem::SearchPath::parse(devices, std::map<std::string, std::string>{}, fs);
  schem::Document doc;
  doc.path = "/work/top.sch";
  doc.instances.push_back(schem::InstanceRecord{"res.sym", Point{0, 0}, 0, 0, "name=R1 value=1k"});
  doc.instances.push_back(schem::InstanceRecord{"nosuch.sym", Point{200, 0}, 0, 0, "name=X1"});
  const SchematicScene scene = build_scene(doc, session_symbol_lookup(session, doc, nullptr));
  REQUIRE(scene.instances.size() == 2);
  CHECK_FALSE(scene.instances[0].placeholder);
  CHECK(scene.instances[1].placeholder);  // the missing-symbol placeholder
  CHECK_FALSE(scene.segments.empty());

  // Without a file system the lookup gives the placeholder.
  schem::Session empty;
  const SchematicScene none = build_scene(doc, session_symbol_lookup(empty, doc, nullptr));
  CHECK(none.instances[0].placeholder);
}

TEST_CASE("the viewport pans, zooms about a point and fits",
          "[cap:schematic-editing][req:Desktop and touch interaction]") {
  SchematicViewport view;
  view.set_size(800, 600);
  CHECK(view.to_canvas(Point{0, 0}).x == Approx(400));
  view.pan_by(100, 50);
  CHECK(view.to_canvas(Point{0, 0}).x == Approx(500));
  CHECK(view.to_canvas(Point{0, 0}).y == Approx(350));

  const CanvasPoint at{200, 100};
  const Point under = view.to_schematic(at);
  view.zoom_at(2.0, at);
  CHECK(view.zoom() == Approx(2.0));
  CHECK(view.to_schematic(at).x == Approx(under.x));
  CHECK(view.to_schematic(at).y == Approx(under.y));
  view.zoom_at(1e9, at);
  CHECK(view.zoom() == Approx(SchematicViewport::kMaxZoom));
  view.zoom_at(0.0, at);  // ignored
  CHECK(view.zoom() == Approx(SchematicViewport::kMaxZoom));

  SceneBox box;
  box.add(Point{-100, -50});
  box.add(Point{300, 150});
  view.fit(box);
  CHECK(view.centre().x == Approx(100));
  CHECK(view.centre().y == Approx(50));
  const CanvasPoint corner = view.to_canvas(Point{-100, -50});
  CHECK(corner.x >= 0.0);
  CHECK(corner.y >= 0.0);
  CHECK(view.to_canvas(Point{300, 150}).x <= 800.0);
  SchematicViewport unsized;
  unsized.fit(box);  // no size: unchanged
  CHECK(unsized.zoom() == Approx(1.0));
}

// Regression (M8 round 7, CI capture review of round 6): canvas texts kept
// one pixel size whatever the zoom, so a fitted test bench drew its labels
// over each other. Their size now follows the zoom.
TEST_CASE("canvas texts scale with the zoom and the fit",
          "[cap:schematic-editing][req:Desktop and touch interaction]") {
  const auto at_one = scene_text_px(0.4, 1.0);
  REQUIRE(at_one.has_value());
  CHECK(*at_one == Approx(0.4 * kTextUnitHeight));
  const auto zoomed_out = scene_text_px(0.4, 0.5);
  REQUIRE(zoomed_out.has_value());
  CHECK(*zoomed_out == Approx(*at_one / 2.0));
  // The record's scale counts; a scale of 0 is the default text scale.
  CHECK(*scene_text_px(0.2, 1.0) == Approx(*at_one / 2.0));
  CHECK(*scene_text_px(0.0, 1.0) == Approx(*at_one));
  // Too small to read: not drawn; very large: capped.
  CHECK_FALSE(scene_text_px(0.4, SchematicViewport::kMinZoom).has_value());
  CHECK(*scene_text_px(0.4, SchematicViewport::kMaxZoom) == Approx(kMaxTextPx));

  // A fit of a large drawing into a small canvas shrinks the texts with it.
  SchematicViewport view;
  view.set_size(800, 600);
  SceneBox box;
  box.add(Point{0, 0});
  box.add(Point{2000, 1500});
  view.fit(box);
  const auto fitted = scene_text_px(0.4, view.zoom());
  REQUIRE(fitted.has_value());
  CHECK(*fitted < *at_one);
}
