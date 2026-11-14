// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The CIF parser (capability cif-format-io; openspec/changes/m5-mask-gds-cif
// design.md D13; Magic's cif/CIFrdutils.c, CIFrdcl.c, CIFrdpt.c): a
// table-driven dispatcher over the commands DS, DF, DD, C, L, B, R, P, W, E,
// comments and the user extensions 9, 91, 94 and 95. It drives a
// CifReadTarget, which is a mask::ReadSink plus the few operations the CIF
// format needs that the shared sink does not carry (cell renames, the
// mid-read rescale, use ids, label stickiness, the end of the top level).
// The read engine sits behind it in a real read (cif::read); tests use a
// recording target.
//
// The parser works in read units: it applies the symbol scale N/Dn and the
// element scale (CIF's doubled centers and wire paths), and leaves the
// conversion to internal units, the per-cell layer processing and the
// polygon to region conversion to the engine (capability mask-input-styles).
//
// Malformed input is bounded (design D21): integers saturate, every scaled
// value is computed in 64 bits and an element whose coordinates leave the
// usable plane is ignored with DB-GEOM-001, comments nest without
// recursion, and the number of symbols per read is capped (CIF-READ-006).
//
// Owner: CIF (openspec/changes/m5-mask-gds-cif/design.md D2, D13).

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cif/read/messages.hpp"
#include "hefesto/cif/read/read.hpp"
#include "hefesto/cif/read/symbols.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/mask/input/sink.hpp"

namespace hefesto::cif {

/// The label type of a CIF layer whose labels are discarded (`ignore`).
inline constexpr std::int32_t kDiscardLabels = -1;

/// One entry of the CIF layer name table (Magic's cifReadLayers with the
/// current style's crs_labelLayer and crs_labelSticky). Its index is the
/// `cif_layer` passed to the sink.
struct ParseLayer {
  std::string name;
  bool in_style = true;          // the name is a layer of the current style (crs_cifLayers)
  std::int32_t label_type = 0;   // a tech::TileType (0 = space), or kDiscardLabels
  bool sticky = false;           // the label kind is not NONE

  friend bool operator==(const ParseLayer&, const ParseLayer&) = default;
};

/// What the parser needs of the current input style.
struct ParseStyle {
  std::string name;              // for "CIF style NAME: units rescaled ..."
  std::int64_t multiplier = 1;   // M (crs_multiplier) at the start of the read
  std::vector<ParseLayer> layers;
  std::size_t rule_count = 0;    // the number of layer and templayer rules (crs_nLayers; Q-MASK-327 ON)
  bool has_copyup = false;       // some rule has copyup (Q-MASK-309)

  /// The index of CIF layer `layer_name` of the style (CIFReadNameToType).
  [[nodiscard]] std::optional<std::size_t> find(std::string_view layer_name) const;
};

struct ParseOptions {
  bool magic_compat = true;
  std::int64_t rescale_limit = 100;   // `cif limit` (CIFRescaleLimit); <= 0 means 100
  std::size_t max_symbols = SymbolTable::kDefaultMaxCells;
  const std::atomic<bool>* cancel = nullptr;
};

/// A call (CIF C).
struct CifCall {
  std::string child;
  /// The transform; the translation is in read units, which the engine
  /// converts exactly, unless `translation_in_read_units` is false: then
  /// the call's transformation was malformed and magic-compat ON places it
  /// with the translation parsed so far, unscaled, as Magic does (Q-MASK-335).
  db::Transform transform;
  bool translation_in_read_units = true;
  std::string use_id;            // from a pending 91; empty = a default id
  bool child_defined = false;    // the child's DS was read before the call (Q-MASK-309 OFF copyup)

  friend bool operator==(const CifCall&, const CifCall&) = default;
};

/// A label (CIF 94, 95) with its resolved type and stickiness.
struct CifLabel {
  mask::ReadLabel label;         // area in read units; cif_layer is the named or current layer
  std::int32_t type = 0;         // tech::TileType (0 = space); discarded labels are never sent
  bool sticky = false;

  friend bool operator==(const CifLabel&, const CifLabel&) = default;
};

/// The parser's view of the read. Paint, begin_cell and end_cell are the
/// mask::ReadSink operations; paint outside a definition goes to the edit
/// cell (the top level). The parser never calls ReadSink::call, label or
/// property: CIF calls and labels go through place() and cif_label().
class CifReadTarget : public mask::ReadSink {
 public:
  /// True when a cell named `name` exists (in the library or made by this read).
  [[nodiscard]] virtual bool cell_exists(std::string_view name) const = 0;
  /// The name of the edit cell, which receives top-level geometry.
  [[nodiscard]] virtual std::string edit_cell_name() const = 0;
  /// Renames a cell (9, and "n" to "n_k" on a redefinition).
  [[nodiscard]] virtual diag::Status rename_cell(std::string_view from, std::string_view to) = 0;
  /// The read units were subdivided by `factor` (CIFInputRescale(factor,
  /// 1)): the engine multiplies S, M, every operator distance and every
  /// read plane by it.
  [[nodiscard]] virtual diag::Status rescale(std::int64_t factor) = 0;
  /// Places a child in the current cell.
  [[nodiscard]] virtual diag::Status place(const CifCall& call) = 0;
  /// Adds a label to the current cell.
  [[nodiscard]] virtual diag::Status cif_label(const CifLabel& label) = 0;
  /// End of file: processes the top-level geometry into the edit cell.
  [[nodiscard]] virtual diag::Status end_top() = 0;
};

/// Parses CIF `text` into `target`. `session` receives the symbol table
/// (its outside-name test is `target.cell_exists`), the message counts and
/// whether E was read. Messages go to `messages`.
void parse(std::string_view text, const ParseStyle& style, const ParseOptions& options, CifReadTarget& target,
           ReadMessages& messages, ReadSession& session);

}  // namespace hefesto::cif
