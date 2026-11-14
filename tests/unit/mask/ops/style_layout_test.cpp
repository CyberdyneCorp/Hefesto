// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The kernels read the operands as the style reader stores them: a style
// read from a technology file (tests/data/mask/style/styles.tech) is
// evaluated on painted layout, so a mismatch between the LayerOp layout of
// mask/style and the one mask/ops reads shows up as wrong geometry.
// Owner: INTEGRATION (openspec/changes/m5-mask-gds-cif/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/mask/ops/evaluate.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"

namespace {

namespace db = hefesto::db;
namespace diag = hefesto::diag;
namespace mask = hefesto::mask;
namespace tech = hefesto::tech;
using Rects = std::vector<db::Rect>;

std::shared_ptr<const tech::Technology> tech_with(const std::string& cifoutput) {
  std::ifstream in(std::filesystem::path{HEFESTO_TEST_DATA_DIR}.parent_path() / "style" / "styles.tech");
  REQUIRE(in.good());
  std::ostringstream text;
  text << in.rdbuf() << "\ncifoutput\n" << cifoutput << "\nend\n\ncifinput\nend\n\ndrc\nend\n\nextract\nend\n";
  hefesto::platform::MemoryFileSystem fs;
  diag::CollectingSink sink;
  auto loaded = tech::load_technology_text(text.str(), "/t/styles.tech", fs, tech::LoadOptions{}, sink);
  REQUIRE(loaded.ok());
  return loaded.value();
}

/// Every layer of the first style read from `cifoutput`, evaluated in order
/// over `layout` painted by `paint`; returns the strips of each layer.
struct Generated {
  std::shared_ptr<const tech::Technology> tech;
  std::unique_ptr<db::CellDef> cell;
  mask::OutputStyleSet set;

  explicit Generated(const std::string& cifoutput) : tech(tech_with(cifoutput)) {
    cell = std::make_unique<db::CellDef>("top", tech->plane_count());
    diag::CollectingSink sink;
    set = mask::read_output_styles(*tech, sink);
    REQUIRE_FALSE(set.styles.empty());
  }

  void paint(std::string_view type, const db::Rect& r) {
    const auto found = tech->find_type(type);
    REQUIRE(found.ok());
    REQUIRE(db::paint_type(*cell, *tech, r, *found.id).ok());
    db::recompute_bbox(*cell);
  }

  [[nodiscard]] std::vector<Rects> run() const {
    const mask::OutputStyle& style = *set.styles.front();
    mask::PlanePool pool;
    diag::CollectingSink sink;
    mask::OperatorContext ctx;
    ctx.pool = &pool;
    ctx.sink = &sink;
    ctx.cell = cell.get();
    std::vector<mask::MaskPlane*> planes(style.layers.size(), nullptr);
    std::vector<Rects> out;
    for (std::size_t i = 0; i < style.layers.size(); ++i) {
      const mask::LayerInputs in{cell.get(), std::span<mask::MaskPlane* const>{planes}, tech.get()};
      planes[i] = &mask::evaluate_layer(style, i, in, db::Rect{-1000, -1000, 1000, 1000}, ctx);
      out.push_back(planes[i]->solid_rects());
    }
    return out;
  }
};

}  // namespace

TEST_CASE("read operators drive the kernels with their distances",
          "[cap:mask-output-operators][req:grow and shrink (default, square metric)]") {
  Generated g{"style s\nscalefactor 1\nlayer G metal1\ngrow 3\nlayer S metal1\nshrink 2"};
  REQUIRE(g.set.styles.front()->scale.scale_factor == 1);
  g.paint("metal1", db::Rect{0, 0, 10, 10});
  const std::vector<Rects> out = g.run();
  CHECK(out[0] == Rects{{-3, -3, 13, 13}});
  CHECK(out[1] == Rects{{2, 2, 8, 8}});
}

TEST_CASE("read bloat tables drive the bloat kernels", "[cap:mask-output-operators][req:bloat-or]") {
  Generated g{"style s\nscalefactor 1\nlayer B ndiff\nbloat-or ndiff poly 4"};
  g.paint("ndiff", db::Rect{0, 0, 10, 10});
  g.paint("poly", db::Rect{10, 0, 20, 10});
  const std::vector<Rects> out = g.run();
  // Only the edge next to poly moves, by poly's distance.
  CHECK(out[0] == Rects{{0, 0, 14, 10}});
}

TEST_CASE("read bridge operands drive the bridge kernel", "[cap:mask-output-operators][req:bridge]") {
  Generated g{"style s\nscalefactor 1\nlayer M metal1\nbridge 5 4"};
  g.paint("metal1", db::Rect{0, 0, 10, 10});
  g.paint("metal1", db::Rect{12, 12, 22, 22});
  const std::vector<Rects> out = g.run();
  // A diagonal gap under the spacing gets a bridge: more than the two squares.
  CHECK(out[0].size() > 2);
}
