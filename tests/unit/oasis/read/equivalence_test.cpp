// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The GDS-equivalence cross-check as a unit property test (tasks.md 11.5,
// 11.8): for seeded random layouts L,
// - L is read from GDS (gds::read) and from an OASIS file the codec's
//   Encoder writes from the same description (each construct as
//   oasis-write writes it, plus list repetitions, which oasis-write never
//   writes), and the .mag text of every cell is compared, timestamp lines
//   excluded;
// - the cells read from L are written by Hefesto as GDS (gds::write) and
//   as OASIS (oasis::write), each file is read into a fresh session, and
//   the .mag texts are compared.
// Owner: OASR (the oasis::write half: OASW, task 11.8).

#include <catch2/catch_test_macros.hpp>

#include <random>

#include "hefesto/gds/write/write.hpp"
#include "hefesto/oasis/write/write.hpp"

#include "oasis_read_fixture.hpp"

using namespace oasis_read_test;

namespace {

/// The layer pairs of the random shapes: mapped (metal1, the metal1 pin
/// and label pairs, metal2, poly) and one unmapped pair.
constexpr std::array<std::pair<int, int>, 6> kPairs{{{68, 20}, {68, 16}, {68, 5}, {69, 20}, {66, 20}, {99, 0}}};

/// The output style of the write-back half: the read style's layers.
constexpr std::string_view kOutput =
    "style out\n"
    "scalefactor 1\n"
    "layer M1 m1\n"
    "labels m1\n"
    "calma 68 20\n"
    "layer M2 m2\n"
    "labels m2\n"
    "calma 69 20\n"
    "layer PO poly\n"
    "labels poly\n"
    "calma 66 20\n";

/// The bytes of file `path` of `s`.
Bytes file_bytes(OasSession& s, const std::string& path) {
  const std::string text = s.fs.read(path).value();
  const auto* first = reinterpret_cast<const std::byte*>(text.data());
  return Bytes(first, first + text.size());
}

std::uint64_t u(int v) {
  return static_cast<std::uint64_t>(v);
}

/// One random layout, emitted to both formats as it is generated.
class Layout {
 public:
  Layout(std::uint32_t seed, bool strict, std::int32_t compress) : rng_(seed), oas_(strict, compress) {
    gds_.library(top_name(), 1e-8);
  }

  void build() {
    for (std::size_t i = 0; i < cells_; ++i) {
      const std::string name = "c" + std::to_string(i);
      oas_.cell(name);
      gds_.begin(name, {70, 1, 1, 0, 0, 0, 70, 1, 1, 0, 0, 0});
      const int elements = pick(1, 6);
      first_ = true;
      for (int e = 0; e < elements; ++e) {
        element(i);
      }
      gds_.end();
    }
    gds_.end_library();
  }

  Bytes oasis_bytes() { return oas_.bytes(); }
  [[nodiscard]] const GdsFile& gds() const { return gds_; }

 private:
  int pick(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng_); }

  /// Each cell places the next one first, so c0 is the only unplaced
  /// cell: the top cell, and the GDS library name.
  static std::string top_name() { return "c0"; }

  void element(std::size_t cell) {
    if (first_ && cell + 1 < cells_) {
      first_ = false;
      placement(cell, cell + 1);
      return;
    }
    switch (pick(0, 6)) {
      case 0:
        rectangle();
        return;
      case 1:
        triangle();
        return;
      case 2:
        wire();
        return;
      case 3:
        label();
        return;
      case 4:
        repeated_rectangle();
        return;
      default:
        if (cell + 1 < cells_) {
          placement(cell, static_cast<std::size_t>(pick(static_cast<int>(cell) + 1, static_cast<int>(cells_) - 1)));
        } else {
          rectangle();
        }
        return;
    }
  }

  std::pair<int, int> pair() { return kPairs[static_cast<std::size_t>(pick(0, kPairs.size() - 1))]; }

  void rectangle() {
    const auto [l, d] = pair();
    const int x = pick(-300, 300);
    const int y = pick(-300, 300);
    const int w = pick(1, 120);
    const int h = pick(1, 120);
    oas_.add(rect(u(l), u(d), x, y, w, h));
    gds_.boundary(l, d, {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}, {x, y}});
  }

  void repeated_rectangle() {
    const auto [l, d] = pair();
    const int x = pick(-300, 300);
    const int w = pick(1, 40);
    const int step = pick(w, 80);
    const int n = pick(2, 4);
    oas_.add(with(rect(u(l), u(d), x, 0, w, 10), grid(2, static_cast<std::uint64_t>(n), {step, 0})));
    for (int i = 0; i < n; ++i) {
      const int xi = x + i * step;
      gds_.boundary(l, d, {{xi, 0}, {xi + w, 0}, {xi + w, 10}, {xi, 10}, {xi, 0}});
    }
  }

  void triangle() {
    const auto [l, d] = pair();
    const int x = pick(-300, 300);
    const int y = pick(-300, 300);
    const int s = pick(2, 60);
    oas_.add(polygon(u(l), u(d), x, y, {{s, 0}, {s, s}}));
    gds_.boundary(l, d, {{x, y}, {x + s, y}, {x + s, y + s}, {x, y}});
  }

  void wire() {
    const auto [l, d] = pair();
    const int x = pick(-300, 300);
    const int y = pick(-300, 300);
    const int hw = pick(1, 8);
    const int len = pick(10, 200);
    const bool vertical = pick(0, 1) == 1;
    const int ends = pick(0, 2);  // flush, half-width, explicit
    const int es = ends == 0 ? 0 : ends == 1 ? hw : pick(0, 12);
    const int ee = ends == 0 ? 0 : ends == 1 ? hw : pick(0, 12);
    const oasis::Point64 to = vertical ? oasis::Point64{0, len} : oasis::Point64{len, 0};
    oas_.add(path(u(l), u(d), hw, es, ee, x, y, {to}));
    const int type = es == 0 && ee == 0 ? 0 : (es == hw && ee == hw ? 2 : 4);
    PathSpec spec{type, 2 * hw, {}, {}};
    if (type == 4) {
      spec.begin = es;
      spec.end = ee;
    }
    gds_.path(l, d, spec, {{x, y}, {x + static_cast<int>(to.x), y + static_cast<int>(to.y)}});
  }

  void label() {
    const bool pin = pick(0, 1) == 1;
    const int x = pick(-300, 300);
    const int y = pick(-300, 300);
    const std::string s = "n" + std::to_string(pick(0, 99));
    const int layer = 68;
    const int type = pin ? 5 : 20;
    oas_.add(text(u(layer), u(type), x, y, s));
    gds_.text(layer, type, {x, y}, s);
  }

  void placement(std::size_t /*parent*/, std::size_t child) {
    const std::string name = "c" + std::to_string(child);
    const double angle = 90.0 * pick(0, 3);
    const bool flip = pick(0, 1) == 1;
    const int x = pick(-500, 500);
    const int y = pick(-500, 500);
    RefSpec spec{flip ? 0x8000 : 0, {}, angle == 0.0 ? std::nullopt : std::optional<double>{angle}, {}};
    const int kind = pick(0, 2);
    const std::string id = "i" + std::to_string(++ids_);
    spec.properties = {{61, id}};
    if (kind == 0) {
      oas_.add(place(name, x, y, angle, flip)).property(gds_property(61, id));
      gds_.sref(name, {x, y}, spec);
      return;
    }
    if (kind == 1) {
      array(name, x, y, angle, flip, spec, id);
      return;
    }
    const int dx = pick(1, 50) * 7;
    oas_.add(with(place(name, x, y, angle, flip), list(4, {{dx, 0}, {3 * dx, 0}}))).property(gds_property(61, id));
    for (const int off : {0, dx, 3 * dx}) {
      gds_.sref(name, {x + off, y}, spec);
    }
  }

  /// A Magic array as oasis-write writes it: the column vector along the
  /// use's x axis in the parent, the row vector along its y axis.
  void array(const std::string& name, int x, int y, double angle, bool flip, const RefSpec& spec,
             const std::string& id) {
    const int nx = pick(2, 4);
    const int ny = pick(2, 3);
    const int sx = pick(15, 60);
    const int sy = pick(15, 60);
    // T applied to (sx, 0) and (0, sy): the flip negates y, then the rotation.
    const auto t = [&](int ux, int uy) -> oasis::Point64 {
      const int fy = flip ? -uy : uy;
      if (angle == 90.0) {
        return {-fy, ux};
      }
      if (angle == 180.0) {
        return {-ux, -fy};
      }
      if (angle == 270.0) {
        return {fy, -ux};
      }
      return {ux, fy};
    };
    const oasis::Point64 a = t(sx, 0);
    const oasis::Point64 b = t(0, sy);
    const oasis::Repetition r = grid(8, static_cast<std::uint64_t>(nx), a, static_cast<std::uint64_t>(ny), b);
    oas_.add(with(place(name, x, y, angle, flip), r)).property(gds_property(61, id));
    const auto px = static_cast<int>(a.x) * nx;
    const auto py = static_cast<int>(a.y) * nx;
    const auto qx = static_cast<int>(b.x) * ny;
    const auto qy = static_cast<int>(b.y) * ny;
    gds_.aref(name, nx, ny, {{x, y}, {x + px, y + py}, {x + qx, y + qy}}, spec);
  }

  std::mt19937 rng_;
  Oas oas_;
  GdsFile gds_;
  std::size_t cells_ = 4;
  bool first_ = false;
  int ids_ = 0;
};

}  // namespace

TEST_CASE("GDS-equivalence cross-check", "[cap:oasis-read][req:GDS-equivalence cross-check]") {
  for (std::uint32_t seed = 1; seed <= 24; ++seed) {
    for (const bool magic_compat : {true, false}) {
      const bool strict = seed % 2 == 0;
      const std::int32_t compress = seed % 3 == 0 ? 6 : 0;
      Layout layout(seed, strict, compress);
      layout.build();
      OasSession from_gds(kStyle, magic_compat);
      from_gds.read(layout.gds());
      OasSession from_oasis(kStyle, magic_compat);
      (void)from_oasis.read_oasis(layout.oasis_bytes());
      INFO("seed " << seed << " magic-compat " << magic_compat << " strict " << strict << " compress " << compress);
      const auto expected = all_mags(from_gds);
      const auto actual = all_mags(from_oasis);
      REQUIRE(actual.size() == expected.size());
      for (std::size_t i = 0; i < expected.size(); ++i) {
        INFO("cell " << expected[i].first);
        CHECK(actual[i].first == expected[i].first);
        CHECK(actual[i].second == expected[i].second);
      }
      CHECK(from_oasis.last.value.to_string() == from_gds.last.value.to_string());
    }
  }
}

TEST_CASE("GDS-equivalence cross-check of Hefesto's GDS and OASIS writes",
          "[cap:oasis-read][req:GDS-equivalence cross-check]") {
  for (std::uint32_t seed = 1; seed <= 24; ++seed) {
    for (const bool magic_compat : {true, false}) {
      Layout layout(seed, seed % 2 == 0, 0);
      layout.build();
      OasSession source(kStyle, magic_compat, kOutput);
      (void)source.read(layout.gds());
      db::CellDef* root = source.editor.cells.find("c0");
      REQUIRE(root != nullptr);
      oasis::OasisWriteOptions o;
      o.settings.strict = seed % 2 == 0;
      o.settings.compress = seed % 3 == 0 ? 6 : 0;
      o.settings.repetitions = seed % 4 != 0;
      o.prepare = oasis::to_gds_write_options(o.settings, {});
      o.prepare.files = &source.fs;
      o.prepare.time_zone = "UTC0";
      const gds::GdsWriteOptions g = o.prepare;
      INFO("seed " << seed << " magic-compat " << magic_compat);
      REQUIRE(gds::write(source.editor, *root, "/data/back.gds", g).ok());
      REQUIRE(oasis::write(source.editor, *root, "/data/back.oas", o).ok());

      OasSession from_gds(kStyle, magic_compat);
      gds::MemorySource gds_bytes{file_bytes(source, "/data/back.gds")};
      from_gds.last = gds::read(from_gds.editor, gds_bytes, from_gds.options);
      OasSession from_oasis(kStyle, magic_compat);
      (void)from_oasis.read_oasis(file_bytes(source, "/data/back.oas"));
      const auto expected = all_mags(from_gds);
      const auto actual = all_mags(from_oasis);
      REQUIRE(actual.size() == expected.size());
      REQUIRE(expected.size() >= 2);
      for (std::size_t i = 0; i < expected.size(); ++i) {
        INFO("cell " << expected[i].first);
        CHECK(actual[i].first == expected[i].first);
        CHECK(actual[i].second == expected[i].second);
      }
    }
  }
}
