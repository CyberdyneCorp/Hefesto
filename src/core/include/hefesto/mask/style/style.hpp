// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Output styles (capability mask-output-styles;
// openspec/changes/m5-mask-gds-cif/design.md D4, D5): the immutable IR of
// one cifoutput style (CIFStyle): its layers in order, each a list of
// operators with typed operands, the scale (scale factor, expander,
// reducer), the label map, the GDS layer map, the options, the halo radius
// and the contact cut sizes. One parser (read_output_styles) serves DRC
// (drc::CifCatalog is a view over it), the GDS and CIF writers, and later
// LEF, OASIS and density.
//
// A style changes only when the technology reloads, a variant or style is
// selected, or a rescale happens; each change builds a new object and swaps
// the shared pointer (as drc::RuleSet does), so the DRC worker keeps the
// style it started with.
//
// Units: every operand is in the style's mask units after unit
// normalisation ("Unit normalisation"): one internal unit is
// scale.scale_factor mask units, one mask unit is 10 / scale.expander nm.
//
// Owner: STYLES (src/core/{include/hefesto,src}/mask/style/, design D2; the
// header was created by the M5 scaffold).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::mask {

/// The cifoutput operators (Magic's CIFOP_*, cif/CIFint.h:152-178), with the
/// interacting/overlapping variants and the square/grid flavours spelled out.
enum class OpKind : std::uint8_t {
  op_and,           // and
  op_or,            // or
  and_not,          // and-not
  grow,             // grow
  grow_min,         // grow-min
  grow_grid,        // grow-grid
  shrink,           // shrink
  bloat_or,         // bloat-or
  bloat_min,        // bloat-min
  bloat_max,        // bloat-max
  bloat_all,        // bloat-all
  squares,          // squares
  squares_grid,     // squares-grid (also `squares` raised by gridlimit)
  slots,            // slots
  bbox,             // bbox
  boundary,         // boundary
  net,              // net
  maxrect,          // maxrect
  interacting,      // interacting / noninteracting / overlapping / nonoverlapping (flags)
  copyup,           // copyup (cifinput)
  close,            // close
  orthogonal,       // orthogonal (CIFOP_MANHATTAN)
  bridge,           // bridge
  bridge_lim,       // bridge-lim
  mask_hints,       // mask-hints
  not_square,       // not-square
  tagged,           // tagged
};

/// The operator keyword of `kind` as written in the technology file
/// (`overlapping` for OpKind::interacting, `squares-grid` for squares_grid).
[[nodiscard]] std::string_view op_keyword(OpKind kind) noexcept;

/// CIFOP_INT_NOT / CIFOP_INT_TOUCHING for OpKind::interacting.
struct InteractFlags {
  bool inverted = false;  // noninteracting / nonoverlapping
  bool touching = false;  // interacting (touching counts) vs overlapping

  friend bool operator==(const InteractFlags&, const InteractFlags&) = default;
};

/// BloatData: the bloat distance of every tile type (bloat-or, bloat-min,
/// bloat-max) or, for bloat-all, 1 for each selected type or mask layer.
struct BloatTable {
  std::array<std::int32_t, tech::kMaxTileTypes> distance{};
  /// bl_plane: the plane every bloat type lies in; -1 when a bloat-all
  /// operates on mask layers (`distance` is then indexed by mask layer).
  std::int32_t plane = 0;

  friend bool operator==(const BloatTable&, const BloatTable&) = default;
};

/// Indices into LayerOp::values, in Magic's client order.
namespace squares_value {
inline constexpr std::size_t border = 0, size = 1, sep = 2, grid_x = 3, grid_y = 4;
}
namespace slots_value {
inline constexpr std::size_t sborder = 0, ssize = 1, ssep = 2, lborder = 3, lsize = 4, lsep = 5, offset = 6,
                             start = 7;
}
namespace bridge_value {
inline constexpr std::size_t width = 0;
}

/// One operator statement with its typed operands (CIFOp).
struct LayerOp {
  OpKind kind = OpKind::op_or;
  tech::TypeMask types;                // co_paintMask
  std::vector<std::size_t> cif_layers; // co_cifMask: indices of earlier layers, ascending
  /// co_distance: grow, grow-min, grow-grid, shrink, close (0 = close all),
  /// bridge and bridge-lim distance, bloat-all clip halo (0 = none).
  std::int32_t distance = 0;
  /// The client data (SquaresData, SlotsData, BridgeData), indexed by
  /// squares_value (3 values for squares, 5 for squares-grid), slots_value
  /// (8) or bridge_value (1); empty for the other operators.
  std::vector<std::int32_t> values;
  std::optional<BloatTable> bloat;     // bloat-or, bloat-min, bloat-max, bloat-all
  InteractFlags interact;
  /// The co_client flag: maxrect `external`, orthogonal `fill`, bbox `top`.
  bool option = false;
  std::string text;                    // net / tagged / mask-hints name

  friend bool operator==(const LayerOp&, const LayerOp&) = default;
};

/// A GDS layer and datatype (the `calma` statement), stored as atoi of the
/// tokens even out of 0..255 (such a layer is not written to GDS).
struct GdsLayer {
  std::int32_t layer = 0;
  std::int32_t datatype = 0;

  friend bool operator==(const GdsLayer&, const GdsLayer&) = default;
};

/// One output layer (CIFLayer).
struct OutputLayer {
  std::string name;
  bool temporary = false;            // templayer (CIF_TEMP)
  bool label_layer = false;          // labellayer (CIF_LABEL)
  std::vector<LayerOp> ops;
  std::optional<GdsLayer> gds;       // calma statement
  std::int32_t min_width = 0;        // min-width
  tech::TypeMask label_types;        // the types of this layer's `labels` statements
  bool label_port_only = false;      // every `labels` statement of the layer was `port`

  /// True when the layer is written to GDS: a calma layer number in 0..255
  /// and not a temporary layer.
  [[nodiscard]] bool gds_written() const noexcept;

  friend bool operator==(const OutputLayer&, const OutputLayer&) = default;
};

/// The scale of a style (cs_scaleFactor, cs_expander, cs_reducer).
struct StyleScale {
  std::int32_t scale_factor = 1;
  std::int32_t expander = 1;
  std::int32_t reducer = 1;

  friend bool operator==(const StyleScale&, const StyleScale&) = default;
};

/// The `options` and statement flags of a style (cs_flags, CWF_*).
namespace style_option {
inline constexpr std::uint32_t permissive_labels = 0x01;  // calma-permissive-labels
inline constexpr std::uint32_t grow_slivers = 0x02;       // set by min-width
inline constexpr std::uint32_t angstroms = 0x04;          // units angstroms
inline constexpr std::uint32_t grow_euclidean = 0x08;     // grow-euclidean
inline constexpr std::uint32_t see_no_vendor = 0x10;      // see-no-vendor (Q-MASK-401: no effect)
inline constexpr std::uint32_t no_errors = 0x20;          // no-errors
inline constexpr std::uint32_t string_limit = 0x40;       // string-limit
inline constexpr std::uint32_t minimum_grid = 0x80;       // set-minimum-grid
}  // namespace style_option

/// "No label layer" in the per-type label tables.
inline constexpr std::int32_t kNoLayer = -1;

class OutputStyle {
 public:
  OutputStyle();

  std::string name;
  StyleScale scale;
  std::int32_t step_size = 0;        // stepsize (0 = Magic's default)
  std::int32_t grid_limit = 0;       // gridlimit, as written (Q-MASK-8)
  std::vector<OutputLayer> layers;   // in declaration order
  std::uint32_t options = 0;         // style_option flags
  bool see_no_vendor = false;        // Q-MASK-401: parsed and stored, no effect
  /// cs_labelLayer, cs_portText, cs_portLayer: per tile type, the layer of
  /// ordinary label text, of port label text and of port geometry, or
  /// kNoLayer ("labels statement and label layer mapping").
  std::array<std::int32_t, tech::kMaxTileTypes> label_layer{};
  std::array<std::int32_t, tech::kMaxTileTypes> port_text{};
  std::array<std::int32_t, tech::kMaxTileTypes> port_layer{};
  /// cs_radius: the halo radius in internal units, as Magic last computed it
  /// (Q-MASK-18: with magic-compat ON a non-reducing rescale keeps it).
  std::int64_t radius = 1;
  /// The tech-load messages of this style's load pass, in order (Magic
  /// prints them each time the style is loaded: tech load for the first
  /// style, `cif ostyle` for the others).
  std::vector<diag::Diagnostic> load_messages;

  [[nodiscard]] bool has_option(std::uint32_t flag) const noexcept { return (options & flag) != 0; }

  /// The interaction (halo) radius of hierarchical generation, in internal
  /// units ("Interaction radius").
  [[nodiscard]] std::int64_t halo() const noexcept;
  /// floor(2 * max(maxG, maxS) / scaleFactor) + 1 computed from the current
  /// operands (cifComputeHalo).
  [[nodiscard]] std::int64_t compute_halo() const noexcept;
  /// The contact cut size of `type` in mask units ("Contact cut size
  /// query", CIFGetContactSize): size + 2 * border of the first cut
  /// operator; nullopt when the style generates no cut for it (Magic's 0).
  [[nodiscard]] std::optional<std::int64_t> contact_size(tech::TileType type) const noexcept;

  /// The cut operator found by the contact size query, with its values.
  struct ContactCut {
    std::int32_t edge = 0;     // squares size or slots short-side size
    std::int32_t border = 0;   // squares border or slots short-side border
    std::int32_t spacing = 0;  // squares separation or slots short-side separation
    [[nodiscard]] std::int64_t size() const noexcept { return std::int64_t{edge} + 2 * std::int64_t{border}; }
  };
  [[nodiscard]] std::optional<ContactCut> contact_cut(tech::TileType type) const noexcept;

  /// CIFTechOutputScale(n, d): the style after the internal grid became n/d
  /// times finer, as a new object (magic-compat ON: Q-MASK-18 keeps the
  /// stale radius when nothing reduces).
  [[nodiscard]] std::shared_ptr<const OutputStyle> rescale(std::int32_t n, std::int32_t d) const;
  [[nodiscard]] std::shared_ptr<const OutputStyle> rescale(std::int32_t n, std::int32_t d,
                                                           const compat::CompatSettings& compat) const;

  /// The index of the first layer named `layer`; nullopt when none.
  [[nodiscard]] std::optional<std::size_t> find_layer(std::string_view layer) const noexcept;
};

/// What one CIFTechOutputScale call did: the multiplier applied to every
/// operand (d, doubled for half-grid contacts) and the common factor then
/// divided out (1 when nothing reduced). drc::CifCatalog passes them to the
/// DRC rule rescale (drcCifScale(d', 1), drcCifScale(1, reduced)).
/// `messages`: the native warnings of the step, for the caller to emit: the
/// Q-MASK-8 gridlimit warning, and with magic-compat OFF the Q-MASK-29
/// warning when the scale factor was truncated.
struct RescaleStep {
  std::shared_ptr<const OutputStyle> style;
  std::int32_t multiplier = 1;
  std::int32_t reduced = 1;
  std::vector<diag::Diagnostic> messages;
};
[[nodiscard]] RescaleStep rescale_step(const OutputStyle& style, std::int32_t n, std::int32_t d,
                                       const compat::CompatSettings& compat);

/// Every cifoutput style of a technology, in declaration order, and the
/// current one (CIFCurStyle). Copies share the styles.
struct OutputStyleSet {
  /// The styles in list order; the current one as loaded and rescaled
  /// (CIFLoadStyle, then every grid change), the others as read.
  std::vector<std::shared_ptr<const OutputStyle>> styles;
  /// The styles as read (CIFTechFinal at the unscaled grid), parallel to
  /// `styles`: a selection loads from here, as Magic re-reads the section.
  std::vector<std::shared_ptr<const OutputStyle>> base;
  std::optional<std::size_t> current;
  /// A statement before the first `style` line failed the section (Magic's
  /// CIFTechLine returns FALSE; Q-MASK-17).
  bool section_failed = false;

  /// The current style, or nullptr when none is loaded.
  [[nodiscard]] const OutputStyle* current_style() const noexcept;
  /// The current style as a shared pointer (for a worker to keep).
  [[nodiscard]] std::shared_ptr<const OutputStyle> current_shared() const noexcept;
  /// The style names in list order.
  [[nodiscard]] std::vector<std::string> names() const;
};

/// Reads the last `cifoutput` section of `technology` (CIFTechInit restarts
/// the list for a repeated section): every style, each loaded as Magic
/// loads it (CIFTechLine, CIFTechFinal with CIFTechOutputScale(1, 1)); the
/// first style is current. Emits the first style's tech-load messages to
/// `sink` (the others are kept in OutputStyle::load_messages for selection);
/// the Q-MASK-29 rounding warning only with magic-compat OFF in `compat`.
/// Empty when the technology has no cifoutput section or no style.
[[nodiscard]] OutputStyleSet read_output_styles(const tech::Technology& technology, diag::DiagnosticSink& sink,
                                                const compat::CompatSettings& compat = {});
/// The same for one given section.
[[nodiscard]] OutputStyleSet read_output_styles(const tech::Technology& technology, const tech::RawSection& section,
                                                diag::DiagnosticSink& sink, const compat::CompatSettings& compat = {});

/// Only the style a tech load makes current (the first one), read quietly
/// from the last `cifoutput` section; nullopt when there is none. The
/// technology loader reads the first style's grid with it.
[[nodiscard]] std::optional<OutputStyle> read_first_output_style(const tech::Technology& technology);

/// CIFTechOutputScale(n, d) on the current style: the internal grid became
/// n/d times finer (the post-load lambda scaling of tech load, `scalegrid`,
/// a finer-grid read). Returns what the step did ({1, 1} without a style);
/// the caller emits its `messages`.
RescaleStep rescale_output_styles(OutputStyleSet& set, std::int32_t n, std::int32_t d,
                                  const compat::CompatSettings& compat);

/// `cif ostyle <name>` (CIFSetStyle, "Output style selection"): an exact
/// name first, otherwise the unique name that starts with `name`. On success
/// the style becomes current, rescaled by the lambda ratio (lambda_n /
/// lambda_d, DBLambda) unless it was already current, and its load messages
/// and the rescale's messages are emitted to `sink`. Fails (MASK-STYLE-005 ambiguous, MASK-STYLE-006
/// unknown) leaving the current style unchanged.
[[nodiscard]] diag::Status select_output_style(OutputStyleSet& set, std::string_view name,
                                               const compat::CompatSettings& compat);
[[nodiscard]] diag::Status select_output_style(OutputStyleSet& set, std::string_view name, std::int32_t lambda_n,
                                               std::int32_t lambda_d, const compat::CompatSettings& compat,
                                               diag::DiagnosticSink& sink);

}  // namespace hefesto::mask
