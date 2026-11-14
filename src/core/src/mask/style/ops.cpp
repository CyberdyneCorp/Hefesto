// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Operator statements ("Operator statement arguments", "squares-to-grid
// conversion by gridlimit"): the keyword table, argument validation with
// Magic's messages, and the typed operands of LayerOp.
//
// Ported from Magic VLSI 8.3.694 cif/CIFtech.c (the operator part of
// CIFTechLine), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Deviations (docs/decisions/cross-cutting.md X-2 tier 1): a 6-token
// `slots` line is rejected as a wrong argument count before any argument
// is read (Q-MASK-9, fix).
//
// Owner: STYLES (openspec/changes/m5-mask-gds-cif/design.md D2).

#include <array>
#include <string>

#include "hefesto/db/geometry.hpp"
#include "mask/style/reader.hpp"
#include "mask/style/text.hpp"

namespace hefesto::mask {

namespace {

struct Keyword {
  std::string_view word;
  OpKind kind;
  InteractFlags interact;
};

constexpr std::array kKeywords = {
    Keyword{"and", OpKind::op_and, {}},
    Keyword{"and-not", OpKind::and_not, {}},
    Keyword{"or", OpKind::op_or, {}},
    Keyword{"grow", OpKind::grow, {}},
    Keyword{"grow-min", OpKind::grow_min, {}},
    Keyword{"grow-grid", OpKind::grow_grid, {}},
    Keyword{"shrink", OpKind::shrink, {}},
    Keyword{"bloat-or", OpKind::bloat_or, {}},
    Keyword{"bloat-max", OpKind::bloat_max, {}},
    Keyword{"bloat-min", OpKind::bloat_min, {}},
    Keyword{"bloat-all", OpKind::bloat_all, {}},
    Keyword{"squares", OpKind::squares, {}},
    Keyword{"squares-grid", OpKind::squares_grid, {}},
    Keyword{"slots", OpKind::slots, {}},
    Keyword{"bbox", OpKind::bbox, {}},
    Keyword{"net", OpKind::net, {}},
    Keyword{"tagged", OpKind::tagged, {}},
    Keyword{"maxrect", OpKind::maxrect, {}},
    Keyword{"boundary", OpKind::boundary, {}},
    Keyword{"mask-hints", OpKind::mask_hints, {}},
    Keyword{"close", OpKind::close, {}},
    Keyword{"orthogonal", OpKind::orthogonal, {}},
    Keyword{"not-square", OpKind::not_square, {}},
    Keyword{"bridge", OpKind::bridge, {}},
    Keyword{"bridge-lim", OpKind::bridge_lim, {}},
    Keyword{"overlapping", OpKind::interacting, {false, false}},
    Keyword{"nonoverlapping", OpKind::interacting, {true, false}},
    Keyword{"interacting", OpKind::interacting, {false, true}},
    Keyword{"noninteracting", OpKind::interacting, {true, true}},
    Keyword{"copyup", OpKind::copyup, {}},
};

}  // namespace

std::string_view op_keyword(OpKind kind) noexcept {
  for (const Keyword& k : kKeywords) {
    if (k.kind == kind) {
      return k.word;
    }
  }
  return {};
}

}  // namespace hefesto::mask

namespace hefesto::mask::style_detail {

namespace {

using style_text::c_atoi;

/// A distance past the usable plane. The operators add distances to plane
/// coordinates in 32 bits, so such a distance is rejected (Magic's int
/// arithmetic overflows on it).
bool beyond_plane(std::int32_t d) noexcept {
  return d > db::kPlaneMax || d < -db::kPlaneMax;
}

std::string beyond_plane_text(const std::string& what) {
  return what + " must be at most " + std::to_string(db::kPlaneMax) + ".";
}

/// The operator parser of one statement.
class OpParser {
 public:
  OpParser(const OpContext& ctx, const Tokens& t) : ctx_(ctx), t_(t) {}

  [[nodiscard]] std::optional<LayerOp> parse(const Keyword& keyword);

 private:
  [[nodiscard]] Messages& messages() const { return *ctx_.layers.messages; }
  std::optional<LayerOp> reject(std::string text) const {
    messages().error(std::move(text));
    return std::nullopt;
  }
  std::optional<LayerOp> wrong_args() const {
    return reject("Wrong number of arguments in " + t_[0] + " statement.");
  }
  [[nodiscard]] std::size_t argc() const noexcept { return t_.size(); }
  [[nodiscard]] LayerSets layers(std::size_t i, bool space_ok) const {
    return parse_layers(ctx_.layers, t_[i], space_ok);
  }
  void set_layers(std::size_t i) {
    LayerSets sets = layers(i, false);
    op_.types = sets.types;
    op_.cif_layers = std::move(sets.layers);
  }

  std::optional<LayerOp> boolean();
  std::optional<LayerOp> grow();
  std::optional<LayerOp> close();
  std::optional<LayerOp> bridge();
  std::optional<LayerOp> bloat_all();
  std::optional<LayerOp> bloat();
  void bloat_pair(BloatTable& table, tech::TypeMask& bloat_types, std::size_t k);
  void bloat_check(BloatTable& table, const tech::TypeMask& bloat_types);
  std::optional<LayerOp> named();
  std::optional<LayerOp> mask_hints();
  std::optional<LayerOp> maxrect();
  std::optional<LayerOp> orthogonal();
  std::optional<LayerOp> bbox();
  std::optional<LayerOp> boundary();
  std::optional<LayerOp> squares();
  [[nodiscard]] bool squares_values(std::array<std::int32_t, 5>& v) const;
  std::optional<LayerOp> slots();
  [[nodiscard]] bool slots_long(std::array<std::int32_t, 8>& v) const;
  /// Reports `text` for a rejected statement inside a helper; false.
  [[nodiscard]] bool fail(std::string text) const {
    messages().error(std::move(text));
    return false;
  }
  std::optional<LayerOp> done() { return std::move(op_); }

  const OpContext& ctx_;
  const Tokens& t_;
  LayerOp op_;
};

std::optional<LayerOp> OpParser::parse(const Keyword& keyword) {
  op_.kind = keyword.kind;
  op_.interact = keyword.interact;
  switch (keyword.kind) {
    case OpKind::op_and:
    case OpKind::and_not:
    case OpKind::op_or:
    case OpKind::interacting: return boolean();
    case OpKind::grow:
    case OpKind::grow_min:
    case OpKind::grow_grid:
    case OpKind::shrink: return grow();
    case OpKind::close: return close();
    case OpKind::bridge:
    case OpKind::bridge_lim: return bridge();
    case OpKind::bloat_all: return bloat_all();
    case OpKind::bloat_or:
    case OpKind::bloat_min:
    case OpKind::bloat_max: return bloat();
    case OpKind::net:
    case OpKind::tagged: return named();
    case OpKind::mask_hints: return mask_hints();
    case OpKind::maxrect: return maxrect();
    case OpKind::orthogonal: return orthogonal();
    case OpKind::bbox: return bbox();
    case OpKind::boundary: return boundary();
    case OpKind::squares:
    case OpKind::squares_grid: return squares();
    case OpKind::slots: return slots();
    default: return done();  // not-square: extra arguments are ignored
  }
}

std::optional<LayerOp> OpParser::boolean() {
  if (argc() != 2) {
    return wrong_args();
  }
  set_layers(1);
  return done();
}

std::optional<LayerOp> OpParser::grow() {
  if (argc() != 2) {
    return wrong_args();
  }
  op_.distance = c_atoi(t_[1]);
  if (op_.distance <= 0) {
    return reject("Grow/shrink distance must be greater than zero.");
  }
  if (beyond_plane(op_.distance)) {
    return reject(beyond_plane_text("Grow/shrink distance"));
  }
  return done();
}

std::optional<LayerOp> OpParser::close() {
  // Without an argument every enclosed area is closed.
  if (argc() == 1) {
    return done();
  }
  if (argc() != 2) {
    return wrong_args();
  }
  return grow();
}

std::optional<LayerOp> OpParser::bridge() {
  if (argc() != (op_.kind == OpKind::bridge ? 3U : 4U)) {
    return wrong_args();
  }
  op_.distance = c_atoi(t_[1]);
  if (op_.distance <= 0) {
    return reject("Bridge distance must be greater than zero.");
  }
  const std::int32_t width = c_atoi(t_[2]);
  if (width <= 0) {
    return reject("Bridge width must be greater than zero.");
  }
  if (beyond_plane(op_.distance) || beyond_plane(width)) {
    return reject(beyond_plane_text("Bridge distance and width"));
  }
  if (op_.kind == OpKind::bridge_lim) {
    set_layers(3);
  }
  op_.values = {width};
  return done();
}

void set_distance(BloatTable& table, const tech::TypeMask& mask, std::int32_t distance) {
  for (std::size_t i = 0; i < tech::kMaxTileTypes; ++i) {
    if (mask.test(static_cast<tech::TileType>(i))) {
      table.distance[i] = distance;
    }
  }
}

std::optional<LayerOp> OpParser::bloat_all() {
  if (argc() != 3 && argc() != 4) {
    return wrong_args();
  }
  set_layers(1);
  BloatTable table;
  const LayerSets bloat = layers(2, true);
  if (!bloat.types.empty() && !bloat.layers.empty()) {
    messages().error("Can't mix CIF and magic layers in bloat statement.");
  }
  if (argc() == 4) {
    // The maximum halo to bloat into (a clip distance).
    op_.distance = c_atoi(t_[3]);
    if (op_.distance <= 0) {
      return reject("Bloat distance must be greater than zero.");
    }
    if (beyond_plane(op_.distance)) {
      return reject(beyond_plane_text("Bloat distance"));
    }
  }
  if (!bloat.types.empty()) {
    set_distance(table, bloat.types, 1);
    bloat_check(table, bloat.types);
  } else {
    for (const std::size_t layer : bloat.layers) {
      table.distance[layer] = 1;
    }
    table.plane = -1;  // the bloat operates on mask layers
  }
  op_.bloat = table;
  return done();
}

void OpParser::bloat_pair(BloatTable& table, tech::TypeMask& bloat_types, std::size_t k) {
  tech::TypeMask mask;
  if (t_[k] == "*") {
    mask = tech::TypeMask::all();
  } else {
    const LayerSets sets = layers(k, true);
    mask = sets.types;
    bloat_types |= mask;
    if (!sets.layers.empty()) {
      messages().error("Can't use templayers in bloat statement.");
    }
  }
  std::int32_t distance = c_atoi(t_[k + 1]);
  if (distance < 0 && op_.kind == OpKind::bloat_or) {
    messages().error("Bloat-or distances must not be negative.");
    distance = 0;
  }
  if (beyond_plane(distance)) {
    messages().error(beyond_plane_text("Bloat distance"));
    distance = distance < 0 ? -db::kPlaneMax : db::kPlaneMax;
  }
  set_distance(table, mask, distance);
}

/// No bloating between tiles of the bloated types, and every type named in
/// one plane.
void OpParser::bloat_check(BloatTable& table, const tech::TypeMask& bloat_types) {
  set_distance(table, op_.types, 0);
  const tech::Technology* technology = ctx_.layers.technology;
  for (std::size_t plane = 0; plane < tech::kMaxPlanes; ++plane) {
    const tech::TypeMask on_plane =
        technology != nullptr ? technology->plane_types(static_cast<tech::PlaneId>(plane)) : tech::TypeMask{};
    if ((bloat_types & on_plane) == bloat_types) {
      table.plane = static_cast<std::int32_t>(plane);
      return;
    }
  }
  messages().error("Not all bloat layers fall in the same plane.");
  table.plane = 0;
}

std::optional<LayerOp> OpParser::bloat() {
  if (argc() < 4) {
    return wrong_args();
  }
  LayerContext types_only = ctx_.layers;
  types_only.layers = nullptr;  // the bloated layers are tile types only
  op_.types = parse_layers(types_only, t_[1], false).types;
  BloatTable table;
  tech::TypeMask bloat_types = op_.types;
  for (std::size_t k = 2; k < argc(); k += 2) {
    if (k + 1 == argc()) {
      return wrong_args();
    }
    bloat_pair(table, bloat_types, k);
  }
  bloat_check(table, bloat_types);
  op_.bloat = table;
  return done();
}

std::optional<LayerOp> OpParser::named() {
  if (argc() != 2 && argc() != 3) {
    return wrong_args();
  }
  op_.text = t_[1];
  if (argc() == 3) {
    set_layers(2);
  }
  return done();
}

std::optional<LayerOp> OpParser::mask_hints() {
  if (argc() != 2) {
    return wrong_args();
  }
  op_.text = t_[1];
  return done();
}

std::optional<LayerOp> OpParser::maxrect() {
  if (argc() == 2) {
    if (style_text::starts_with(t_[1], "ext")) {
      op_.option = true;
    } else if (!style_text::starts_with(t_[1], "int")) {
      messages().error("Maxrect takes only one optional argument \"external\" or \"internal\" (default).");
    }
  } else if (argc() != 1) {
    return wrong_args();
  }
  return done();
}

std::optional<LayerOp> OpParser::orthogonal() {
  if (argc() == 2) {
    if (t_[1] == "fill") {
      op_.option = true;
    } else if (t_[1] != "remove") {
      messages().error("Orthogonal takes only one optional argument \"fill\" or \"remove\" (default).");
    }
  } else if (argc() != 1) {
    return wrong_args();
  }
  return done();
}

std::optional<LayerOp> OpParser::bbox() {
  if (argc() == 2) {
    if (t_[1] == "top") {
      op_.option = true;
    } else {
      messages().error("BBox takes only one optional argument \"top\".");
    }
  } else if (argc() != 1) {
    return wrong_args();
  }
  return done();
}

std::optional<LayerOp> OpParser::boundary() {
  if (argc() != 1) {
    return wrong_args();
  }
  return done();
}

/// border, size, sep, grid x, grid y of the 3- to 5-argument forms.
bool OpParser::squares_values(std::array<std::int32_t, 5>& v) const {
  v = {c_atoi(t_[1]), 0, 0, 1, 1};
  if (v[0] < 0) {
    return fail("Square border must not be negative.");
  }
  v[1] = c_atoi(t_[2]);
  if (v[1] <= 0) {
    return fail("Squares must have positive sizes.");
  }
  v[2] = c_atoi(t_[3]);
  if (v[2] <= 0) {
    return fail("Square separation must be positive.");
  }
  if (argc() >= 5) {
    v[3] = v[4] = c_atoi(t_[4]);
    if (v[3] <= 0) {
      return fail("Square grid must be strictly positive.");
    }
  }
  if (argc() == 6) {
    v[4] = c_atoi(t_[5]);
    if (v[4] <= 0) {
      return fail("Square y-grid must be strictly positive.");
    }
  }
  return true;
}

std::optional<LayerOp> OpParser::squares() {
  std::array<std::int32_t, 5> v{};
  if (argc() == 2) {
    const std::int32_t s = c_atoi(t_[1]);
    if (s <= 0 || (s & 1) != 0) {
      return reject("Squares must have positive even sizes.");
    }
    v = {s / 2, s, s, 1, 1};
  } else if (argc() == 4 || ((argc() == 5 || argc() == 6) && op_.kind == OpKind::squares_grid)) {
    if (!squares_values(v)) {
      return std::nullopt;
    }
  } else {
    return wrong_args();
  }
  // Squares are never placed below the minimum mask resolution: a grid
  // finer than gridlimit makes the operation squares-grid.
  if (v[3] < ctx_.grid_limit) {
    v[3] = ctx_.grid_limit;
    op_.kind = OpKind::squares_grid;
  }
  if (v[4] < ctx_.grid_limit) {
    v[4] = ctx_.grid_limit;
    op_.kind = OpKind::squares_grid;
  }
  const std::size_t count = op_.kind == OpKind::squares_grid ? 5 : 3;
  op_.values.assign(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(count));
  return done();
}

/// The long-side values of a slots statement (arguments 4 to 8).
bool OpParser::slots_long(std::array<std::int32_t, 8>& v) const {
  if (argc() >= 5) {
    v[3] = c_atoi(t_[4]);
    if (v[3] < 0) {
      return fail("Slot border must be non-negative.");
    }
  }
  if (argc() >= 7) {
    v[4] = c_atoi(t_[5]);
    if (v[4] < 0) {
      return fail("Slot long-side size must be positive or zero.");
    }
    v[5] = c_atoi(t_[6]);
    if (v[5] < 0) {
      return fail("Slot long-side separation must be non-negative.");
    }
    if (v[5] == 0 && v[4] > 0) {
      return fail("Slot long-side separation must be strictly positive when long-side size is nonzero");
    }
  }
  if (argc() >= 8) {
    v[6] = c_atoi(t_[7]);
    if (v[6] < 0) {
      return fail("Slot offset must be non-negative.");
    }
  }
  if (argc() == 9) {
    v[7] = c_atoi(t_[8]);
    if (v[7] < 0) {
      return fail("Slot start must be non-negative.");
    }
  }
  return true;
}

std::optional<LayerOp> OpParser::slots() {
  if (argc() < 4 || argc() == 6 || argc() > 9) {
    return wrong_args();  // Q-MASK-9: the 6-token form is rejected first
  }
  std::array<std::int32_t, 8> v{};
  v[0] = c_atoi(t_[1]);
  if (v[0] < 0) {
    return reject("Slot border must be non-negative.");
  }
  v[1] = c_atoi(t_[2]);
  if (v[1] <= 0) {
    return reject("Slot short-side size must be strictly positive.");
  }
  v[2] = c_atoi(t_[3]);
  if (v[2] <= 0) {
    return reject("Slot separation must be strictly positive.");
  }
  if (!slots_long(v)) {
    return std::nullopt;
  }
  op_.values.assign(v.begin(), v.end());
  return done();
}

}  // namespace

std::optional<LayerOp> parse_operator(const OpContext& ctx, const Tokens& t) {
  for (const Keyword& keyword : kKeywords) {
    if (keyword.word == t[0] && keyword.kind != OpKind::copyup) {
      return OpParser(ctx, t).parse(keyword);
    }
  }
  ctx.layers.messages->error("Unknown statement \"" + t[0] + "\".");
  return std::nullopt;
}

}  // namespace hefesto::mask::style_detail
