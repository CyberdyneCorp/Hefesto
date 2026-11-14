// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc::CifCatalog: the read-only view of the technology's `cifoutput`
// styles that DRC needs before M5 (openspec/changes/m3-drc/design.md D5,
// D13): style names in declaration order and the current style, each
// style's scalefactor, expander and reducer, its layer names (to bind
// `cifstyle` and resolve `cif*` rule layers), and for each contact type the
// `squares` (or `slots`) operator border used by Euclidean derived borders
// (drcGetContactBorder) and by the default step size (6400 / cifScale).
//
// Since M5 the catalog is a view over the mask-output-styles reader
// (mask::read_output_styles, openspec/changes/m5-mask-gds-cif/design.md D5):
// each style is the mask::OutputStyle Magic loads when it becomes the
// current output style (CIFTechLine, CIFTechFinal), and rescale() is
// CIFTechOutputScale on it, so the scalefactor and squares borders are
// Magic's. The catalog reads quietly: the cifoutput section's messages are
// reported by the session's own read (mask-output-styles).
//
// (Magic ref: cif/CIFtech.c CIFTechFinal, CIFGetContactSize,
// CIFTechOutputScale; cif/CIFint.h CIFStyle; drc/DRCtech.c
// drcGetContactBorder)
//
// Owner: RULES (openspec/changes/m3-drc/design.md D2); the re-sourcing onto
// mask::OutputStyle: STYLES (openspec/changes/m5-mask-gds-cif/design.md D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::diag {
class DiagnosticSink;
}

namespace hefesto::tech {
class Technology;
}

namespace hefesto::mask {
class OutputStyle;
}

namespace hefesto::drc {

/// A contact's border beyond its cut, num / den internal units
/// (drcGetContactBorder: num = squares border in CIF units, den = the
/// style's scalefactor).
struct ContactBorder {
  std::int32_t num = 0;
  std::int32_t den = 1;

  friend bool operator==(const ContactBorder&, const ContactBorder&) = default;
};

/// The numeric operands of one cifoutput operator, as CIFTechOutputScale
/// rescales them (CIFOp co_distance and its co_client data).
struct CifOpValues {
  enum class Kind : std::uint8_t {
    plain,         // no client data (and, or, grow, shrink, close, ...)
    squares,       // sq_border, sq_size, sq_sep
    squares_grid,  // sq_border, sq_size, sq_sep, sq_gridx, sq_gridy
    slots,         // sl_sborder, sl_ssize, sl_ssep, sl_lborder, sl_lsize, sl_lsep, sl_offset, sl_start
    bridge,        // br_width
    bloat,         // the distinct non-zero bl_distance values (bloat-or, bloat-min, bloat-max)
    bloat_all,     // bloat-all: its distance-1 flags (scaled, never divided)
  };
  Kind kind = Kind::plain;
  std::int32_t distance = 0;          // co_distance
  std::vector<std::int32_t> values;   // client values in Magic's order (see Kind)
  /// For an `or` operator without CIF layers: its paint types (contact lookup).
  std::optional<tech::TypeMask> or_paint;
  /// `or` or `and-not`: operators that keep the relation between a contact
  /// type and its cuts (CIFGetContactSize).
  bool keeps_cuts = false;

  friend bool operator==(const CifOpValues&, const CifOpValues&) = default;
};

/// One cifoutput style as DRC sees it.
struct CifStyleInfo {
  std::string name;
  std::int32_t scale_factor = 1;  // cs_scaleFactor: CIF units per internal unit
  std::int32_t expander = 1;      // cs_expander
  std::int32_t reducer = 1;       // cs_reducer
  std::vector<std::string> layers;  // layer names in declaration order (layer, templayer, labellayer)
  /// Operators per layer, in order (parallel to `layers`).
  std::vector<std::vector<CifOpValues>> ops;
  /// Border per contact type (indexed by tile type); missing = no border.
  std::vector<std::optional<ContactBorder>> contact_borders;

  friend bool operator==(const CifStyleInfo&, const CifStyleInfo&) = default;
};

class CifCatalog {
 public:
  /// An empty catalog: no cifoutput style loaded (CIFCurStyle == NULL).
  CifCatalog() = default;

  /// Reads the `cifoutput` section of `technology` with
  /// mask::read_output_styles; the first style is current. The read is
  /// quiet (the session reports the section's messages once); `sink` is
  /// unused.
  [[nodiscard]] static CifCatalog from_technology(const tech::Technology& technology, diag::DiagnosticSink& sink);

  [[nodiscard]] std::span<const CifStyleInfo> styles() const noexcept { return styles_; }
  /// The current output style, or nullptr when none is loaded.
  [[nodiscard]] const CifStyleInfo* current() const noexcept;
  /// Makes `name` (exact) the current style (`cif ostyle <name>`); the
  /// caller then rebuilds the DRC rule set (design D4). Fails when unknown.
  [[nodiscard]] diag::Status select(std::string_view name);

  /// drcGetContactBorder in the current style; std::nullopt when `type` is
  /// not a contact with a positive border or no style is loaded.
  [[nodiscard]] std::optional<ContactBorder> contact_border(tech::TileType type) const noexcept;
  /// The mask::OutputStyle the style `name` (exact) views, rescaled with
  /// the catalog; nullptr when unknown. DRC's mask-layer check generates
  /// with it (drcCifStyle).
  [[nodiscard]] std::shared_ptr<const mask::OutputStyle> output_style(std::string_view name) const;
  /// The step size used when the halo is 0: 6400 / scalefactor of the
  /// current style (integer division), or 64 without a style.
  [[nodiscard]] std::int32_t default_step_size() const noexcept;

  /// CIFTechOutputScale(n, d) on every style: the internal grid became
  /// n/d times finer (`scalegrid n d`, a finer-grid read with n = 1).
  /// Returns the factor Magic applies to the drc-cif rule distances of the
  /// current style (drcCifScale(d', 1) then drcCifScale(1, lexpand)) as
  /// {multiplier, divisor}; {1, 1} without a style.
  std::pair<std::int32_t, std::int32_t> rescale(std::int32_t n, std::int32_t d);

  friend bool operator==(const CifCatalog&, const CifCatalog&) = default;

 private:
  /// The mask::OutputStyle each entry of styles_ views (rescale() scales
  /// these). Equality is the view's: two catalogs are equal when their
  /// styles_ are.
  struct Sources {
    std::vector<std::shared_ptr<const mask::OutputStyle>> styles;
    friend bool operator==(const Sources&, const Sources&) noexcept { return true; }
  };

  std::vector<CifStyleInfo> styles_;
  std::optional<std::size_t> current_;
  Sources sources_;
};

}  // namespace hefesto::drc
