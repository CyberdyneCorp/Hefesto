// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-3d-visualization "3D export" (M10 design D14; task 11.3): the
// `export3d` command on the open 3D window (the WIND3D test session: cell
// c3d with metal1 (0,0)-(100,50) and metal2 (20,10)-(60,40), 0.1 µm per
// internal unit): formats, files, visibility, the cut box and the errors.
// Owner: EXPORT.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../client/client_fixture.hpp"
#include "export_fixture.hpp"
#include "hefesto/view3d/export/command.hpp"

namespace view3d = hefesto::view3d;
using Catch::Approx;
using view3d::test::ClientSession;
using view3d::test::ParsedGltf;

namespace {

// A session with the display bindings of mos.OpenGL.dstyle's metal1 and
// metal2 styles (a headless session has no style set, and the stack's
// (type, style) pairs come from them), as the WIND3D scene tests do.
struct Session : ClientSession {
  Session() {
    editor.styles.emplace();
    auto& types = editor.styles->types.styles_of_type;
    types.resize(hefesto::tech::kMaxTileTypes);
    types[*editor.technology->find_type("metal1").id] = {hefesto::display::kFixedStyleCount + 20};
    types[*editor.technology->find_type("metal2").id] = {hefesto::display::kFixedStyleCount + 21};
  }
};

std::string file(ClientSession& s, const std::string& path) {
  auto read = s.services.view().files.read(path);
  REQUIRE(read.ok());
  return read.value();
}

bool exists(ClientSession& s, const std::string& path) {
  return s.services.view().files.exists(path);
}

// Runs a command line as typed (the CLI origin): commands run from a script
// see the sandboxed file system, which grants no root in this session.
hefesto::cmd::CommandResult typed(ClientSession& s, std::string_view line) {
  hefesto::cmd::CommandResult last = hefesto::cmd::CommandResult::success();
  for (auto& r : hefesto::ui::run_command_line(*s.dispatcher, line, hefesto::cmd::Origin::cli)) {
    last = std::move(r);
  }
  return last;
}

void typed_ok(ClientSession& s, std::string_view line) {
  const hefesto::cmd::CommandResult r = typed(s, line);
  INFO(line << " -> " << (r.diagnostics.empty() ? std::string{} : r.diagnostics.back().text));
  REQUIRE(r.ok());
}

// Opens the 3D window on c3d in layout-layer mode (it opens in mask-layer
// mode: an output style is current; since the mask-layer source landed,
// that mode exports the mask layers).
void open_layout(ClientSession& s) {
  s.ok("specialopen wind3d c3d");
  s.w3d("cif");
  REQUIRE(s.window().mode == view3d::Mode::layout_layer);
}

bool has_code(const hefesto::cmd::CommandResult& r, std::string_view code) {
  for (const hefesto::diag::Diagnostic& d : r.diagnostics) {
    if (d.code == code) {
      return true;
    }
  }
  return false;
}

std::string error_code(const hefesto::cmd::CommandResult& r) {
  return r.diagnostics.empty() ? std::string{} : r.diagnostics.front().code;
}

}  // namespace

TEST_CASE("glTF export: `export3d inv.glb` writes a valid glTF 2.0 binary, one node per visible layer, in µm",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  Session s;
  open_layout(s);
  const hefesto::cmd::CommandResult r = typed(s, "export3d /t/inv.glb");
  REQUIRE(r.ok());
  CHECK(r.value.to_string() == "/t/inv.glb");
  const std::string bytes = file(s, "/t/inv.glb");
  CHECK(bytes.starts_with("glTF"));
  const ParsedGltf glb(bytes);
  REQUIRE(glb.result == cgltf_result_success);
  REQUIRE(glb.validation == cgltf_result_success);
  CHECK(glb.data().file_type == cgltf_file_type_glb);
  const cgltf_node& root = *glb.data().scene->nodes[0];
  REQUIRE(root.children_count == 2);
  CHECK(std::string{root.children[0]->name} == "metal1");
  CHECK(std::string{root.children[1]->name} == "metal2");
  // The region is the bounding box grown by 1: origin (-1, -1) units = (-0.1, -0.1) µm.
  CHECK(root.translation[0] == Approx(-0.1));
  CHECK(root.translation[2] == Approx(0.1));
  const cgltf_accessor* metal1 = glb.positions("metal1");
  CHECK(static_cast<double>(metal1->max[0] - metal1->min[0]) == Approx(10.0));  // 100 units
  CHECK(static_cast<double>(metal1->max[2] - metal1->min[2]) == Approx(5.0));   // 50 units
}

TEST_CASE("`export3d x.gltf` writes the JSON and x.bin beside it; -format overrides the extension",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  Session s;
  open_layout(s);
  const hefesto::cmd::CommandResult r = typed(s, "export3d /t/inv.gltf");
  REQUIRE(r.ok());
  CHECK(r.value.to_string() == "/t/inv.bin /t/inv.gltf");
  const ParsedGltf json(file(s, "/t/inv.gltf"));
  REQUIRE(json.validation == cgltf_result_success);
  CHECK(std::string{json.data().buffers[0].uri} == "inv.bin");
  CHECK(json.data().buffers[0].size == file(s, "/t/inv.bin").size());

  typed_ok(s, "export3d /t/model.bin3 -format GLB");
  CHECK(file(s, "/t/model.bin3").starts_with("glTF"));
}

TEST_CASE("`export3d` STL: one file per layer named <stem>_<layer>.stl, or one with -merged",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  Session s;
  open_layout(s);
  const hefesto::cmd::CommandResult r = typed(s, "export3d /t/inv.stl");
  REQUIRE(r.ok());
  CHECK(r.value.to_string() == "/t/inv_metal1.stl /t/inv_metal2.stl");
  CHECK_FALSE(exists(s, "/t/inv.stl"));
  const view3d::test::StlFile m1 = view3d::test::read_stl(file(s, "/t/inv_metal1.stl"));
  const view3d::test::StlFile m2 = view3d::test::read_stl(file(s, "/t/inv_metal2.stl"));
  CHECK(m1.header.starts_with("Hefesto STL layer metal1"));
  CHECK(m2.header.starts_with("Hefesto STL layer metal2"));

  typed_ok(s, "export3d /t/all.stl -merged");
  const view3d::test::StlFile all = view3d::test::read_stl(file(s, "/t/all.stl"));
  CHECK(all.header.starts_with("Hefesto STL merged"));
  CHECK(all.triangles == m1.triangles + m2.triangles);
}

TEST_CASE("`export3d` follows the window's visibility; -all writes the hidden layers too",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  Session s;
  open_layout(s);  // layout-layer mode: `see` acts on tile types
  REQUIRE(s.w3d("see no metal2").ok());
  typed_ok(s, "export3d /t/v.glb");
  const ParsedGltf visible(file(s, "/t/v.glb"));
  CHECK(visible.node("metal1") != nullptr);
  CHECK(visible.node("metal2") == nullptr);
  typed_ok(s, "export3d /t/a.glb -all");
  const ParsedGltf all(file(s, "/t/a.glb"));
  CHECK(all.node("metal2") != nullptr);
  typed_ok(s, "export3d /t/v2.glb -all -visible");  // the last of the two wins
  CHECK(ParsedGltf(file(s, "/t/v2.glb")).node("metal2") == nullptr);
}

TEST_CASE("`export3d` exports the window's cut box", "[cap:hefesto-3d-visualization][req:3D export]") {
  Session s;
  open_layout(s);
  REQUIRE(s.w3d("cutbox 0 0 40 20").ok());
  typed_ok(s, "export3d /t/cut.glb");
  const ParsedGltf glb(file(s, "/t/cut.glb"));
  REQUIRE(glb.validation == cgltf_result_success);
  const cgltf_node& root = *glb.data().scene->nodes[0];
  CHECK(root.translation[0] == 0.0F);
  const cgltf_accessor* metal1 = glb.positions("metal1");
  CHECK(metal1->min[0] == 0.0F);
  CHECK(metal1->max[0] == Approx(4.0));   // clipped at x = 40
  CHECK(metal1->min[2] == Approx(-2.0));  // clipped at y = 20
  const cgltf_accessor* metal2 = glb.positions("metal2");
  CHECK(metal2->min[0] == Approx(2.0));
  CHECK(metal2->max[0] == Approx(4.0));
}

TEST_CASE("`export3d -zscale k` scales heights and records k", "[cap:hefesto-3d-visualization][req:3D export]") {
  Session s;
  open_layout(s);
  typed_ok(s, "export3d /t/z1.glb");
  typed_ok(s, "export3d /t/z3.glb -zscale 3");
  const ParsedGltf one(file(s, "/t/z1.glb"));
  const ParsedGltf three(file(s, "/t/z3.glb"));
  CHECK(three.thickness("metal1") == Approx(3 * one.thickness("metal1")).margin(1e-6));
  CHECK(three.asset_extras().find(R"("zscale":3)") != std::string::npos);
}

TEST_CASE("`export3d` with no 3D window open fails with VIEW3D-EXPORT-001 and writes nothing",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  Session s;
  const hefesto::cmd::CommandResult r = typed(s, "export3d /t/none.glb");
  CHECK_FALSE(r.ok());
  CHECK(error_code(r) == "VIEW3D-EXPORT-001");
  CHECK_FALSE(exists(s, "/t/none.glb"));
}

TEST_CASE("`export3d` argument errors are VIEW3D-EXPORT-002", "[cap:hefesto-3d-visualization][req:3D export]") {
  Session s;
  open_layout(s);
  for (const char* line : {"export3d", "export3d /t/a.obj", "export3d /t/a.glb -zscale 0",
                           "export3d /t/a.glb -zscale -2", "export3d /t/a.glb -zscale x", "export3d /t/a.glb -zscale",
                           "export3d /t/a.glb -format obj", "export3d /t/a.glb -bogus", "export3d /t/a.glb /t/b.glb"}) {
    INFO(line);
    const hefesto::cmd::CommandResult r = typed(s, line);
    CHECK_FALSE(r.ok());
    CHECK(error_code(r) == "VIEW3D-EXPORT-002");
  }
  CHECK_FALSE(exists(s, "/t/a.glb"));
}

TEST_CASE("`export3d` on a window whose cell is gone fails with VIEW3D-EXPORT-004",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  Session s;
  open_layout(s);
  s.window().cell = "no_such_cell";
  const hefesto::cmd::CommandResult r = typed(s, "export3d /t/gone.glb");
  CHECK(error_code(r) == "VIEW3D-EXPORT-004");
  CHECK_FALSE(exists(s, "/t/gone.glb"));
}

TEST_CASE("export3d in mask-layer mode writes the mask layers in µm: mask units divided by the scale factor",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  // Regression: the mask-layer mesh is in mask units (scalefactor 10: 0.01 µm
  // each), so without MeshModel::units_per_root the export was 10 times too
  // large. MET1 has `render MET1 metal1 1.376 0.36`; MET2 has no render
  // height and is left out with VIEW3D-STACK-001. Same in both modes.
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    ClientSession s{compat};  // no display styles: every mask layer counts as drawn
    s.editor.styles.reset();
    s.ok("specialopen wind3d c3d");
    REQUIRE(s.window().mode == view3d::Mode::mask_layer);
    const hefesto::cmd::CommandResult r = typed(s, "export3d /t/mask.glb");
    REQUIRE(r.ok());
    CHECK(has_code(r, "VIEW3D-STACK-001"));
    const ParsedGltf glb(file(s, "/t/mask.glb"));
    REQUIRE(glb.validation == cgltf_result_success);
    const cgltf_node& root = *glb.data().scene->nodes[0];
    REQUIRE(root.children_count == 1);
    CHECK(std::string{root.children[0]->name} == "MET1");
    const cgltf_accessor* met1 = glb.positions("MET1");
    CHECK(static_cast<double>(met1->max[0] - met1->min[0]) == Approx(10.0));  // 100 internal units
    CHECK(static_cast<double>(met1->max[2] - met1->min[2]) == Approx(5.0));   // 50 internal units
    CHECK(glb.thickness("MET1") == Approx(0.36).margin(1e-6));
  }
}

// Review of PR #122 (finding 4): `.gltf` writes <stem>.bin only when there
// is geometry; with no exported layer it writes the JSON alone.
TEST_CASE("`export3d x.gltf` with no exported layer writes the JSON only, with no buffer and no x.bin",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  Session s;
  open_layout(s);
  REQUIRE(s.w3d("see no metal1").ok());
  REQUIRE(s.w3d("see no metal2").ok());
  const hefesto::cmd::CommandResult r = typed(s, "export3d /t/empty.gltf");
  REQUIRE(r.ok());
  CHECK(r.value.to_string() == "/t/empty.gltf");
  CHECK_FALSE(exists(s, "/t/empty.bin"));
  const ParsedGltf json(file(s, "/t/empty.gltf"));
  REQUIRE(json.validation == cgltf_result_success);
  CHECK(json.data().buffers_count == 0);
  CHECK(json.data().scene->nodes[0]->children_count == 0);
}

// Review of PR #122 (finding 3): names were made safe after the duplicate
// check, so `a/b` and `a_b` (and, on a case-insensitive file system, `MET1`
// and `met1`) were written to one file, the later replacing the earlier.
TEST_CASE("`export3d` STL per-layer file names are unique ignoring case",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  const std::vector<std::string> files =
      view3d::layer_file_names("inv", {"a/b", "a_b", "MET1", "met1", "a_b_2", "metal2"});
  CHECK(files == std::vector<std::string>{"inv_a_b.stl", "inv_a_b_2.stl", "inv_MET1.stl", "inv_met1_2.stl",
                                          "inv_a_b_2_2.stl", "inv_metal2.stl"});
}

namespace {

// The WIND3D technology with a temporary layer HALO (metal1 grown by 5)
// that has a `render` height, so that only the temporary-layer rule can
// leave it out of an export.
std::string halo_tech() {
  std::string tech{view3d::test::kTech};
  const std::string anchor = "    render MET1 metal1 1.376 0.36\n";
  const std::size_t at = tech.find(anchor);
  REQUIRE(at != std::string::npos);
  tech.insert(at + anchor.size(), "    templayer HALO m1\n        grow 5\n    render HALO metal1 0.5 0.1\n");
  return tech;
}

}  // namespace

// Review of PR #122 (finding 5): with magic-compat OFF the temporary layers
// were left out through the window's hidden flags, which `-all` ignores, so
// `export3d -all` wrote them. They are no masks: never exported OFF; ON
// draws and exports them, as Magic draws every CIF layer.
TEST_CASE("`export3d -all` in mask-layer mode: temporary layers are exported with magic-compat ON only",
          "[cap:hefesto-3d-visualization][req:3D export]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    ClientSession s{compat, halo_tech()};
    s.editor.styles.reset();
    s.ok("specialopen wind3d c3d");
    REQUIRE(s.window().mode == view3d::Mode::mask_layer);
    typed_ok(s, "export3d /t/all.glb -all");
    typed_ok(s, "export3d /t/visible.glb");
    const ParsedGltf all(file(s, "/t/all.glb"));
    REQUIRE(all.validation == cgltf_result_success);
    CHECK(all.node("MET1") != nullptr);
    CHECK((all.node("HALO") != nullptr) == compat);
    const ParsedGltf visible(file(s, "/t/visible.glb"));
    CHECK((visible.node("HALO") != nullptr) == compat);
  }
}
