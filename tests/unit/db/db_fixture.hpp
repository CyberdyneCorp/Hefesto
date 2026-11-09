// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared helpers of the MODEL unit tests: paint rows for bare planes, an
// independent reference model of Magic's canonical decomposition and
// enumeration order (written from the spec text, not from the port), and
// the golden test technology `mt` of tests/golden/MODEL/CASES.md built
// through the tech contract.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "hefesto/db/plane.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::db::test {

/// Painting `t` replaces anything (a non-contact type on its plane).
inline PaintRow paint_row_of(tech::TileType t) {
  PaintRow row{};
  row.fill(t);
  return row;
}

/// Erasing `t` leaves space where `t` was.
inline PaintRow erase_row_of(tech::TileType t) {
  PaintRow row{};
  for (std::size_t i = 0; i < row.size(); ++i) {
    row[i] = static_cast<tech::TileType>(i == t ? tech::kSpace : i);
  }
  return row;
}

/// A portable pseudo-random sequence (64-bit LCG, Knuth's MMIX constants).
/// Unlike std::uniform_int_distribution and std::shuffle, whose results differ
/// between libc++ and libstdc++, it gives the same values on every platform,
/// so seeded test histories are identical on every CI leg.
class Lcg {
 public:
  explicit Lcg(std::uint64_t seed) : state_(seed) {}

  /// The next 32 high-quality bits.
  std::uint32_t next() {
    state_ = state_ * 6364136223846793005ULL + 1442695040888963407ULL;
    return static_cast<std::uint32_t>(state_ >> 32U);
  }

  /// A value in [lo, hi] (the slight modulo bias is irrelevant for tests).
  int uniform(int lo, int hi) {
    const auto span = static_cast<std::uint32_t>(hi - lo) + 1U;
    return lo + static_cast<int>(next() % span);
  }

  /// Fisher-Yates shuffle driven by this sequence.
  template <typename T>
  void shuffle(std::vector<T>& v) {
    for (std::size_t i = v.size(); i > 1; --i) {
      const std::size_t j = next() % i;
      std::swap(v[i - 1], v[j]);
    }
  }

 private:
  std::uint64_t state_;
};

inline tech::TypeMask non_space() {
  return ~tech::TypeMask::of(tech::kSpace);
}

/// Non-space tiles of `plane` in enumeration order over the whole plane.
inline std::vector<TileView> tiles(const Plane& plane, const tech::TypeMask& mask = non_space()) {
  std::vector<TileView> out;
  plane.enumerate(whole_plane(), mask, [&](const TileView& t) {
    out.push_back(t);
    return true;
  });
  return out;
}

inline std::vector<Rect> rects(const Plane& plane, const tech::TypeMask& mask = non_space()) {
  std::vector<Rect> out;
  for (const TileView& t : tiles(plane, mask)) {
    out.push_back(t.area);
  }
  return out;
}

/// A raster of types over a bounded window, the reference for Manhattan
/// geometry. Everything outside the window is space.
class Raster {
 public:
  explicit Raster(Rect window) : w_(window), cells_(static_cast<std::size_t>(window.width() * window.height())) {}

  void paint(const Rect& r, tech::TileType t) {
    for (Coord y = std::max(r.y0, w_.y0); y < std::min(r.y1, w_.y1); ++y) {
      for (Coord x = std::max(r.x0, w_.x0); x < std::min(r.x1, w_.x1); ++x) {
        at(x, y) = t;
      }
    }
  }
  void erase(const Rect& r, tech::TileType t) {
    for (Coord y = std::max(r.y0, w_.y0); y < std::min(r.y1, w_.y1); ++y) {
      for (Coord x = std::max(r.x0, w_.x0); x < std::min(r.x1, w_.x1); ++x) {
        if (at(x, y) == t) {
          at(x, y) = tech::kSpace;
        }
      }
    }
  }
  [[nodiscard]] tech::TileType type(Coord x, Coord y) const {
    if (x < w_.x0 || x >= w_.x1 || y < w_.y0 || y >= w_.y1) {
      return tech::kSpace;
    }
    return cells_[index(x, y)];
  }

  struct Tile {
    Rect r;
    tech::TileType type;
  };

  /// "Canonical maximal-horizontal-strip decomposition", from its
  /// definition: maximal runs per row, vertically joined when x1, x2 and
  /// the type agree.
  [[nodiscard]] std::vector<Tile> decomposition() const {
    std::vector<Tile> done;
    std::vector<Tile> open;  // tiles ending at the current row
    for (Coord y = w_.y0; y < w_.y1; ++y) {
      std::vector<Tile> next;
      Coord x = w_.x0;
      while (x < w_.x1) {
        const tech::TileType t = type(x, y);
        Coord e = x;
        while (e < w_.x1 && type(e, y) == t) {
          ++e;
        }
        auto it = std::find_if(open.begin(), open.end(),
                               [&](const Tile& o) { return o.r.x0 == x && o.r.x1 == e && o.type == t; });
        if (it != open.end()) {
          Tile grown = *it;
          grown.r.y1 = y + 1;
          open.erase(it);
          next.push_back(grown);
        } else {
          next.push_back(Tile{Rect{x, y, e, y + 1}, t});
        }
        x = e;
      }
      done.insert(done.end(), open.begin(), open.end());
      open = std::move(next);
    }
    done.insert(done.end(), open.begin(), open.end());
    return done;
  }

  /// "Tile enumeration order" over the window, from its definition: roots
  /// on the left edge top to bottom; parent of R contains
  /// (R.x0 - 1, R.y0); children top to bottom, depth first.
  [[nodiscard]] std::vector<Tile> enumeration() const {
    const std::vector<Tile> all = decomposition();
    constexpr std::size_t kNone = static_cast<std::size_t>(-1);
    const auto containing = [&](Coord x, Coord y) {
      for (std::size_t i = 0; i < all.size(); ++i) {
        const Rect& r = all[i].r;
        if (r.x0 <= x && x < r.x1 && r.y0 <= y && y < r.y1) {
          return i;
        }
      }
      return kNone;
    };
    std::map<std::size_t, std::vector<std::size_t>> children;
    std::vector<std::size_t> roots;
    for (std::size_t i = 0; i < all.size(); ++i) {
      if (all[i].r.x0 == w_.x0) {
        roots.push_back(i);
      } else {
        children[containing(all[i].r.x0 - 1, all[i].r.y0)].push_back(i);
      }
    }
    const auto top_down = [&](std::size_t a, std::size_t b) { return all[a].r.y1 > all[b].r.y1; };
    std::sort(roots.begin(), roots.end(), top_down);
    std::vector<Tile> order;
    std::function<void(std::size_t)> visit = [&](std::size_t i) {
      order.push_back(all[i]);
      auto& c = children[i];
      std::sort(c.begin(), c.end(), top_down);
      for (const std::size_t k : c) {
        visit(k);
      }
    };
    for (const std::size_t r : roots) {
      visit(r);
    }
    return order;
  }

  [[nodiscard]] std::vector<Rect> non_space_order() const {
    std::vector<Rect> out;
    for (const Tile& t : enumeration()) {
      if (t.type != tech::kSpace) {
        out.push_back(t.r);
      }
    }
    return out;
  }

 private:
  [[nodiscard]] std::size_t index(Coord x, Coord y) const {
    return static_cast<std::size_t>((std::int64_t{y} - w_.y0) * w_.width() + (x - w_.x0));
  }
  tech::TileType& at(Coord x, Coord y) { return cells_[index(x, y)]; }

  Rect w_;
  std::vector<tech::TileType> cells_;
};

/// The test technology `mt` of tests/golden/MODEL/CASES.md, built through
/// tech::TechnologyBuilder. Returns null (and the test skips the
/// technology-dependent checks) when the tech contract cannot build it yet.
struct MtTech {
  std::shared_ptr<const tech::Technology> tech;
  tech::PlaneId active = 0, metal1 = 0, metal2 = 0, metal3 = 0;
  tech::TileType ndiff = 0, pdiff = 0, poly = 0, nfet = 0, m1 = 0, m2 = 0, m3 = 0, pdc = 0, via1 = 0, via2 = 0;
};

inline std::optional<MtTech> build_mt() {
  tech::TechnologyBuilder b;
  b.set_name("mt");
  MtTech mt;
  const auto plane = [&](std::string_view n, tech::PlaneId& id) {
    auto r = b.add_plane(n);
    if (r.ok()) {
      id = r.value();
    }
    return r.ok();
  };
  const auto type = [&](std::string_view p, std::string_view n, tech::TileType& id) {
    auto r = b.add_type(p, n);
    if (r.ok()) {
      id = r.value();
    }
    return r.ok();
  };
  bool ok = plane("active", mt.active) && plane("metal1", mt.metal1) && plane("metal2", mt.metal2) &&
            plane("metal3", mt.metal3);
  ok = ok && type("active", "ndiff", mt.ndiff) && type("active", "pdiff", mt.pdiff) && type("active", "poly", mt.poly) &&
       type("active", "nfet", mt.nfet) && type("metal1", "metal1", mt.m1) && type("metal2", "metal2", mt.m2) &&
       type("metal3", "metal3", mt.m3) && type("active", "pdc", mt.pdc) && type("metal1", "via1", mt.via1) &&
       type("metal2", "via2", mt.via2);
  const std::vector<std::string_view> pdc{"pdiff", "metal1"};
  const std::vector<std::string_view> v1{"metal1", "metal2"};
  const std::vector<std::string_view> v2{"metal2", "metal3"};
  const std::vector<std::string_view> stack{"via1", "via2"};
  const std::vector<std::string_view> compose{"nfet", "poly", "ndiff"};
  ok = ok && b.add_contact("pdc", pdc).ok() && b.add_contact("via1", v1).ok() && b.add_contact("via2", v2).ok() &&
       b.add_stackable(stack).ok() && b.add_compose_rule("compose", compose).ok();
  ok = ok && b.add_connect("metal1", "via1").ok() && b.add_connect("metal2", "via1,via2").ok() &&
       b.add_connect("metal3", "via2").ok();
  if (!ok) {
    return std::nullopt;
  }
  auto t = b.finalize();
  if (!t.ok()) {
    return std::nullopt;
  }
  mt.tech = t.value();
  return mt;
}

/// The `mt` technology, or SKIP when the tech contract cannot build it.
#define HEFESTO_REQUIRE_MT(var)                                       \
  const auto var##_opt = ::hefesto::db::test::build_mt();             \
  if (!var##_opt) {                                                   \
    SKIP("tech contract cannot build the mt technology yet");         \
  }                                                                   \
  const ::hefesto::db::test::MtTech& var = *var##_opt

}  // namespace hefesto::db::test
