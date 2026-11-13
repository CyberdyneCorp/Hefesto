// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Output styles (capability mask-output-styles;
// openspec/changes/m5-mask-gds-cif/design.md D4, D5): the immutable IR of
// one cifoutput style (CIFStyle): its layers in order, each a list of
// operators with typed operands, the scale (scale factor, expander,
// reducer), the label map, the GDS layer map, the options, the halo radius
// and the contact cut sizes. One parser (read_output_styles) serves DRC
// (drc::CifCatalog becomes a view over it), the GDS and CIF writers, and
// later LEF, OASIS and density.
//
// A style changes only when the technology reloads, a variant or style is
// selected, or a rescale happens; each change builds a new object and swaps
// the shared pointer (as drc::RuleSet does), so the DRC worker keeps the
// style it started with.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); STYLES fills
// in the implementation in src/core/src/mask/style/ (design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
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
  squares_grid,     // squares-grid
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

/// CIFOP_INT_NOT / CIFOP_INT_TOUCHING for OpKind::interacting.
struct InteractFlags {
  bool inverted = false;  // noninteracting / nonoverlapping
  bool touching = false;  // interacting (touching counts) vs overlapping

  friend bool operator==(const InteractFlags&, const InteractFlags&) = default;
};

/// One operator statement with its typed operands (CIFOp): the layout
/// types and earlier mask layers it reads, and up to four distances in CIF
/// units (co_distance and the squares/slots/bridge parameters, in Magic's
/// order). `text` holds a name operand (mask-hints, tagged, bbox top).
struct LayerOp {
  OpKind kind = OpKind::op_or;
  tech::TypeMask types;                // co_paintMask
  std::vector<std::size_t> cif_layers; // co_cifMask: indices of earlier layers
  std::vector<std::int64_t> values;    // distances / parameters in CIF units
  InteractFlags interact;
  std::string text;

  friend bool operator==(const LayerOp&, const LayerOp&) = default;
};

/// A GDS layer and datatype (the `calma` statement).
struct GdsLayer {
  std::int32_t layer = 0;
  std::int32_t datatype = 0;

  friend bool operator==(const GdsLayer&, const GdsLayer&) = default;
};

/// One output layer (CIFLayer).
struct OutputLayer {
  std::string name;
  bool temporary = false;            // templayer
  std::vector<LayerOp> ops;
  std::optional<GdsLayer> gds;       // calma statement
  std::int64_t min_width = 0;        // min-width
  tech::TypeMask label_types;        // labels statement / labellayer
  bool label_port_only = false;      // labels ... port

  friend bool operator==(const OutputLayer&, const OutputLayer&) = default;
};

/// The scale of a style (cs_scaleFactor, cs_expander, cs_reducer).
struct StyleScale {
  std::int32_t scale_factor = 1;
  std::int32_t expander = 1;
  std::int32_t reducer = 1;

  friend bool operator==(const StyleScale&, const StyleScale&) = default;
};

class OutputStyle {
 public:
  OutputStyle() = default;

  std::string name;
  StyleScale scale;
  std::int32_t step_size = 0;        // stepsize (0 = Magic's default)
  std::int32_t grid_limit = 0;       // gridlimit
  std::vector<OutputLayer> layers;   // in declaration order
  std::uint32_t options = 0;         // the `options` flags (CWF_*)
  bool see_no_vendor = false;        // Q-MASK-401: parsed and stored, no effect

  /// The interaction (halo) radius of hierarchical generation, in internal
  /// units ("Interaction radius").
  [[nodiscard]] std::int64_t halo() const noexcept;
  /// The contact cut size of `type` in internal units ("Contact cut size
  /// query"); nullopt when the style generates no cut for it.
  [[nodiscard]] std::optional<std::int64_t> contact_size(tech::TileType type) const noexcept;
  /// CIFTechOutputScale(n, d): the style after the internal grid became n/d
  /// times finer, as a new object.
  [[nodiscard]] std::shared_ptr<const OutputStyle> rescale(std::int32_t n, std::int32_t d) const;

  /// The index of the layer named `layer`; nullopt when none.
  [[nodiscard]] std::optional<std::size_t> find_layer(std::string_view layer) const noexcept;
};

/// Every cifoutput style of a technology, in declaration order, and the
/// current one (CIFCurStyle). Copies share the styles.
struct OutputStyleSet {
  std::vector<std::shared_ptr<const OutputStyle>> styles;
  std::optional<std::size_t> current;

  /// The current style, or nullptr when none is loaded.
  [[nodiscard]] const OutputStyle* current_style() const noexcept;
  /// The current style as a shared pointer (for a worker to keep).
  [[nodiscard]] std::shared_ptr<const OutputStyle> current_shared() const noexcept;
};

/// Reads the `cifoutput` section; the first style is current. Reports
/// Magic's tech-load messages to `sink`.
[[nodiscard]] OutputStyleSet read_output_styles(const tech::RawSection& section, diag::DiagnosticSink& sink);

/// `cif ostyle <name>`: exact-name and base+variant matching, with a warning
/// (Q-MASK-10, fix). Fails when no style matches.
[[nodiscard]] diag::Status select_output_style(OutputStyleSet& set, std::string_view name,
                                               const compat::CompatSettings& compat);

}  // namespace hefesto::mask
