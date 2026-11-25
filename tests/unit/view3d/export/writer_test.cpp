// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-3d-visualization "3D export" (M10 design D14, J2; tasks 11.3, 11.4):
// the glTF/GLB and STL writers on hand-built stacks and meshes: units and z
// scale, physical heights in both magic-compat modes (Q-MISC-72), layer
// choice (visibility, drawn styles, one entry per name), materials, face
// orientation, the warnings for layers without a physical height, and an
// export from mask-layer mode on gf180mcuD. Owner: EXPORT.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "export_fixture.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "reference_data.hpp"

namespace view3d = hefesto::view3d;
namespace diag = hefesto::diag;
namespace gds = hefesto::gds;
namespace db = hefesto::db;
using Catch::Approx;
using view3d::test::ParsedGltf;

namespace {

constexpr db::Rect kRegion{-10, -20, 90, 80};

struct Written {
  std::string bytes;
  std::vector<diag::Diagnostic> messages;
};

Written gltf(const view3d::MeshModel& mesh, const view3d::LayerStack& stack, view3d::ExportOptions options) {
  gds::MemorySink out;
  diag::CollectingSink sink;
  REQUIRE(view3d::write_gltf(mesh, stack, options, out, sink).ok());
  return {view3d::test::text_of(out.bytes()), sink.take()};
}

Written stl(const view3d::MeshModel& mesh, const view3d::LayerStack& stack, view3d::ExportOptions options) {
  gds::MemorySink out;
  diag::CollectingSink sink;
  REQUIRE(view3d::write_stl(mesh, stack, options, out, sink).ok());
  return {view3d::test::text_of(out.bytes()), sink.take()};
}

view3d::ExportOptions glb() {
  view3d::ExportOptions options;
  options.format = view3d::ExportFormat::glb;
  return options;
}

// A mask-layer stack whose MET1 has `render MET1 metal1 1.376 0.36`, at
// 0.1 µm per internal unit (scalefactor 10), and its mesh: one box.
struct MaskModel {
  view3d::LayerStack stack;
  view3d::MeshModel mesh;
};

MaskModel met1_model(bool magic_compat) {
  MaskModel m;
  m.stack = view3d::LayerStack{view3d::Mode::mask_layer,
                               {view3d::test::mask_layer("MET1", 0, std::array<float, 2>{1.376F, 0.36F})},
                               magic_compat,
                               0.1};
  m.mesh = view3d::test::one_chunk_mesh(view3d::Mode::mask_layer, kRegion, m.stack);
  view3d::test::add_box(m.mesh.chunks[0], 0, 0.0F, 0.0F, 40.0F, 30.0F);
  return m;
}

// Layout-layer stack: poly (h 2, t 1) and metal1 (h 5, t 3) at 0.1 µm per unit.
struct LayoutModel {
  view3d::LayerStack stack;
  view3d::MeshModel mesh;
};

LayoutModel two_layers() {
  LayoutModel m;
  m.stack = view3d::LayerStack{view3d::Mode::layout_layer,
                               {view3d::test::layout_layer("poly", 2.0F, 1.0F),
                                view3d::test::layout_layer("metal1", 5.0F, 3.0F)},
                               true,
                               0.1};
  m.mesh = view3d::test::one_chunk_mesh(view3d::Mode::layout_layer, kRegion, m.stack);
  view3d::test::add_box(m.mesh.chunks[0], 0, 0.0F, 0.0F, 20.0F, 100.0F);
  view3d::test::add_box(m.mesh.chunks[0], 1, 10.0F, 10.0F, 100.0F, 50.0F);
  return m;
}

}  // namespace

TEST_CASE("Export without the view's exaggeration: metal1 0.36 µm thick, 1.44 µm with -zscale 4 recorded in "
          "asset.extras",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  const MaskModel m = met1_model(true);
  // The view's s_z (Magic's default 25) is a view parameter: nothing of it reaches the export.
  const ParsedGltf plain(gltf(m.mesh, m.stack, glb()).bytes);
  REQUIRE(plain.result == cgltf_result_success);
  REQUIRE(plain.validation == cgltf_result_success);
  CHECK(plain.thickness("MET1") == Approx(0.36).margin(1e-6));
  const cgltf_accessor* p = plain.positions("MET1");
  CHECK(p->min[1] == Approx(1.376).margin(1e-6));
  CHECK(plain.asset_extras().find(R"("zscale":1)") != std::string::npos);

  view3d::ExportOptions scaled = glb();
  scaled.zscale = 4.0;
  const ParsedGltf four(gltf(m.mesh, m.stack, scaled).bytes);
  REQUIRE(four.validation == cgltf_result_success);
  CHECK(four.thickness("MET1") == Approx(1.44).margin(1e-6));
  CHECK(four.positions("MET1")->min[1] == Approx(4 * 1.376).margin(1e-5));
  CHECK(four.asset_extras().find(R"("hefesto")") != std::string::npos);
  CHECK(four.asset_extras().find(R"("zscale":4)") != std::string::npos);
  CHECK(four.asset_extras().find(R"("units":"um")") != std::string::npos);
}

TEST_CASE("Physical heights with magic-compat ON: mask-layer `render` values written as µm (Q-MISC-72)",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  const MaskModel on = met1_model(true);
  CHECK(on.stack.draw_height(0) == view3d::DrawHeight{1.376F, 0.36F});  // drawn as stored
  const ParsedGltf file(gltf(on.mesh, on.stack, glb()).bytes);
  REQUIRE(file.validation == cgltf_result_success);
  CHECK(file.thickness("MET1") == Approx(0.36).margin(1e-6));
  CHECK(file.positions("MET1")->min[1] == Approx(1.376).margin(1e-6));
}

TEST_CASE("Physical heights with magic-compat OFF: the same bytes as with it ON (Q-MISC-72)",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  const MaskModel off = met1_model(false);
  // OFF draws the render values in internal units (divided by 0.1 µm per unit) ...
  CHECK(off.stack.draw_height(0).thickness == Approx(3.6F));
  // ... but the export is in µm, independent of the setting.
  const Written off_file = gltf(off.mesh, off.stack, glb());
  const ParsedGltf file(off_file.bytes);
  REQUIRE(file.validation == cgltf_result_success);
  CHECK(file.thickness("MET1") == Approx(0.36).margin(1e-6));
  CHECK(off_file.bytes == gltf(met1_model(true).mesh, met1_model(true).stack, glb()).bytes);
  CHECK(stl(off.mesh, off.stack, {}).bytes == stl(met1_model(true).mesh, met1_model(true).stack, {}).bytes);
}

TEST_CASE("glTF: one node per layer in layer order, µm relative to the origin in the root translation",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  const LayoutModel m = two_layers();
  const ParsedGltf file(gltf(m.mesh, m.stack, glb()).bytes);
  REQUIRE(file.result == cgltf_result_success);
  REQUIRE(file.validation == cgltf_result_success);
  const cgltf_data& d = file.data();
  CHECK(std::string{d.asset.version} == "2.0");
  CHECK(std::string{d.asset.generator} == "Hefesto");
  REQUIRE(d.scenes_count == 1);
  REQUIRE(d.scene->nodes_count == 1);
  const cgltf_node& root = *d.scene->nodes[0];
  REQUIRE(root.children_count == 2);
  CHECK(std::string{root.children[0]->name} == "poly");
  CHECK(std::string{root.children[1]->name} == "metal1");
  // The region's lower-left corner (-10, -20) units = (-1, -2) µm; glTF is +Y up: (x, z, -y).
  CHECK(root.translation[0] == Approx(-1.0));
  CHECK(root.translation[1] == 0.0F);
  CHECK(root.translation[2] == Approx(2.0));
  const cgltf_accessor* metal = file.positions("metal1");
  // Instances are relative to the chunk's origin, here the region's corner.
  CHECK(metal->min[0] == Approx(1.0));   // 10 units
  CHECK(metal->max[0] == Approx(10.0));  // 100 units
  CHECK(metal->min[1] == Approx(0.5));   // h = 5 units
  CHECK(metal->max[1] == Approx(0.8));   // h + t = 8 units
  CHECK(metal->min[2] == Approx(-5.0));  // -(50 units)
  CHECK(metal->max[2] == Approx(-1.0));  // -(10 units)
  CHECK(metal->component_type == cgltf_component_type_r_32f);
  const cgltf_primitive& prim = root.children[1]->mesh->primitives[0];
  CHECK(prim.indices->component_type == cgltf_component_type_r_32u);
  CHECK(prim.indices->count == 3 * (4 + 8));  // top and bottom: 2 triangles each; 4 sides: 2 each
}

TEST_CASE("glTF materials: the display style's long name and linear base color; grey and layer names without "
          "styles",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  const LayoutModel m = two_layers();
  const ParsedGltf plain(gltf(m.mesh, m.stack, glb()).bytes);
  REQUIRE(plain.data().materials_count == 2);
  CHECK(std::string{plain.data().materials[1].name} == "metal1");
  CHECK(plain.data().materials[1].pbr_metallic_roughness.base_color_factor[0] == Approx(0.215860501));  // sRGB 128/255 (0.5 grey) in linear light

  const hefesto::display::StyleSet styles = view3d::test::two_styles();
  view3d::LayerStack styled{view3d::Mode::layout_layer,
                            {view3d::test::layout_layer("metal1", 5.0F, 3.0F, hefesto::display::kFixedStyleCount)},
                            true,
                            0.1};
  view3d::MeshModel mesh = view3d::test::one_chunk_mesh(view3d::Mode::layout_layer, kRegion, styled);
  view3d::test::add_box(mesh.chunks[0], 0, 0.0F, 0.0F, 10.0F, 10.0F);
  view3d::ExportOptions options = glb();
  options.styles = &styles;
  const ParsedGltf file(gltf(mesh, styled, options).bytes);
  REQUIRE(file.validation == cgltf_result_success);
  const cgltf_material& material = *file.node("metal1")->mesh->primitives[0].material;
  CHECK(std::string{material.name} == "metal1 wiring");
  CHECK(material.pbr_metallic_roughness.base_color_factor[0] == 0.0F);
  CHECK(material.pbr_metallic_roughness.base_color_factor[2] == 1.0F);
  CHECK(material.pbr_metallic_roughness.metallic_factor == 0.0F);
}

TEST_CASE("Hidden layers are left out with -visible and written with -all",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  const LayoutModel m = two_layers();
  view3d::ExportOptions options = glb();
  options.hidden = {false, true};
  const ParsedGltf visible(gltf(m.mesh, m.stack, options).bytes);
  CHECK(visible.node("poly") != nullptr);
  CHECK(visible.node("metal1") == nullptr);
  options.visible_only = false;
  const ParsedGltf all(gltf(m.mesh, m.stack, options).bytes);
  CHECK(all.node("metal1") != nullptr);
}

TEST_CASE("A type in two display styles is exported once; a style that is not drawn is not exported (Q-MISC-77)",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  const hefesto::display::StyleSet styles = view3d::test::two_styles();
  const std::size_t solid = hefesto::display::kFixedStyleCount;
  view3d::LayerStack stack{view3d::Mode::layout_layer,
                           {view3d::test::layout_layer("via", 5.0F, 3.0F, solid),
                            view3d::test::layout_layer("via", 5.0F, 3.0F, solid),
                            view3d::test::layout_layer("ring", 1.0F, 1.0F, solid + 1)},
                           true,
                           0.1};
  view3d::MeshModel mesh = view3d::test::one_chunk_mesh(view3d::Mode::layout_layer, kRegion, stack);
  for (view3d::LayerId layer = 0; layer < 3; ++layer) {
    view3d::test::add_box(mesh.chunks[0], layer, 0.0F, 0.0F, 10.0F, 10.0F);
  }
  view3d::ExportOptions options = glb();
  options.styles = &styles;
  diag::NullSink quiet;
  const std::vector<view3d::ExportLayer> layers = view3d::export_layers(mesh, stack, options, quiet);
  REQUIRE(layers.size() == 1);
  CHECK(layers[0].name == "via");
  CHECK(layers[0].layer == 1);  // the last entry drawn wins on coplanar faces
  options.styles = nullptr;     // without styles every entry is drawn
  CHECK(view3d::export_layers(mesh, stack, options, quiet).size() == 2);
}

TEST_CASE("Layout layers without a physical height are left out with VIEW3D-EXPORT-003",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  LayoutModel m = two_layers();
  // No output style: no µm per internal unit.
  const view3d::LayerStack unscaled{view3d::Mode::layout_layer,
                                    {m.stack.layers().begin(), m.stack.layers().end()},
                                    true,
                                    0.0};
  const Written w = gltf(m.mesh, unscaled, glb());
  REQUIRE(w.messages.size() == 1);
  CHECK(w.messages[0].code == "VIEW3D-EXPORT-003");
  CHECK(w.messages[0].severity == diag::Severity::warning);
  CHECK(w.messages[0].text.find("poly metal1;") != std::string::npos);
  const ParsedGltf file(w.bytes);
  CHECK(file.validation == cgltf_result_success);
  CHECK(file.data().meshes_count == 0);
}

TEST_CASE("Export from mask-layer mode on gf180mcuD leaves every layer out with VIEW3D-STACK-001",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  // The mask-layer mesh (MASKSRC, task 10.1) is not on main yet: the stack is
  // gf180mcuD's own (no `render` lines), the mesh hand-built over it.
  const auto file = hefesto::test::pinned_gf180mcud_tech();
  if (!file) {
    SKIP("gf180mcuD.tech not in .cache/pdks");
  }
  hefesto::platform::LocalFileSystem files;
  diag::CollectingSink load_sink;
  auto loaded = hefesto::tech::load_technology(files, file->string(), hefesto::tech::LoadOptions{}, load_sink);
  REQUIRE(loaded.ok());
  const hefesto::mask::OutputStyleSet set = hefesto::mask::read_output_styles(*loaded.value(), load_sink);
  REQUIRE(set.current_style() != nullptr);
  const hefesto::mask::OutputStyle& style = *set.current_style();
  diag::NullSink quiet;
  const view3d::LayerStack stack = view3d::build_mask_stack(style, nullptr, true, quiet);
  view3d::MeshModel mesh = view3d::test::one_chunk_mesh(view3d::Mode::mask_layer, kRegion, stack);
  for (std::size_t i = 0; i < stack.size(); ++i) {
    view3d::test::add_box(mesh.chunks[0], static_cast<view3d::LayerId>(i), 0.0F, 0.0F, 10.0F, 10.0F);
  }
  view3d::ExportOptions options = glb();
  options.output_style = &style;
  const Written w = gltf(mesh, stack, options);
  REQUIRE(w.messages.size() == 1);
  CHECK(w.messages[0].code == "VIEW3D-STACK-001");
  CHECK(w.messages[0].text.find(" " + style.layers[*style.find_layer("MET1")].name) != std::string::npos);
  const ParsedGltf parsed(w.bytes);
  CHECK(parsed.validation == cgltf_result_success);
  CHECK(parsed.data().meshes_count == 0);
  // Without the output style the warning still names the layers.
  options.output_style = nullptr;
  CHECK(gltf(mesh, stack, options).messages.at(0).code == "VIEW3D-STACK-001");
  // STL: an empty solid, same warning.
  const Written s = stl(mesh, stack, options);
  CHECK(view3d::test::read_stl(s.bytes).triangles == 0);
  CHECK(s.messages.at(0).code == "VIEW3D-STACK-001");
}

TEST_CASE(".gltf writes the buffer to the .bin sink under its URI, or embeds it as a data URI",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  const LayoutModel m = two_layers();
  gds::MemorySink json;
  gds::MemorySink bin;
  view3d::ExportOptions options;
  options.bin_out = &bin;
  options.bin_uri = "inv.bin";
  diag::NullSink quiet;
  REQUIRE(view3d::write_gltf(m.mesh, m.stack, options, json, quiet).ok());
  const ParsedGltf file(view3d::test::text_of(json.bytes()));
  REQUIRE(file.validation == cgltf_result_success);
  REQUIRE(file.data().buffers_count == 1);
  CHECK(std::string{file.data().buffers[0].uri} == "inv.bin");
  CHECK(file.data().buffers[0].size == bin.bytes().size());

  const ParsedGltf embedded(gltf(m.mesh, m.stack, {}).bytes);
  REQUIRE(embedded.validation == cgltf_result_success);
  CHECK(std::string{embedded.data().buffers[0].uri}.starts_with("data:application/octet-stream;base64,"));
  cgltf_options load{};
  REQUIRE(cgltf_load_buffers(&load, const_cast<cgltf_data*>(&embedded.data()), nullptr) == cgltf_result_success);
  CHECK(std::memcmp(embedded.data().buffers[0].data, bin.bytes().data(), bin.bytes().size()) == 0);
}

TEST_CASE("STL: fixed header, triangles in layer and mesh order, normals from the geometry, µm",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  const LayoutModel m = two_layers();
  const Written merged = stl(m.mesh, m.stack, {});
  const view3d::test::StlFile f = view3d::test::read_stl(merged.bytes);
  CHECK(f.header.starts_with("Hefesto STL merged "));
  CHECK(f.header.find_first_not_of(' ', 18) == std::string::npos);
  CHECK(f.triangles == 24);
  CHECK(f.size == 84 + (50 * 24));
  // The first triangle: poly's top face, normal +z, its first corner at the box's lower-left.
  const std::string_view b = merged.bytes;
  CHECK(view3d::test::f32_at(b, 84 + 8) == 1.0F);
  CHECK(view3d::test::f32_at(b, 84 + 12) == 0.0F);  // the model origin
  CHECK(view3d::test::f32_at(b, 84 + 16) == 0.0F);
  CHECK(view3d::test::f32_at(b, 84 + 20) == Approx(0.3));  // z = (h + t) * 0.1
  // Per layer: only that layer, named in the header.
  view3d::ExportOptions one;
  one.stl_layer = 1;
  const view3d::test::StlFile metal = view3d::test::read_stl(stl(m.mesh, m.stack, one).bytes);
  CHECK(metal.header.starts_with("Hefesto STL layer metal1 "));
  CHECK(metal.triangles == 12);
}

TEST_CASE("Faces are counter-clockwise seen from outside: each triangle's winding agrees with its normal",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  LayoutModel m = two_layers();
  view3d::Chunk& chunk = m.mesh.chunks[0];
  // A split tile: a triangle and its diagonal face (outward to the left).
  chunk.triangles.push_back(view3d::Triangle{{30.0F, 40.0F, 30.0F}, {60.0F, 60.0F, 90.0F}, 0, 2.0F, 1.0F});
  chunk.sides.push_back(view3d::SideInstance{30.0F, 90.0F, 40.0F, 60.0F, 0, view3d::side_flag::outward_left |
                                                                                 view3d::side_flag::diagonal,
                                             0.0F, 0.0F});
  const std::string bytes = stl(m.mesh, m.stack, {}).bytes;
  const view3d::test::StlFile f = view3d::test::read_stl(bytes);
  CHECK(f.triangles == 24 + 2 + 2);
  for (std::uint32_t t = 0; t < f.triangles; ++t) {
    const std::size_t at = 84 + (50 * std::size_t{t});
    std::array<std::array<double, 3>, 4> v{};
    for (std::size_t i = 0; i < 4; ++i) {
      for (std::size_t c = 0; c < 3; ++c) {
        v[i][c] = static_cast<double>(view3d::test::f32_at(bytes, at + (12 * i) + (4 * c)));
      }
    }
    const std::array<double, 3> e1{v[2][0] - v[1][0], v[2][1] - v[1][1], v[2][2] - v[1][2]};
    const std::array<double, 3> e2{v[3][0] - v[1][0], v[3][1] - v[1][1], v[3][2] - v[1][2]};
    const std::array<double, 3> cross{(e1[1] * e2[2]) - (e1[2] * e2[1]), (e1[2] * e2[0]) - (e1[0] * e2[2]),
                                      (e1[0] * e2[1]) - (e1[1] * e2[0])};
    const double dot = (cross[0] * v[0][0]) + (cross[1] * v[0][1]) + (cross[2] * v[0][2]);
    INFO("triangle " << t);
    CHECK(dot > 0.0);
    CHECK(std::hypot(v[0][0], v[0][1], v[0][2]) == Approx(1.0));
  }
}
