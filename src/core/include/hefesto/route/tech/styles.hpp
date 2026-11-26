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

/// Magic's RouteFloat: the search penalty as mantissa and negative
/// exponent (value = mantissa / 2^exponent).
struct Penalty {
  std::int64_t mantissa = 2048;  // DEF_PENALTY_MANTISSA
  int exponent = 1;              // DEF_PENALTY_NEXPONENT
};

/// A maze-router style (Magic's MazeStyle with its MazeParameters, the
/// defaults of mzSetParmDefaults, mzInternal.h DEF_*). Lists are in file
/// order.
struct MazeStyle {
  std::string name;
  std::vector<RouteLayer> layers;
  std::vector<RouteContact> contacts;
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
};

/// Every style of the section, in file order.
struct MazeStyles {
  std::vector<MazeStyle> styles;
};

/// Reads the `mzrouter` section `section` (nullptr: the technology has
/// none) against `tech`, reporting Magic's tech-load messages to `sink`.
[[nodiscard]] MazeStyles read_mzrouter_section(const tech::RawSection* section, const tech::Technology& tech,
                                               diag::DiagnosticSink& sink);

/// MZAfterTech / IRAfterTech on `styles` (see the file comment). `rules`
/// may be null (no DRC style loaded).
void derive_after_load(MazeStyles& styles, const tech::Technology& tech, const drc::RuleSet* rules,
                       const lef::LayerTable& lef_layers, bool magic_compat, diag::DiagnosticSink& sink);

/// The style named `name`, or nullptr.
[[nodiscard]] const MazeStyle* find_style(const MazeStyles& styles, std::string_view name);

}  // namespace hefesto::route
