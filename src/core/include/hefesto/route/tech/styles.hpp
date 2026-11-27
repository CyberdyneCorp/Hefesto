// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `mzrouter` technology section and the maze-router styles (capability
// router-maze "mzrouter section grammar", "Style defaults and lookup",
// "Width and spacing derivation from DRC"; router-maze-iroute "Derived
// irouter style"; openspec/changes/m10-plot-route/design.md D4, D6, D13,
// "Area contracts" MZTECH).
//
// Two stages:
//   - read_mzrouter_section: the styles as written, a pure function of the
//     technology file. tech::Technology keeps them (Technology::maze_styles());
//     the loader calls it once per load on the last `mzrouter` section.
//   - derive_after_load: Magic's MZAfterTech / IRAfterTech on a copy of
//     those styles: widths, lengths and spacings from the loaded DRC rules,
//     the explicit `spacing` statements, the `width` rule (Q-LEGACY-33) and
//     the derived `irouter` style (Q-LEGACY-37, OFF only). It depends on the
//     session's DRC rules and grid, so its result lives in the session
//     (route::IrouteSession::styles), rebuilt after every technology load
//     and every rescale (route::refresh_session_styles).
//
// Owner: MZTECH.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::tech {
class Technology;
struct RawSection;
}  // namespace hefesto::tech

namespace hefesto::drc {
class RuleSet;
}

namespace hefesto::lef {
class LayerTable;
}

namespace hefesto::route {

/// Design rules of a route type (Magic's RouteType): the layer or contact
/// type, width, length and spacings (indexed by tile type; the last entry,
/// `subcell_spacing`, is Magic's SUBCELL column).
struct RouteType {
  tech::TileType type = tech::kSpace;
  bool active = true;
  int width = 0;
  int length = 0;
  std::vector<int> spacing;  // per tile type; -1 = no spacing rule
  int subcell_spacing = 0;
};

/// A route layer (Magic's RouteLayer).
struct RouteLayer {
  RouteType route;
  int h_cost = 1;
  int v_cost = 1;
  int jog_cost = 1;
  int hint_cost = 1;
  int over_cost = 1;
};

/// A route contact (Magic's RouteContact): the indices of the two route
/// layers it connects in MazeStyle::layers.
struct RouteContact {
  RouteType route;
  std::size_t layer1 = 0;
  std::size_t layer2 = 0;
  int cost = 1;
};

/// One entry of a style's route-type list (Magic's mp_rTypes): a route
/// layer or a route contact, by index into MazeStyle::layers or
/// MazeStyle::contacts.
struct RouteTypeRef {
  enum class Kind : std::uint8_t { layer, contact };
  Kind kind = Kind::layer;
  std::size_t index = 0;

  friend bool operator==(const RouteTypeRef&, const RouteTypeRef&) = default;
};

/// A `spacing` statement entry, kept for derive_after_load (Magic's
/// TechSpacing list). `subcell` stands for Magic's SUBCELL column (`type`
/// is then unused); `value` -1 is `NIL`.
struct SpacingStatement {
  RouteTypeRef route;
  tech::TileType type = tech::kSpace;
  bool subcell = false;
  int value = 0;
};

/// A `width` statement, kept for derive_after_load (Q-LEGACY-33: it takes
/// effect with magic-compat OFF only). `length` is the width when the
/// statement gives none.
struct WidthStatement {
  RouteTypeRef route;
  int width = 0;
  int length = 0;
};

/// Magic's RouteFloat: the search penalty as mantissa and negative
/// exponent (value = mantissa / 2^exponent).
struct Penalty {
  std::int64_t mantissa = 2048;  // DEF_PENALTY_MANTISSA
  int exponent = 1;              // DEF_PENALTY_NEXPONENT
};

/// A maze-router style (Magic's MazeStyle with its MazeParameters, the
/// defaults of mzSetParmDefaults, mzInternal.h DEF_*). Lists are in file
/// order. As read_mzrouter_section returns it, every route type has width,
/// length and subcell spacing 0 and an empty spacing table;
/// derive_after_load fills them.
struct MazeStyle {
  std::string name;
  std::vector<RouteLayer> layers;
  std::vector<RouteContact> contacts;
  /// Every route layer and contact in file order (Magic's type order).
  std::vector<RouteTypeRef> types;
  /// The explicit `spacing` entries, in file order.
  std::vector<SpacingStatement> spacings;
  /// The `width` statements, in file order.
  std::vector<WidthStatement> widths;
  Penalty penalty;
  std::int64_t window_width = 10000;
  std::int64_t window_rate = 500;
  std::int64_t bloom_delta_cost = 1;
  int bounds_increment = -1;
  bool estimate = true;
  bool expand_endpoints = true;
  bool top_hints_only = false;
  int max_walk_length = -1;
  int verbosity = 1;
  int bloom_limit = 0;  // 0 = no limit (Magic's default); the deterministic bound (J4)
  bool derived = false;  // the `irouter` style derived from the lef section (D13)

  /// The route type `ref` names (a layer's or a contact's).
  [[nodiscard]] RouteType& route_type(RouteTypeRef ref) noexcept {
    return ref.kind == RouteTypeRef::Kind::layer ? layers[ref.index].route : contacts[ref.index].route;
  }
  [[nodiscard]] const RouteType& route_type(RouteTypeRef ref) const noexcept {
    return ref.kind == RouteTypeRef::Kind::layer ? layers[ref.index].route : contacts[ref.index].route;
  }
};

/// Every style of the section, in file order (a derived `irouter` style,
/// when there is one, last).
struct MazeStyles {
  std::vector<MazeStyle> styles;
};

/// Reads the `mzrouter` section `section` (nullptr: the technology has
/// none) against `tech`, reporting Magic's tech-load messages to `sink`.
/// Every statement is validated in full before any of it takes effect; a
/// statement that reports an error defines nothing (Q-LEGACY-31, -39, -47).
[[nodiscard]] MazeStyles read_mzrouter_section(const tech::RawSection* section, const tech::Technology& tech,
                                               diag::DiagnosticSink& sink);

/// MZAfterTech / IRAfterTech on `styles` (see the file comment). `rules`
/// may be null (no DRC style loaded: every width and spacing is 0).
/// `lef_layers` is the session's LEF layer table, which with magic-compat
/// OFF decides the classes of the `lef` section lines the derived
/// `irouter` style is built from.
void derive_after_load(MazeStyles& styles, const tech::Technology& tech, const drc::RuleSet* rules,
                       const lef::LayerTable& lef_layers, bool magic_compat, diag::DiagnosticSink& sink);

/// The style named `name` (of several with that name, the last in the
/// file, as Magic's MZFindStyle), or nullptr.
[[nodiscard]] const MazeStyle* find_style(const MazeStyles& styles, std::string_view name);

}  // namespace hefesto::route
