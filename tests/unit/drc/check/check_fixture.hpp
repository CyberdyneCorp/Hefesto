// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared fixture of the basic check tests (drc-rule-semantics): a small
// technology (planes well, active, metal1, metal2), a flat cell to paint
// in, and rule builders that produce the primitive rules Magic's DRCtech.c
// compiles for each statement (drcWidth, drcMaskSpacing, drcMaxwidth,
// drcArea, drcOffGrid, drcRectangle, drcAngles), so the tests use
// hand-built RuleSets without the rule compiler (design D2).
//
// Owner: CHECK (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/drc/check/basic_check.hpp"
#include "hefesto/hints/hints.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::drc::test {

using db::Rect;
using tech::PlaneId;
using tech::TileType;
using tech::TypeMask;

struct CheckTech {
  std::shared_ptr<const tech::Technology> tech;
  PlaneId well = 0;
  PlaneId active = 0;
  PlaneId metal1 = 0;
  PlaneId metal2 = 0;
  TileType nwell = 0;
  TileType pdiff = 0;
  TileType poly = 0;
  TileType pmc = 0;
  TileType m1 = 0;
  TileType m2 = 0;
  TileType via = 0;
};

inline const CheckTech& check_tech() {
  static const CheckTech instance = [] {
    CheckTech t;
    tech::TechnologyBuilder b;
    b.set_name("drccheck");
    REQUIRE(b.add_plane("well").ok());
    REQUIRE(b.add_plane("active").ok());
    REQUIRE(b.add_plane("metal1").ok());
    REQUIRE(b.add_plane("metal2").ok());
    REQUIRE(b.add_type("well", "nwell").ok());
    REQUIRE(b.add_type("active", "pdiff").ok());
    REQUIRE(b.add_type("active", "poly").ok());
    REQUIRE(b.add_type("active", "pmc").ok());
    REQUIRE(b.add_type("metal1", "m1").ok());
    REQUIRE(b.add_type("metal2", "m2").ok());
    REQUIRE(b.add_type("metal1", "via").ok());
    const std::vector<std::string_view> via{"m1", "m2"};
    REQUIRE(b.add_contact("via", via).ok());
    auto finalized = b.finalize();
    REQUIRE(finalized.ok());
    t.tech = finalized.value();
    t.well = t.tech->find_plane("well").id.value();
    t.active = t.tech->find_plane("active").id.value();
    t.metal1 = t.tech->find_plane("metal1").id.value();
    t.metal2 = t.tech->find_plane("metal2").id.value();
    t.nwell = t.tech->find_type("nwell").id.value();
    t.pdiff = t.tech->find_type("pdiff").id.value();
    t.poly = t.tech->find_type("poly").id.value();
    t.pmc = t.tech->find_type("pmc").id.value();
    t.m1 = t.tech->find_type("m1").id.value();
    t.m2 = t.tech->find_type("m2").id.value();
    t.via = t.tech->find_type("via").id.value();
    return t;
  }();
  return instance;
}

inline TypeMask mask_of(std::initializer_list<TileType> types) {
  TypeMask m;
  for (const TileType t : types) {
    m.set(t);
  }
  return m;
}

/// The types visible on `plane`, space included.
inline TypeMask on_plane(PlaneId plane) {
  TypeMask m = check_tech().tech->plane_types(plane);
  m.set(tech::kSpace);
  return m;
}

template <typename F>
void for_each_type(const TypeMask& mask, F&& f) {
  for (std::size_t t = 0; t < tech::kMaxTileTypes; ++t) {
    if (mask.test(static_cast<TileType>(t))) {
      f(static_cast<TileType>(t));
    }
  }
}

inline Rule make_rule(std::int32_t dist, std::int32_t cdist, const TypeMask& ok, const TypeMask& corner,
                      RuleFlags flags, PlaneId check_plane, PlaneId edge_plane, WhyIndex why) {
  Rule r;
  r.dist = dist;
  r.cdist = cdist;
  r.ok = ok;
  r.corner = corner;
  r.flags = flags;
  r.check_plane = check_plane;
  r.edge_plane = edge_plane;
  r.why = why;
  return r;
}

/// Adds `rule` for every pair (a in `from`, b in `to`), a != b.
inline void add_pairs(RuleSetBuilder& b, const TypeMask& from, const TypeMask& to, const Rule& rule) {
  for_each_type(from, [&](TileType a) {
    for_each_type(to, [&](TileType c) {
      if (a != c) {
        b.add(a, c, rule);
      }
    });
  });
}

/// drcWidth: `width set d`.
inline void add_width(RuleSetBuilder& b, const TypeMask& set, PlaneId plane, std::int32_t d, WhyIndex why,
                      std::uint8_t exception = kNoException) {
  Rule r = make_rule(d, d, set, set, flags::kForward, plane, plane, why);
  r.exception = exception;
  add_pairs(b, on_plane(plane) & ~set, set, r);
}

/// drcMaskSpacing with set1 == set2 on one plane (touching_ok and
/// touching_illegal compile the same way then): `spacing set set d ...`.
inline void add_spacing(RuleSetBuilder& b, const TypeMask& set, PlaneId plane, std::int32_t d, WhyIndex why,
                        RuleFlags extra = 0, std::uint8_t exception = kNoException) {
  const TypeMask others = on_plane(plane) & ~set;
  Rule r = make_rule(d, d, others, others, flags::kForward | extra, plane, plane, why);
  r.exception = exception;
  add_pairs(b, set, others, r);
}

/// drcMaskSpacing, widerule: `widespacing set w [runlength] set d touching_ok`.
inline void add_widespacing(RuleSetBuilder& b, const TypeMask& set, PlaneId plane, std::int32_t w,
                            std::int32_t runlength, std::int32_t d, WhyIndex why) {
  const TypeMask others = on_plane(plane) & ~set;
  const std::int32_t c = runlength > 0 ? runlength : d;
  const RuleFlags rl = runlength > 0 ? flags::kRunLength : 0;
  const RuleFlags maxw = flags::kMaxWidth | flags::kTrigger | flags::kBends;
  for_each_type(set, [&](TileType i) {
    for_each_type(others, [&](TileType j) {
      b.add(i, j, make_rule(w, c, set, set, flags::kReverse | maxw, plane, plane, why));
      b.add(i, j, make_rule(d, c, others, others, rl | flags::kForward | flags::kBothCorners, plane, plane, why));
      b.add(j, i, make_rule(w, c, set, set, flags::kForward | maxw, plane, plane, why));
      b.add(j, i, make_rule(d, c, others, others, rl | flags::kReverse | flags::kBothCorners, plane, plane, why));
    });
  });
}

/// drcMaskSpacing, surround_ok: `spacing set1 set2 d surround_ok` with set1
/// on `plane1` and set2 on another plane `plane2`.
inline void add_surround_spacing(RuleSetBuilder& b, const TypeMask& set1, PlaneId plane1, const TypeMask& set2,
                                 PlaneId plane2, std::int32_t d, WhyIndex why) {
  const TypeMask ok = on_plane(plane2) & ~set2;
  const TypeMask corner = on_plane(plane1) & ~set1;
  for_each_type(set1, [&](TileType i) {
    for_each_type(corner, [&](TileType j) {
      b.add(i, j, make_rule(1, 1, set2, corner, flags::kForward | flags::kTrigger, plane2, plane1, why));
      b.add(i, j, make_rule(d, d, ok, corner, flags::kForward | flags::kBothCorners, plane2, plane1, why));
      b.add(j, i, make_rule(1, 1, set2, corner, flags::kReverse | flags::kTrigger, plane2, plane1, why));
      b.add(j, i, make_rule(d, d, ok, corner, flags::kReverse | flags::kBothCorners, plane2, plane1, why));
    });
  });
}

/// drcMaxwidth: `maxwidth set d [bend_ok|bend_illegal|both] [exclude]`.
inline void add_maxwidth(RuleSetBuilder& b, const TypeMask& set, PlaneId plane, std::int32_t d, RuleFlags bend,
                         WhyIndex why, const TypeMask* exclude = nullptr, PlaneId exclude_plane = 0) {
  const Rule r = exclude == nullptr
                     ? make_rule(d, d, set, set, flags::kMaxWidth | bend, plane, plane, why)
                     : make_rule(d, d, set, *exclude, flags::kMaxWidth | bend, exclude_plane, plane, why);
  add_pairs(b, on_plane(plane) & ~set, set, r);
}

/// drcArea: `area set area horizon`.
inline void add_area(RuleSetBuilder& b, const TypeMask& set, PlaneId plane, std::int32_t area,
                     std::int32_t horizon, WhyIndex why) {
  add_pairs(b, on_plane(plane) & ~set, set,
            make_rule(horizon, area, set, set, flags::kArea | flags::kForward, plane, plane, why));
}

/// drcOffGrid: `off_grid set pitch`.
inline void add_off_grid(RuleSetBuilder& b, const TypeMask& set, PlaneId plane, std::int32_t pitch, WhyIndex why) {
  const TypeMask others = on_plane(plane) & ~set;
  add_pairs(b, others, set, make_rule(pitch, 0, set, set, flags::kOffGrid | flags::kForward, plane, plane, why));
  add_pairs(b, set, others, make_rule(pitch, 0, set, set, flags::kOffGrid | flags::kReverse, plane, plane, why));
}

/// drcRectangle's size rule: `rectangle set max even|odd|any`.
inline void add_rect_size(RuleSetBuilder& b, const TypeMask& set, PlaneId plane, std::int32_t max_size,
                          std::int32_t parity, WhyIndex why) {
  add_pairs(b, on_plane(plane) & ~set, set,
            make_rule(max_size, parity, set, set, flags::kRectSize, plane, plane, why));
}

/// drcAngles: `angles set 45` (the boundary rules carry ANGLES_45|ANGLES_90)
/// or `angles set 90`.
inline void add_angles(RuleSetBuilder& b, const TypeMask& set, PlaneId plane, RuleFlags angles, WhyIndex why) {
  const TypeMask others = on_plane(plane) & ~set;
  add_pairs(b, others, set, make_rule(1, 1, set, set, angles, plane, plane, why));
  add_pairs(b, set, others, make_rule(1, 1, set, set, angles | flags::kReverse, plane, plane, why));
}

/// A flat cell of the check technology.
struct Layout {
  const CheckTech& t = check_tech();
  db::CellDef cell{"check", t.tech->plane_count()};

  void paint(TileType type, const Rect& r) { REQUIRE(db::paint(cell, *t.tech, r, type).ok()); }
  void triangle(TileType type, const Rect& r, db::Diagonal diagonal, bool right_side) {
    REQUIRE(db::paint_triangle(cell, *t.tech, r, diagonal, right_side, type).ok());
  }
  void hint(std::string_view name, std::initializer_list<Rect> rects) {
    db::set_property(cell, hints::property_key(name), db::PropertyValue{hints::HintRegion{rects}});
  }
};

inline constexpr Rect kEverywhere{-10000, -10000, 10000, 10000};

struct Options {
  bool magic_compat = true;
  bool euclidean = false;
  Rect check = kEverywhere;
  Rect clip = kEverywhere;
  const CifCatalog* cif = nullptr;
};

inline bool rect_less(const Rect& a, const Rect& b) {
  if (a.y0 != b.y0) return a.y0 < b.y0;
  if (a.x0 != b.x0) return a.x0 < b.x0;
  if (a.y1 != b.y1) return a.y1 < b.y1;
  return a.x1 < b.x1;
}

/// Runs the check and returns every report.
inline std::vector<CollectingErrorSink::Error> run_reports(const Layout& layout, const RuleSet& rules,
                                                           const Options& options = {}) {
  CheckInput input;
  input.cell = &layout.cell;
  input.technology = layout.t.tech.get();
  input.rules = &rules;
  input.cif = options.cif;
  input.euclidean = options.euclidean;
  input.compat.magic_compat = options.magic_compat;
  CollectingErrorSink sink;
  REQUIRE(basic_check(input, options.check, options.clip, sink) == CheckOutcome::completed);
  REQUIRE(sink.count() == sink.errors().size());
  return sink.errors();
}

/// The distinct error rectangles of a check, sorted.
inline std::vector<Rect> run_check(const Layout& layout, const RuleSet& rules, const Options& options = {}) {
  std::vector<Rect> rects;
  for (const auto& e : run_reports(layout, rules, options)) {
    rects.push_back(e.area);
  }
  std::sort(rects.begin(), rects.end(), rect_less);
  rects.erase(std::unique(rects.begin(), rects.end()), rects.end());
  return rects;
}

inline std::vector<Rect> sorted(std::vector<Rect> rects) {
  std::sort(rects.begin(), rects.end(), rect_less);
  return rects;
}

inline bool contains(const std::vector<Rect>& rects, const Rect& r) {
  return std::find(rects.begin(), rects.end(), r) != rects.end();
}

inline bool any_overlaps(const std::vector<Rect>& rects, const Rect& r) {
  return std::any_of(rects.begin(), rects.end(), [&](const Rect& e) { return db::overlap(e, r); });
}

}  // namespace hefesto::drc::test

namespace Catch {
template <>
struct StringMaker<hefesto::db::Rect> {
  static std::string convert(const hefesto::db::Rect& r) {
    return "[" + std::to_string(r.x0) + "," + std::to_string(r.x1) + "]x[" + std::to_string(r.y0) + "," +
           std::to_string(r.y1) + "]";
  }
};
}  // namespace Catch
