// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The compat branch points of the operator kernels and their pinned float
// paths (capability mask-output-operators; openspec/changes/m5-mask-gds-cif/
// design.md D6 "Compat branch points", D22). Each magic-compat decision is
// one small helper: the ON path is Magic's arithmetic, the OFF path the
// intended one, so both are testable on their own. The float helpers use
// Magic's types and operation order (the core builds with
// -ffp-contract=off), so their results are identical on every platform.
//
// Owner: OPS (design D2).

#pragma once

#include <cstdint>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/geometry.hpp"

namespace hefesto::mask {

// --- Integer division conventions -------------------------------------------

/// C's division: truncation toward zero (Magic's `a / s`).
[[nodiscard]] constexpr std::int64_t div_trunc(std::int64_t a, std::int64_t b) noexcept { return a / b; }
/// Floor division (b > 0).
[[nodiscard]] constexpr std::int64_t div_floor(std::int64_t a, std::int64_t b) noexcept {
  const std::int64_t q = a / b;
  return (a % b != 0 && a < 0) ? q - 1 : q;
}
/// Ceiling division (b > 0).
[[nodiscard]] constexpr std::int64_t div_ceil(std::int64_t a, std::int64_t b) noexcept {
  const std::int64_t q = a / b;
  return (a % b != 0 && a > 0) ? q + 1 : q;
}

/// A mask-unit rectangle divided into internal units: each coordinate
/// truncated toward zero, or rounded outward (floor for the lower edges,
/// ceil for the upper ones) when `outward`.
[[nodiscard]] db::Rect divide_rect(const db::Rect& r, std::int32_t scale, bool outward) noexcept;

// --- Compat branch points (design D6 table) ---------------------------------

/// Q-MASK-1 (grow-grid): ON moves each finite side outward by |c| mod g (a
/// side at the plane's edge stays); OFF rounds each finite side outward to
/// the g-grid.
[[nodiscard]] db::Rect q_mask_1_grow_grid(const db::Rect& r, std::int32_t g, const compat::CompatSettings& c) noexcept;

/// Q-MASK-2 (close): whether an accumulated area `total` (mask units², of a
/// bounded space region) is below the operator's threshold. ON compares with
/// the linearly normalised threshold `scaled`; OFF compares exactly in 64
/// bits, total·L² < declared·D².
struct CloseThreshold {
  std::int64_t scaled = 0;    // values[0]
  std::int64_t declared = 0;  // values[1], else values[0]
  std::int64_t multiply = 1;  // D
  std::int64_t reduce = 1;    // L
};
[[nodiscard]] bool q_mask_2_below(std::int64_t total, const CloseThreshold& t,
                                  const compat::CompatSettings& c) noexcept;

/// Q-MASK-3 (bridge and bridge-lim pass 1, SE outer corner): true when the
/// bottom neighbour tested is the one under the tile's right end (OFF); false
/// for Magic's bottom-left neighbour (ON).
[[nodiscard]] constexpr bool q_mask_3_se_bottom_right(const compat::CompatSettings& c) noexcept {
  return !c.magic_compat;
}

/// Q-MASK-5 (bridge-lim limit searches on layout planes): ON searches with
/// the mask-unit rectangle unchanged; OFF converts it to internal units,
/// rounding outward.
[[nodiscard]] db::Rect q_mask_5_limit_search_area(const db::Rect& mask_area, std::int32_t scale,
                                                  const compat::CompatSettings& c) noexcept;
/// Q-MASK-5: a layout tile edge as bridge-lim uses it (ON raw, OFF scaled).
[[nodiscard]] std::int64_t q_mask_5_tile_edge(std::int64_t edge, std::int32_t scale,
                                              const compat::CompatSettings& c) noexcept;

/// Q-MASK-6 (tagged): the label search rectangle used on mask planes. ON
/// keeps the internal-unit rectangle; OFF multiplies it by the scale factor.
[[nodiscard]] db::Rect q_mask_6_label_area(const db::Rect& label, std::int32_t scale,
                                           const compat::CompatSettings& c) noexcept;

/// Q-MASK-7 (not-square): split tiles are dropped (ON) or copied (OFF).
[[nodiscard]] constexpr bool q_mask_7_keeps_triangles(const compat::CompatSettings& c) noexcept {
  return !c.magic_compat;
}

/// Q-MASK-21 ((non)interacting, (non)overlapping): a mask-unit rectangle
/// converted to internal units for the layout test, truncating (ON) or
/// rounding outward (OFF).
[[nodiscard]] db::Rect q_mask_21_to_internal(const db::Rect& r, std::int32_t scale,
                                             const compat::CompatSettings& c) noexcept;

/// Q-MASK-27 (squares grid-limit step): the new upper edge when the first
/// cut origin `origin` is δ = `delta` > 0 off the grid. ON is sign-blind
/// (`upper - 2δ`); OFF moves the edge the other way for a negative origin.
[[nodiscard]] constexpr std::int32_t q_mask_27_shift(std::int32_t upper, std::int32_t origin, std::int32_t delta,
                                                     const compat::CompatSettings& c) noexcept {
  return (c.magic_compat || origin >= 0) ? upper - 2 * delta : upper + 2 * delta;
}

/// Q-MASK-228 (bridge-lim passes 1 and 2): ON enumerates the plane it
/// paints; OFF enumerates a snapshot.
[[nodiscard]] constexpr bool q_mask_228_snapshot(const compat::CompatSettings& c) noexcept { return !c.magic_compat; }

/// Q-MASK-229 (bridge-lim limit erasure): ON erases whole limiting tiles,
/// unscaled; OFF only their part inside the bridge, in mask units.
[[nodiscard]] constexpr bool q_mask_229_clip_erase(const compat::CompatSettings& c) noexcept { return !c.magic_compat; }

/// Q-MASK-230 (bloat-all marks): ON leaves Magic's stale trigger marks; OFF
/// resets every searched plane.
[[nodiscard]] constexpr bool q_mask_230_reset_all(const compat::CompatSettings& c) noexcept { return !c.magic_compat; }

/// Q-MASK-231 (bloat-all seed search): ON seeds the first unmarked tile
/// only; OFF seeds every unmarked tile overlapping the area.
[[nodiscard]] constexpr bool q_mask_231_seed_all(const compat::CompatSettings& c) noexcept { return !c.magic_compat; }

/// Q-MASK-232 (bloat-all mask-layer trigger to internal units): truncation
/// (ON) or outward rounding (OFF).
[[nodiscard]] db::Rect q_mask_232_to_internal(const db::Rect& r, std::int32_t scale,
                                              const compat::CompatSettings& c) noexcept;

// --- Float paths (design D22) ------------------------------------------------

/// Euclidean grow of a split tile (cifGrowEuclideanFunc): the on-grid
/// offsets dx (from ratio h/w) and dy (from w/h) of a diagonal edge grown by
/// `d`, before the grid limit: ceil(d * r / (1 + sqrt(1 + r*r))) in double.
struct EuclideanOffsets {
  std::int32_t dx = 0;
  std::int32_t dy = 0;
};
[[nodiscard]] EuclideanOffsets euclidean_offsets(std::int32_t d, std::int32_t width, std::int32_t height) noexcept;

/// grow-min's corner height (Q-MASK-15, replicate):
/// (int) sqrt((double)(g*g) - 0.25 * (double)((g+w)*(g+w)) + 0.5), with
/// 32-bit int products.
[[nodiscard]] std::int32_t grow_min_height(std::int32_t g, std::int32_t width) noexcept;

/// bridge's minimum-width expansion (GetExpandedAreaGrid) of the corner to
/// corner rectangle `r` (possibly inverted) for width rule `w`, with
/// IEEE single-precision square roots, and the Q-MASK-227 fix (fix in both
/// modes): every sqrtf argument is clamped to 0. `grid_limit` > 1 snaps the
/// result outward.
[[nodiscard]] db::Rect bridge_expand(const db::Rect& r, std::int32_t w, bool space, std::int32_t grid_limit) noexcept;

}  // namespace hefesto::mask
