// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Byte stability of exported files (M10 design D14, task 11.4;
// hefesto-testing-strategy "Cross-platform determinism checks"): a fixed
// reference model (two chunks, offsets off the unit grid of µm, split-tile
// triangles, an own-height side face, a style set, a z scale) is written as
// GLB, .gltf with its .bin, embedded .gltf, merged STL and per-layer STL;
// their SHA-256 digests must equal tests/data/view3d/export/digests.txt on
// every CI leg (Linux x86_64, Linux aarch64, macOS arm64). With the
// environment variable HEFESTO_EXPORT3D_OUT set, the files are also written
// to that directory (the export3d-determinism workflow compares them across
// legs and runs the Khronos glTF-Validator on them). Owner: EXPORT.

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "export_fixture.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/pdk/sha256.hpp"

namespace view3d = hefesto::view3d;
namespace diag = hefesto::diag;
namespace gds = hefesto::gds;

namespace {

struct Reference {
  hefesto::display::StyleSet styles = view3d::test::two_styles();
  view3d::LayerStack stack;
  view3d::MeshModel mesh;
};

// sky130-like values: 0.005 µm per internal unit; heights in internal units.
Reference reference_model() {
  Reference r;
  const std::size_t metal = hefesto::display::kFixedStyleCount;
  r.stack = view3d::LayerStack{view3d::Mode::layout_layer,
                               {view3d::test::layout_layer("poly", 66.0F, 36.0F, metal + 1),
                                view3d::test::layout_layer("li1", 188.0F, 20.0F, metal),
                                view3d::test::layout_layer("met1", 275.2F, 72.0F, metal)},
                               true,
                               0.005};
  r.mesh = view3d::test::one_chunk_mesh(view3d::Mode::layout_layer, hefesto::db::Rect{-1003, -777, 2049, 1531}, r.stack);
  view3d::Chunk& a = r.mesh.chunks[0];
  view3d::test::add_box(a, 0, 13.0F, 7.0F, 333.0F, 101.0F);
  view3d::test::add_box(a, 1, 0.0F, 0.0F, 1021.0F, 517.0F);
  a.triangles.push_back(view3d::Triangle{{100.0F, 300.0F, 100.0F}, {200.0F, 200.0F, 450.0F}, 1, 188.0F, 20.0F});
  a.sides.push_back(view3d::SideInstance{100.0F, 450.0F, 300.0F, 200.0F, 1,
                                         view3d::side_flag::outward_left | view3d::side_flag::diagonal |
                                             view3d::side_flag::own_height,
                                         66.0F, 36.0F});
  view3d::Chunk b;
  b.origin_x = 1021.0 - 1003.0;
  b.origin_y = -777.0;
  b.area = hefesto::db::Rect{18, -777, 2049, 1531};
  view3d::test::add_box(b, 2, 3.0F, 5.0F, 2031.0F, 2303.0F);
  view3d::test::add_box(b, 1, 77.0F, 91.0F, 777.0F, 1001.0F);
  r.mesh.chunks.push_back(std::move(b));
  r.mesh.layer_heights[2] = view3d::DrawHeight{280.0F, 72.0F};  // met1 spanned (J1)
  return r;
}

std::string bytes_of(const gds::MemorySink& sink) {
  return view3d::test::text_of(sink.bytes());
}

std::map<std::string, std::string> reference_files() {
  Reference r = reference_model();
  std::map<std::string, std::string> files;
  diag::NullSink quiet;
  view3d::ExportOptions options;
  options.styles = &r.styles;
  options.zscale = 2.5;
  {
    gds::MemorySink out;
    options.format = view3d::ExportFormat::glb;
    REQUIRE(view3d::write_gltf(r.mesh, r.stack, options, out, quiet).ok());
    files["reference.glb"] = bytes_of(out);
  }
  {
    gds::MemorySink json;
    gds::MemorySink bin;
    options.format = view3d::ExportFormat::gltf;
    options.bin_out = &bin;
    options.bin_uri = "reference.bin";
    REQUIRE(view3d::write_gltf(r.mesh, r.stack, options, json, quiet).ok());
    files["reference.gltf"] = bytes_of(json);
    files["reference.bin"] = bytes_of(bin);
    options.bin_out = nullptr;
  }
  {
    gds::MemorySink out;
    options.zscale.reset();
    REQUIRE(view3d::write_gltf(r.mesh, r.stack, options, out, quiet).ok());
    files["embedded.gltf"] = bytes_of(out);
  }
  {
    gds::MemorySink out;
    options.format = view3d::ExportFormat::stl;
    REQUIRE(view3d::write_stl(r.mesh, r.stack, options, out, quiet).ok());
    files["merged.stl"] = bytes_of(out);
  }
  for (const view3d::ExportLayer& layer : view3d::export_layers(r.mesh, r.stack, options, quiet)) {
    gds::MemorySink out;
    options.stl_layer = layer.layer;
    REQUIRE(view3d::write_stl(r.mesh, r.stack, options, out, quiet).ok());
    files["reference_" + layer.name + ".stl"] = bytes_of(out);
  }
  return files;
}

std::map<std::string, std::string> expected_digests() {
  std::ifstream in(std::filesystem::path{HEFESTO_TEST_DATA_DIR} / "digests.txt");
  REQUIRE(in.good());
  std::map<std::string, std::string> out;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line.front() == '#') {
      continue;
    }
    std::istringstream words(line);
    std::string digest;
    std::string name;
    words >> digest >> name;
    out[name] = digest;
  }
  return out;
}

void write_out(const std::map<std::string, std::string>& files) {
  const char* dir = std::getenv("HEFESTO_EXPORT3D_OUT");
  if (dir == nullptr || *dir == '\0') {
    return;
  }
  std::filesystem::create_directories(dir);
  for (const auto& [name, bytes] : files) {
    std::ofstream out(std::filesystem::path{dir} / name, std::ios::binary);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    REQUIRE(out.good());
  }
}

}  // namespace

TEST_CASE("Exported files are byte-identical across platforms: SHA-256 digests of the reference model",
          "[cap:hefesto-3d-visualization][req:3D export][cap:hefesto-testing-strategy]"
          "[req:Cross-platform determinism checks]") {
  const std::map<std::string, std::string> files = reference_files();
  write_out(files);
  const std::map<std::string, std::string> expected = expected_digests();
  std::string actual;
  for (const auto& [name, bytes] : files) {
    hefesto::pdk::Sha256 sha;
    sha.update(bytes);
    actual += sha.finish_hex() + "  " + name + "\n";
  }
  INFO("digests of this platform (sha256sum format):\n" << actual);
  CHECK(files.size() == expected.size());
  for (const auto& [name, bytes] : files) {
    hefesto::pdk::Sha256 sha;
    sha.update(bytes);
    const std::string want = expected.count(name) != 0 ? expected.at(name) : std::string{"(missing)"};
    INFO(name);
    CHECK(sha.finish_hex() == want);
  }
}

TEST_CASE("Writing the same model twice gives the same bytes", "[cap:hefesto-3d-visualization][req:3D export]") {
  CHECK(reference_files() == reference_files());
}
