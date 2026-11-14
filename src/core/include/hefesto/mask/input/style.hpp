// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Input styles (capability mask-input-styles;
// openspec/changes/m5-mask-gds-cif/design.md D4, D9): the parsed cifinput
// styles (CIFReadStyle) and the style list (cifReadStyleList).
//
// One InputStyle is the result of one load pass over the `cifinput` section
// (CIFReadTechLine for every line, then CIFReadTechFinal), as Magic re-reads
// the section to load a style. It holds the CIF layer name table (whose
// index is the `cif_layer` the readers paint on), the layer and templayer
// rules with their operators (LayerOp, the cifoutput IR; `cif_layers`
// index the CIF layer table and every distance is in read units), the label
// mapping of each CIF layer, the GDS layer map and the scale (S, M).
//
// The mode-dependent cifinput decisions are resolved as follows:
//   - Q-MASK-302 (style registration) and Q-MASK-300 (the scalefactor unit
//     keyword) when the section is read: read_input_styles takes the compat
//     settings, and refresh_input_styles re-reads the section when
//     magic-compat changes (magic-compat is a session setting and the
//     technology file is read once). InputStyle::resolve() re-derives the
//     scale and the distances from the values as written;
//   - Q-MASK-306 and 307 (grow algorithm, boundary/mask-hints in layer
//     rules), 309, 310, 330 and 334 by the read engine (engine.hpp), from the
//     session's compat settings and output style;
//   - Q-MASK-311 and 326 while the section is read.
//
// Contract changes from the scaffold (design "Area contracts", D4): the
// scaffold's InputLayer and InputResolved are replaced by InputRule,
// CifLayerLabels and ScaleSource below; read_input_styles takes the
// technology (type names, contact planes) like read_output_styles; the
// lambda-aware select_input_style, rescale_input_styles,
// refresh_input_styles and input_scale are added.
//
// Owner: INPUT (design D2; the header was created by the M5 scaffold).

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::mask {

/// At most this many CIF layer names and this many rules per style
/// (MAXCIFRLAYERS).
inline constexpr std::size_t kMaxInputLayers = tech::kMaxTileTypes - 1;

/// The label kind of a CIF layer (crs_labelSticky: LABEL_TYPE_*).
enum class LabelKind : std::uint8_t {
  none,     // no kind given
  text,     // `text` and `sticky` (Q-MASK-301)
  port,     // `port`
  cell_id,  // any word starting with `cell`
};

/// The label type of a CIF layer whose labels are discarded (`ignore`).
inline constexpr std::int32_t kDiscardLabels = -1;

/// The label handling of one CIF layer (crs_labelLayer, crs_labelSticky).
struct CifLayerLabels {
  /// The layout type labels on the layer are attached to (a tech::TileType;
  /// space when no `labels` statement named the layer), or kDiscardLabels.
  std::int32_t type = 0;
  LabelKind kind = LabelKind::none;

  friend bool operator==(const CifLayerLabels&, const CifLayerLabels&) = default;
};

/// One `layer` or `templayer` rule (CIFReadLayer).
struct InputRule {
  bool temporary = false;            // templayer (CIFR_TEMPLAYER)
  tech::TileType type = tech::kSpace;  // layer rule: the layout type painted
  std::size_t cif_layer = 0;         // templayer: the CIF layer its result replaces
  /// The operators; `cif_layers` index the CIF layer table and `distance`
  /// is in read units. An optional layer list on the rule line is the
  /// first operator, an `or`.
  std::vector<LayerOp> ops;
  bool simple = true;                // CIFR_SIMPLE: only the leading `or`

  /// The first operator of `kind`, or nullptr.
  [[nodiscard]] const LayerOp* find(OpKind kind) const noexcept;

  friend bool operator==(const InputRule&, const InputRule&) = default;
};

/// A GDS layer and datatype of the `calma` statement; -1 is the wildcard.
struct GdsKey {
  std::int32_t layer = 0;
  std::int32_t datatype = 0;

  friend auto operator<=>(const GdsKey&, const GdsKey&) = default;
};

/// The `scalefactor` statement as written (the last one of the style), from
/// which resolve() derives S and M per Q-MASK-300.
struct ScaleSource {
  std::int64_t value = 0;     // CIFParseScale's numerator (0 = no statement or a bad value)
  std::int64_t decimal = 1;   // CIFParseScale's denominator (10^k reduced)
  std::int64_t unit = 0;      // 10 for nanometers, 100 for angstroms, 0 for none

  friend bool operator==(const ScaleSource&, const ScaleSource&) = default;
};

/// The `options` flags of a style (crs_flags).
namespace input_option {
inline constexpr std::uint32_t ignore_unknown_layer_labels = 0x01;  // CRF_IGNORE_UNKNOWNLAYER_LABELS
}

/// What one CIFTechInputScale call did.
struct InputScaleStep {
  std::int64_t reduced = 1;  // the common factor divided out (lmult; 0 when the style is unusable)
  /// Magic divides by min(gcd(M, distances), gcd(S, M)), which need not
  /// divide S or every distance; true when it did not (values truncated).
  bool truncated = false;
};

class InputStyle {
 public:
  std::string name;
  std::int64_t scale_factor = 0;  // crs_scaleFactor (S): read units per internal unit
  std::int64_t multiplier = 1;    // crs_multiplier (M): read units per centimicron
  std::int32_t grid_limit = 0;    // gridlimit, in nanometers (Q-MASK-305)
  std::uint32_t options = 0;      // input_option flags
  std::vector<std::string> cif_layers;   // the CIF layer name table
  std::vector<CifLayerLabels> labels;    // parallel to cif_layers
  std::vector<InputRule> rules;          // in declaration order
  /// calma statements: (layer, datatype) -> CIF layer; a later statement
  /// replaces an earlier one for the same pair.
  std::map<GdsKey, std::size_t> calma;
  ScaleSource scale_source;
  /// The tech-load messages of this style's load pass, in order, ending with
  /// Magic's `Input style <name>: scaleFactor=<S>, multiplier=<M>` line
  /// (emitted at tech load for the first style, by `cif istyle` for the
  /// others).
  std::vector<diag::Diagnostic> load_messages;

  [[nodiscard]] bool has_option(std::uint32_t flag) const noexcept { return (options & flag) != 0; }

  /// The index of CIF layer `layer_name` (exact, case-sensitive), if known.
  [[nodiscard]] std::optional<std::size_t> find_cif_layer(std::string_view layer_name) const noexcept;
  /// CIFCalmaLayerToCifLayer: (layer, datatype), then (layer, *), (*,
  /// datatype) and (*, *); nullopt when none maps.
  [[nodiscard]] std::optional<std::size_t> calma_layer(std::int32_t layer, std::int32_t datatype) const noexcept;
  /// The label handling of CIF layer `cif_layer` (a default for an index
  /// out of range).
  [[nodiscard]] CifLayerLabels label_of(std::size_t cif_layer) const noexcept;

  /// Re-derives S, M (Q-MASK-300: with magic-compat ON a unit keyword sets
  /// M, OFF multiplies it) and every operator distance from the values as
  /// written, then normalises them as CIFReadTechFinal does
  /// (rescale(1, 1, true)).
  InputScaleStep resolve(const compat::CompatSettings& compat);

  /// CIFTechInputScale(n, d, reduce_fully): the internal grid became n/d
  /// times coarser (S *= n, M *= d, distances *= d), then the common factor
  /// is divided out ("Scale normalization"); without `reduce_fully` only a
  /// factor of d (the mid-read grid subdivision). Saturates at Magic's int.
  InputScaleStep technology_scale(std::int64_t n, std::int64_t d, bool reduce_fully);

  /// CIFInputRescale(n, d): the read units were subdivided by n (S, M and
  /// every distance times n), then divided by d.
  void read_rescale(std::int64_t n, std::int64_t d);

  /// CIFReadTechLimitScale(ns, ds) ("Grid limit"): true when subdividing the
  /// internal grid by ns/ds would make it finer than, or not a multiple of,
  /// the grid limit.
  [[nodiscard]] bool limit_scale(std::int64_t ns, std::int64_t ds) const noexcept;

  /// CIFGetInputScale(1000) ("Input-scale reporting"): microns per internal
  /// unit, (float)(10 * S) / (float)(M * 1000).
  [[nodiscard]] float microns_per_unit() const noexcept;

  /// CIFReadGetGrowSize: the summed grow-shrink distance of the first layer
  /// rule of `type` whose sum is positive (LEF via-size reading).
  [[nodiscard]] std::int64_t grow_size(tech::TileType type) const noexcept;

  /// The distances as written, in rule then operator order (resolve()).
  [[nodiscard]] const std::vector<std::int32_t>& source_distances() const noexcept { return source_distances_; }
  void set_source_distances(std::vector<std::int32_t> distances) { source_distances_ = std::move(distances); }

 private:
  std::vector<std::int32_t> source_distances_;
};

/// Every cifinput style in list order, and the current one
/// (cifCurReadStyle).
struct InputStyleSet {
  /// The styles in list order: the current one as loaded and rescaled (the
  /// selection, then every grid change and read), the others as read.
  std::vector<std::shared_ptr<const InputStyle>> styles;
  /// The styles as read (CIFReadTechFinal at the unscaled grid), parallel to
  /// `styles`: a selection loads from here, as Magic re-reads the section.
  std::vector<std::shared_ptr<const InputStyle>> base;
  std::optional<std::size_t> current;
  /// The magic-compat setting the section was read with (Q-MASK-300, 302).
  bool magic_compat = true;
  /// A statement came before the first `style` line. The capability ignores
  /// it; Magic's CIFReadTechLine returns FALSE, which fails the tech load as
  /// Q-MASK-17 does for cifoutput (kept for the caller to decide).
  bool section_failed = false;

  /// The current style, or nullptr when none is loaded.
  [[nodiscard]] const InputStyle* current_style() const noexcept;
  [[nodiscard]] std::shared_ptr<const InputStyle> current_shared() const noexcept;
  /// The style names in list order (cif listall istyle).
  [[nodiscard]] std::vector<std::string> names() const;
  /// Replaces the current style (after a read changed its scale).
  void replace_current(std::shared_ptr<const InputStyle> style);
};

/// Reads the last `cifinput` section of `technology`: every style, each
/// loaded by its own pass; the first style is current, as read (the caller
/// applies the post-load lambda with rescale_input_styles). Emits the first
/// style's tech-load messages to `sink`; the others are kept in
/// InputStyle::load_messages. Empty when there is no section or no style.
[[nodiscard]] InputStyleSet read_input_styles(const tech::Technology& technology, const compat::CompatSettings& compat,
                                              diag::DiagnosticSink& sink);
/// The same for one given section.
[[nodiscard]] InputStyleSet read_input_styles(const tech::Technology& technology, const tech::RawSection& section,
                                              const compat::CompatSettings& compat, diag::DiagnosticSink& sink);

/// CIFTechInputScale(n, d, TRUE) on the current style: the internal grid
/// became d/n times finer (DBLambda = n/d after the tech load; `scalegrid`;
/// a finer-grid `.mag` read). Returns the native messages to emit (the
/// truncation warning with magic-compat OFF).
std::vector<diag::Diagnostic> rescale_input_styles(InputStyleSet& set, std::int64_t n, std::int64_t d,
                                                   const compat::CompatSettings& compat);

/// `cif istyle <name>` (CIFSetReadStyle, "Input style selection"): the
/// styles `name` is a prefix of; one match, or the exact name among several
/// (Q-MASK-303, fix in both modes), becomes current: loaded from `base`,
/// its load messages emitted to `sink`, and rescaled by the lambda ratio
/// lambda_n / lambda_d (DBLambda) unless it is already current (a no-op).
/// Fails with MASK-INPUT-004 (ambiguous) or MASK-INPUT-005 (unknown),
/// leaving the current style unchanged.
[[nodiscard]] diag::Status select_input_style(InputStyleSet& set, std::string_view name, std::int64_t lambda_n,
                                              std::int64_t lambda_d, const compat::CompatSettings& compat,
                                              diag::DiagnosticSink& sink);
/// The same at lambda 1/1, discarding the messages.
[[nodiscard]] diag::Status select_input_style(InputStyleSet& set, std::string_view name,
                                              const compat::CompatSettings& compat);

/// magic-compat changed: re-reads the section of `technology` with
/// `compat` (quietly) when it was read with the other setting, keeps the
/// current style by name (else the first) and applies the lambda ratio.
void refresh_input_styles(InputStyleSet& set, const tech::Technology& technology, std::int64_t lambda_n,
                          std::int64_t lambda_d, const compat::CompatSettings& compat);

/// `cif scale in` (CIFGetInputScale(1000)): microns per internal unit of
/// the current style; nullopt ("Error: No style is set") without one.
[[nodiscard]] std::optional<float> input_scale(const InputStyleSet& set) noexcept;

}  // namespace hefesto::mask
