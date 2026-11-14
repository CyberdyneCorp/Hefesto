// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DRC on generated mask layers (capability drc-cif-rules;
// openspec/changes/m5-mask-gds-cif/design.md D14): Magic's drcCifCheck.
//
// basic_check calls mask_layer_check after its paint planes. When the DRC
// style's `cifstyle` names a cifoutput style the rules were compiled
// against (StyleDetails::cif_bound) and the style has mask-layer rules, it
//   1. generates the layers the rules read (and the layers those depend on)
//      from the checked cell's own paint over K, as Magic's CIFGen does
//      (mask::generate_area grows K by the style's halo radius itself);
//   2. runs every rule of every layer in style order, the solid-tile list
//      before the space-tile list ("Mask-layer check procedure"): the width
//      and spacing edge checks, the area flood (`cifarea`) and the maxwidth
//      check (`cifmaxwidth`);
//   3. maps each error to layout coordinates (map_mask_error, Q-DRC-13) and
//      reports it clipped to L.
//
// The `cifstyle` bookkeeping when the bound style is not current ("Loading
// DRC CIF style.") is drc::after_basic_check's (drc/rules/style.hpp): the
// check itself only runs with rules already compiled, and generates with
// the bound style whatever the current cifoutput style is.
//
// Scratch planes come from CheckInput::generation (the engine's context)
// when set; otherwise from a context local to the call.
//
// Owner: DRCCIF (openspec/changes/m5-mask-gds-cif/design.md D2).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/drc/check/basic_check.hpp"
#include "hefesto/drc/types.hpp"
#include "hefesto/mask/style/style.hpp"

namespace hefesto::drc {

struct StyleDetails;

/// One compiled mask-layer primitive rule (a DRCCookie of drcCifRules):
/// distances in mask units; `check_layer` is the layer `rule.ok` is tested
/// on (drcc_plane). `rule.ok` and `rule.corner` hold the mask types
/// space (0) and solid (mask::kMaskSolid).
struct MaskRule {
  Rule rule;
  std::size_t check_layer = 0;

  friend bool operator==(const MaskRule&, const MaskRule&) = default;
};

/// The list a rule is evaluated on: the tiles of the layer that are solid
/// (Magic's DRC_CIF_SPACE index: rules of an edge with space on its outer
/// side) or space (DRC_CIF_SOLID).
enum class MaskList : std::uint8_t { solid_tiles = 0, space_tiles = 1 };

/// drcCifRules for one style: per layer of the bound style (indexed like
/// OutputStyle::layers), the two rule lists in check order (the last
/// statement first).
struct MaskRules {
  std::vector<std::array<std::vector<MaskRule>, 2>> layers;
  /// drcCifGenLayers: the layers a rule is attached to or checks.
  std::vector<bool> used;

  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] const std::vector<MaskRule>& list(std::size_t layer, MaskList which) const {
    return layers[layer][static_cast<std::size_t>(which)];
  }

  friend bool operator==(const MaskRules&, const MaskRules&) = default;
};

/// The primitive rules Magic's drcCifWidth, drcCifSpacing, drcCifArea and
/// drcCifMaxwidth compile from the style's mask-layer statements against
/// `style` ("Mask-layer rule statements"). A statement whose layer the
/// style no longer has is skipped.
[[nodiscard]] MaskRules compile_mask_rules(const StyleDetails& details, const mask::OutputStyle& style);

/// drcCifCheck over the check rectangle `check_area` (K) of input.cell,
/// reporting to `sink` clipped to `clip_area` (L). Reports nothing when the
/// style has no compiled mask-layer rule or the bound style cannot be found
/// in input.cif. Returns `cancelled` when input.cancel stopped it.
CheckOutcome mask_layer_check(const CheckInput& input, const db::Rect& check_area, const db::Rect& clip_area,
                              ErrorSink& sink);

/// How an error rectangle was found: by an edge rule (areaCifCheck) or as
/// a whole tile of an area or maxwidth rule (drcCheckCifArea,
/// drcCheckCifMaxwidth).
enum class MaskErrorKind : std::uint8_t { edge, region };

/// "Mapping mask errors to layout coordinates" (Q-DRC-13): `mask_rect`
/// (mask units) in internal units for a style of scale `scale`. With
/// magic-compat ON coordinates are divided truncating toward zero; an edge
/// error of zero extent is widened by one unit, a region error that
/// collapses gives nullopt (dropped). With it OFF lower sides are floored
/// and upper sides ceiled, so nothing collapses.
[[nodiscard]] std::optional<db::Rect> map_mask_error(const db::Rect& mask_rect, std::int32_t scale,
                                                     MaskErrorKind kind, const compat::CompatSettings& compat);

/// Whether a region error collapses under truncating division (the count
/// the DRC-MASK-001 diagnostic reports in both modes).
[[nodiscard]] bool collapses_when_truncated(const db::Rect& mask_rect, std::int32_t scale) noexcept;

}  // namespace hefesto::drc
