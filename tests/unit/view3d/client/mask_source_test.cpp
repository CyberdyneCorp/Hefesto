// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The mask-layer source of the 3D window (tasks 10.1-10.3; design D15):
// window_build and build_scene in mask-layer mode, the generation area and
// halo, the Q-MISC-72 heights in both modes, the mask visibility set
// (Q-MISC-71), the face log (N3), incremental regeneration and the PDK
// stacks. Owner: MASKSRC.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "client_fixture.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/mask/gen/generate.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/view3d/client/face_log.hpp"
#include "hefesto/view3d/client/scene.hpp"
#include "hefesto/view3d/mesh/worker.hpp"
#include "reference_data.hpp"

namespace view3d = hefesto::view3d;
namespace display = hefesto::display;
namespace db = hefesto::db;
using view3d::Mode;
using view3d::test::ClientSession;

namespace {

// The MISC-3D technology with a halo: NWELL is metal1 grown by 50 mask
// units (the style's radius is 11 internal units) through the temporary layer HALO; `micron` has
// 0.01 µm per internal unit (scalefactor 10 nanometers reduces to f = 1).
constexpr std::string_view kMaskTech = R"(tech
    format 35
    v3dmask
end
planes
    active,a
    metal1,m1
    metal2,m2
end
types
    active  polysilicon,poly
    metal1  metal1,m1
    metal2  metal2,m2
end
styles
    styletype mos
    poly 1
    metal1 20
    metal2 21
end
contact
end
compose
end
connect
    poly    poly
    metal1  metal1
    metal2  metal2
end
cifoutput
style gdsii
    scalefactor 10
    layer POLY poly
        calma 66 20
    layer MET1 m1
        calma 68 20
    layer MET2 m2
        calma 69 20
    templayer HALO m1
        grow 50
    layer NWELL HALO
        calma 64 20
    render MET1 metal1 1.0 0.5
    render MET2 metal2 2.0 0.5
style micron
    scalefactor 10 nanometers
    layer MET1 m1
        calma 68 20
    render MET1 metal1 1.376 0.36
end
cifinput
end
drc
end
extract
style default
    lambda 1
    step 100
    sidehalo 0
    planeorder active 0
    planeorder metal1 1
    planeorder metal2 2
    height poly 0.2 0.1
    height metal1 0.5 0.3
end
)";

// Mask layers of `gdsii` in style order.
constexpr view3d::LayerId kMet1 = 1;
constexpr view3d::LayerId kMet2 = 2;
constexpr view3d::LayerId kHalo = 3;
constexpr view3d::LayerId kNwell = 4;

constexpr std::size_t kMetal1Style = display::kFixedStyleCount + 20;
constexpr std::size_t kCrossStyle = display::kFixedStyleCount + 21;

// A session on kMaskTech whose cell c3d holds metal1 (0,0)-(60,50) and
// (108,0)-(120,50) only.
struct MaskSession : ClientSession {
  explicit MaskSession(bool magic_compat) : ClientSession(magic_compat, kMaskTech) {
    clear();
    paint("metal1", db::Rect{0, 0, 60, 50});
    paint("metal1", db::Rect{108, 0, 120, 50});
  }

  void clear() {
    for (const std::string_view type : {"metal1", "metal2"}) {
      const auto t = editor.technology->find_type(type);
      REQUIRE(hefesto::db::erase_type(*cell, *editor.technology, db::Rect{-1000, -1000, 1000, 1000}, *t.id).ok());
    }
    hefesto::db::recompute_bbox(*cell);
  }

  view3d::Scene scene() {
    view3d::BuildContext ctx;
    return view3d::build_scene(editor, window(), project.compat.magic_compat, ctx);
  }
};

// metal1's long name is `metal1`, metal2's a cross style (`contact_X`).
display::StyleSet style_set() {
  display::StyleSet set;
  set.table.styles.resize(kCrossStyle + 1);
  set.table.layout_count = kCrossStyle + 1 - display::kFixedStyleCount;
  set.table.styles[kMetal1Style].long_name = "metal1";
  set.table.styles[kCrossStyle].long_name = "contact_X";
  set.table.styles[kCrossStyle].fill = display::FillKind::cross;
  return set;
}

struct Extent {
  double x0 = std::numeric_limits<double>::max();
  double y0 = std::numeric_limits<double>::max();
  double x1 = std::numeric_limits<double>::lowest();
  double y1 = std::numeric_limits<double>::lowest();
  double area = 0.0;
  int rects = 0;
  int sides = 0;
};

// The top faces of `layer` in mesh coordinates (no split tiles here).
Extent extent_of(const view3d::MeshModel& mesh, view3d::LayerId layer) {
  Extent e;
  for (const view3d::Chunk& c : mesh.chunks) {
    for (const view3d::RectInstance& r : c.rects) {
      if (r.layer != layer) {
        continue;
      }
      const double x0 = c.origin_x + static_cast<double>(r.x0);
      const double y0 = c.origin_y + static_cast<double>(r.y0);
      const double x1 = c.origin_x + static_cast<double>(r.x1);
      const double y1 = c.origin_y + static_cast<double>(r.y1);
      e.x0 = std::min(e.x0, x0);
      e.y0 = std::min(e.y0, y0);
      e.x1 = std::max(e.x1, x1);
      e.y1 = std::max(e.y1, y1);
      e.area += (x1 - x0) * (y1 - y0);
      ++e.rects;
    }
    e.sides += static_cast<int>(std::count_if(c.sides.begin(), c.sides.end(),
                                              [layer](const view3d::SideInstance& s) { return s.layer == layer; }));
  }
  return e;
}

// A headless editor on a PDK technology (its current output style) with a
// cell c3d holding metal1 (0,0)-(100,50), and a 3D window on it in
// mask-layer mode.
struct PdkSession {
  hefesto::editor::EditorState editor;
  view3d::WindowState window;

  explicit PdkSession(const std::filesystem::path& file) {
    hefesto::platform::LocalFileSystem files;
    hefesto::diag::NullSink sink;
    auto loaded = hefesto::tech::load_technology(files, file.string(), hefesto::tech::LoadOptions{}, sink);
    REQUIRE(loaded.ok());
    editor.technology = loaded.value();
    editor.mask.output_styles = hefesto::mask::read_output_styles(*editor.technology, sink);
    REQUIRE(editor.mask.output_styles.current_style() != nullptr);
    db::CellDef* cell = editor.cells.create("c3d", editor.technology->plane_count()).value();
    cell->flags().available = true;
    const auto metal1 = editor.technology->find_type("metal1");
    REQUIRE(metal1.id);
    REQUIRE(hefesto::db::paint(*cell, *editor.technology, db::Rect{0, 0, 100, 50}, *metal1.id).ok());
    hefesto::db::recompute_bbox(*cell);
    window.cell = "c3d";
    window.mode = Mode::mask_layer;
  }

  [[nodiscard]] const hefesto::mask::OutputStyle& style() const { return *editor.mask.output_styles.current_style(); }

  view3d::Scene scene(bool magic_compat) {
    view3d::BuildContext ctx;
    return view3d::build_scene(editor, window, magic_compat, ctx);
  }
};

}  // namespace

TEST_CASE("mask-layer mode draws the generated mask layers in mask units, with no xy factor",
          "[cap:view-3d][req:Rendered geometry]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d");
  REQUIRE(s.window().mode == Mode::mask_layer);
  view3d::BuildContext ctx;
  const view3d::Scene scene = view3d::build_scene(s.editor, s.window(), true, ctx);
  CHECK(scene.xy_factor == 1.0F);
  CHECK(scene.stack.mode() == Mode::mask_layer);
  REQUIRE(scene.stack.size() == 3);  // POLY, MET1, MET2 of `gdsii`
  REQUIRE(scene.mesh != nullptr);
  CHECK(scene.mesh->mode == Mode::mask_layer);
  // The bounding box (0,0)-(100,50) times f = 10, not grown (W3Dmain.c:1743).
  CHECK(scene.clip == db::Rect{0, 0, 1000, 500});
  const Extent met1 = extent_of(*scene.mesh, 1);
  CHECK(met1.area == 1000.0 * 500.0);
  const Extent met2 = extent_of(*scene.mesh, 2);
  CHECK(met2.x0 == 200.0);
  CHECK(met2.y0 == 100.0);
  CHECK(met2.x1 == 600.0);
  CHECK(met2.y1 == 400.0);
  CHECK(extent_of(*scene.mesh, 0).rects == 0);  // no poly
}

TEST_CASE("Region model: the mask source reads the style's halo around the box and is clipped to it",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    MaskSession s(compat);
    s.ok("specialopen wind3d c3d");
    s.w3d("cutbox 0 0 105 50");
    hefesto::diag::NullSink quiet;
    const view3d::WindowBuild build = view3d::window_build(s.editor, s.window(), compat, quiet);
    REQUIRE(build.inputs.mask.has_value());
    CHECK(build.inputs.mask->area == db::Rect{0, 0, 105, 50});
    CHECK(build.inputs.clip.area == db::Rect{0, 0, 1050, 500});
    CHECK(build.inputs.clip.cut_box);
    CHECK(build.inputs.clip.halo ==
          hefesto::mask::generation_radius(*build.inputs.mask->style, build.inputs.mask->generation.compat));
    CHECK(build.inputs.clip.halo == 11);  // Magic's cs_radius for `grow 50` at scalefactor 10
    const view3d::Scene scene = s.scene();
    REQUIRE(scene.mesh != nullptr);
    // MET1 only inside the box: the metal at x = 108 is outside it.
    const Extent met1 = extent_of(*scene.mesh, kMet1);
    CHECK(met1.area == 600.0 * 500.0);
    // NWELL: metal1 (0,0)-(60,50) grown by 50, and the metal at x = 108,
    // outside the box but inside its halo, grown into it from x = 1030.
    const Extent nwell = extent_of(*scene.mesh, kNwell);
    CHECK(nwell.x0 == 0.0);
    CHECK(nwell.x1 == 1050.0);
    CHECK(nwell.y0 == 0.0);
    CHECK(nwell.y1 == 500.0);
    CHECK(nwell.area == (650.0 + 20.0) * 500.0);
  }
}

TEST_CASE("without a cut box magic-compat ON clips the mask layers to the cell's bounding box",
          "[cap:view-3d][req:Rendered geometry]") {
  MaskSession s(true);
  s.ok("specialopen wind3d c3d");
  CHECK(view3d::mask_generation_area(s.editor, s.window(), true) == db::Rect{0, 0, 120, 50});
  const view3d::Scene scene = s.scene();
  REQUIRE(scene.mesh != nullptr);
  CHECK_FALSE(scene.clipped);
  CHECK(scene.clip == db::Rect{0, 0, 1200, 500});
  const Extent nwell = extent_of(*scene.mesh, kNwell);
  CHECK(nwell.x0 == 0.0);  // the grow past the cell's edge is cut, as in Magic
  CHECK(nwell.y0 == 0.0);
  CHECK(nwell.x1 == 1200.0);
  CHECK(nwell.y1 == 500.0);
}

TEST_CASE("without a cut box magic-compat OFF generates the bounding box grown by the halo",
          "[cap:view-3d][req:Rendered geometry]") {
  MaskSession s(false);
  s.ok("specialopen wind3d c3d");
  CHECK(view3d::mask_generation_area(s.editor, s.window(), false) == db::Rect{-11, -11, 131, 61});
  const view3d::Scene scene = s.scene();
  REQUIRE(scene.mesh != nullptr);
  CHECK(scene.clip == db::Rect{-110, -110, 1310, 610});
  const Extent nwell = extent_of(*scene.mesh, kNwell);
  CHECK(nwell.x0 == -50.0);  // nothing of a mask layer is cut at the cell's edge
  CHECK(nwell.y0 == -50.0);
  CHECK(nwell.x1 == 1250.0);
  CHECK(nwell.y1 == 550.0);
}

TEST_CASE("a cut box outside the cell gives an empty mask-layer mesh in both modes",
          "[cap:view-3d][req:Rendered geometry]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    MaskSession s(compat);
    s.ok("specialopen wind3d c3d");
    s.w3d("cutbox 500 500 600 600");
    const view3d::Scene scene = s.scene();
    REQUIRE(scene.mesh != nullptr);
    CHECK(scene.mesh->chunks.empty());
    CHECK(scene.clip == db::Rect{5000, 5000, 6000, 6000});
  }
}

TEST_CASE("Render heights in micrometres with magic-compat ON: MET1 drawn with h and t as stored (Q-MISC-72)",
          "[cap:view-3d][req:Rendered geometry]") {
  MaskSession s(true);
  s.ok("cif ostyle micron");
  s.ok("specialopen wind3d c3d");
  const view3d::Scene scene = s.scene();
  REQUIRE(scene.mesh != nullptr);
  CHECK(scene.stack.microns_per_unit() == 0.01);
  CHECK(scene.mesh->layer_heights.at(0) == view3d::DrawHeight{1.376F, 0.36F});
  CHECK(s.w3d_value("render MET1") == "1.3760000467300415 0.36000001430511475 -49");
}

TEST_CASE("Render heights in micrometres with magic-compat OFF: MET1 drawn with 137.6 and 36 units (Q-MISC-72)",
          "[cap:view-3d][req:Rendered geometry]") {
  MaskSession s(false);
  s.ok("cif ostyle micron");
  s.ok("specialopen wind3d c3d");
  const view3d::Scene scene = s.scene();
  REQUIRE(scene.mesh != nullptr);
  const view3d::DrawHeight h = scene.mesh->layer_heights.at(0);
  CHECK(h.height == static_cast<float>(static_cast<double>(1.376F) / 0.01));
  CHECK(h.thickness == static_cast<float>(static_cast<double>(0.36F) / 0.01));
  CHECK(s.w3d_value("render MET1") == "1.3760000467300415 0.36000001430511475 -49");  // stored values in both settings
}

TEST_CASE("Same stack as Magic: metal1 with render 1.0 / 0.5 is drawn from z 1.0 to 1.5 tech units times s_z",
          "[cap:hefesto-3d-visualization][req:Magic 3D view parity]") {
  MaskSession s(true);
  s.editor.styles.emplace(style_set());
  view3d::FaceLogDisplay log{s.services.view().files, "/t/gl.log", &s.display};
  s.editor.view3d.display = &log;
  s.ok("specialopen wind3d c3d");
  const view3d::Scene scene = s.scene();
  REQUIRE(scene.mesh != nullptr);
  CHECK(scene.mesh->layer_heights.at(kMet1) == view3d::DrawHeight{1.0F, 0.5F});
  CHECK(scene.stack.layers()[kMet1].style == kMetal1Style);
  const float sz = s.window().view.scale_z;
  const float top = -1.0F * sz;
  const float bottom = top - (0.5F * sz);
  char expected[160];
  std::snprintf(expected, sizeof expected, "poly metal1 4 0 0 %.9g 600 0 %.9g 600 500 %.9g 0 500 %.9g\n",
                static_cast<double>(top), static_cast<double>(top), static_cast<double>(top),
                static_cast<double>(top));
  const auto text = s.services.view().files.read("/t/gl.log");
  REQUIRE(text.ok());
  CHECK(text.value().find(expected) != std::string::npos);
  std::snprintf(expected, sizeof expected, "poly metal1 4 0 500 %.9g 600 500 %.9g 600 0 %.9g 0 0 %.9g\n",
                static_cast<double>(bottom), static_cast<double>(bottom), static_cast<double>(bottom),
                static_cast<double>(bottom));
  CHECK(text.value().find(expected) != std::string::npos);
}

TEST_CASE("the mask-layer face log leaves out cross styles and hidden mask layers in both modes (N3)",
          "[cap:view-3d][req:Rendered geometry]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    MaskSession s(compat);
    s.paint("metal2", db::Rect{10, 10, 20, 20});
    s.editor.styles.emplace(style_set());
    view3d::FaceLogDisplay log{s.services.view().files, "/t/gl.log", &s.display};
    s.editor.view3d.display = &log;
    s.ok("specialopen wind3d c3d");
    s.w3d("render MET2 2 0.5 " + std::to_string(view3d::render_style_position(kCrossStyle)));
    auto text = s.services.view().files.read("/t/gl.log");
    REQUIRE(text.ok());
    CHECK(text.value().find("poly metal1 ") != std::string::npos);
    CHECK(text.value().find("contact_X") == std::string::npos);  // MET2 now in a cross style
    s.w3d("see no MET1");
    text = s.services.view().files.read("/t/gl.log");
    REQUIRE(text.ok());
    CHECK(text.value().find("poly metal1 ") == std::string::npos);
  }
}

TEST_CASE("a change of output style resets the mask visibility set and rebuilds from the new style",
          "[cap:view-3d][req:Rendered geometry]") {
  MaskSession s(true);
  s.ok("specialopen wind3d c3d");
  s.w3d("see no MET2");
  hefesto::diag::NullSink quiet;
  const view3d::WindowBuild before = view3d::window_build(s.editor, s.window(), true, quiet);
  CHECK(before.hidden.at(kMet2));
  s.ok("cif ostyle micron");
  s.w3d("refresh");  // the window sees the new style (Q-MISC-71)
  const view3d::WindowBuild after = view3d::window_build(s.editor, s.window(), true, quiet);
  REQUIRE(after.inputs.mask.has_value());
  CHECK(after.inputs.mask->style != before.inputs.mask->style);  // a new source: the host rebuilds fully
  REQUIRE(after.stack->size() == 1);
  CHECK(after.hidden == std::vector<bool>{false});
  CHECK(s.window().visibility.mask_hidden.empty());
  const view3d::Scene scene = s.scene();
  REQUIRE(scene.mesh != nullptr);
  CHECK(extent_of(*scene.mesh, 0).area == (60.0 * 50.0) + (12.0 * 50.0));  // f = 1
}

TEST_CASE("temporary layers are drawn with magic-compat ON, as Magic, and left out OFF",
          "[cap:view-3d][req:Rendered geometry]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    MaskSession s(compat);
    s.ok("specialopen wind3d c3d");
    hefesto::diag::NullSink quiet;
    const view3d::WindowBuild build = view3d::window_build(s.editor, s.window(), compat, quiet);
    REQUIRE(build.hidden.size() == 5);
    CHECK(build.hidden.at(kHalo) == !compat);
    CHECK_FALSE(build.hidden.at(kNwell));
  }
}

TEST_CASE("a local edit regenerates only the mask-layer chunks within the halo of it",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  MaskSession s(true);
  s.clear();
  for (int i = 0; i < 8; ++i) {
    s.paint("metal1", db::Rect{i * 200, 0, (i * 200) + 50, 50});
  }
  s.ok("specialopen wind3d c3d");
  hefesto::diag::NullSink quiet;
  view3d::WindowBuild build = view3d::window_build(s.editor, s.window(), true, quiet);
  build.inputs.max_chunk_tiles = 4;  // one rectangle's MET1, HALO and NWELL overlap: fewer would split them down to single units
  view3d::MeshWorker worker;
  worker.build(build.inputs);
  REQUIRE(worker.wait_idle(20000));
  const view3d::MeshHandle first = worker.current();
  REQUIRE(first != nullptr);
  REQUIRE(first->chunks.size() > 4);

  // metal1 right of the first rectangle, inside the bounding box.
  const db::Rect edit{60, 10, 80, 40};
  s.paint("metal1", edit);
  REQUIRE(s.cell->bbox() == db::Rect{0, 0, 1450, 50});
  const std::vector<db::Rect> dirty{edit};
  worker.changed(dirty);
  REQUIRE(worker.wait_idle(20000));
  const view3d::MeshHandle second = worker.current();
  REQUIRE(second != nullptr);
  REQUIRE(second != first);

  // The marked area: the edit grown by the halo (11), in mask units, grown by 1.
  const db::Rect marked{((60 - 11) * 10) - 1, ((10 - 11) * 10) - 1, ((80 + 11) * 10) + 1, ((40 + 11) * 10) + 1};
  int rebuilt = 0;
  for (const view3d::Chunk& c : second->chunks) {
    const auto old = std::find_if(first->chunks.begin(), first->chunks.end(),
                                  [&c](const view3d::Chunk& o) { return o.id == c.id; });
    const bool meets = !db::Rect{std::max(c.area.x0, marked.x0), std::max(c.area.y0, marked.y0),
                                 std::min(c.area.x1, marked.x1), std::min(c.area.y1, marked.y1)}
                            .is_null();
    if (old == first->chunks.end() || old->version != c.version) {
      ++rebuilt;
      CHECK(meets);
    } else {
      CHECK_FALSE(meets);
    }
  }
  CHECK(rebuilt > 0);
  CHECK(rebuilt < static_cast<int>(second->chunks.size()));

  // The geometry equals a fresh build's.
  view3d::BuildContext ctx;
  build = view3d::window_build(s.editor, s.window(), true, quiet);
  const std::optional<view3d::MeshModel> fresh = view3d::build_once(build.inputs, ctx);
  REQUIRE(fresh.has_value());
  CHECK(fresh->region == second->region);
  for (const view3d::LayerId layer : {kMet1, kNwell}) {
    INFO("layer " << layer);
    CHECK(extent_of(*second, layer).area == extent_of(*fresh, layer).area);
  }
  CHECK(extent_of(*second, kMet1).area == extent_of(*first, kMet1).area + (200.0 * 300.0));
}

TEST_CASE("sky130A in mask-layer mode: MET1 from its render statement, heights per Q-MISC-72",
          "[cap:view-3d][req:Rendered geometry]") {
  const auto file = hefesto::test::pinned_sky130a_tech();
  if (!file || !std::filesystem::exists(*file)) {
    SKIP("PDK tech file not in .cache/pdks");
  }
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    PdkSession s(*file);
    const view3d::Scene scene = s.scene(compat);
    const hefesto::mask::OutputStyle& style = s.style();
    REQUIRE(scene.mesh != nullptr);
    REQUIRE(scene.stack.size() == style.layers.size());
    const auto met1 = style.find_layer("MET1");
    REQUIRE(met1.has_value());
    const auto id = static_cast<view3d::LayerId>(*met1);
    const double f = style.scale.scale_factor;
    CHECK(extent_of(*scene.mesh, id).area == (100.0 * f) * (50.0 * f));  // metal1 (0,0)-(100,50)
    const view3d::StackLayer& layer = scene.stack.layers()[id];
    REQUIRE(layer.source == view3d::ValueSource::render);
    const view3d::DrawHeight h = scene.mesh->layer_heights.at(id);
    if (compat) {
      CHECK(h == view3d::DrawHeight{layer.height, layer.thickness});
    } else {
      const double um = scene.stack.microns_per_unit();
      CHECK(h.height == static_cast<float>(static_cast<double>(layer.height) / um));
      CHECK(h.thickness == static_cast<float>(static_cast<double>(layer.thickness) / um));
    }
    CHECK(extent_of(*scene.mesh, id).sides > 0);
  }
}

TEST_CASE("gf180mcuD in mask-layer mode: no render statements, every layer flat in the pale-highlight style",
          "[cap:view-3d][req:Rendered geometry]") {
  const auto file = hefesto::test::pinned_gf180mcud_tech();
  if (!file || !std::filesystem::exists(*file)) {
    SKIP("PDK tech file not in .cache/pdks");
  }
  PdkSession s(*file);
  const view3d::Scene scene = s.scene(true);
  const hefesto::mask::OutputStyle& style = s.style();
  REQUIRE(scene.mesh != nullptr);
  REQUIRE(scene.stack.size() == style.layers.size());
  for (std::size_t i = 0; i < scene.stack.size(); ++i) {
    CHECK(scene.stack.layers()[i].style == view3d::kPaleHighlightStyle);
    CHECK(scene.mesh->layer_heights.at(i) == view3d::DrawHeight{});
  }
  for (const view3d::Chunk& c : scene.mesh->chunks) {
    CHECK(c.sides.empty());  // h = t = 0: top faces only
  }
  const auto met1 = style.find_layer("MET1");
  REQUIRE(met1.has_value());
  const double f = style.scale.scale_factor;
  CHECK(extent_of(*scene.mesh, static_cast<view3d::LayerId>(*met1)).area == (100.0 * f) * (50.0 * f));
}
