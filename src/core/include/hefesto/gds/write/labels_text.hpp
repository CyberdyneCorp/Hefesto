// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Label layers and label TEXT records (capability gds-write "Label layers",
// "Label TEXT records", "Structure body order" steps 3 and 4;
// openspec/changes/m5-mask-gds-cif/design.md D12). A port of Magic 8.3.694
// calmaPaintLabelFunc, calmaWriteLabelFunc and the label part of
// calmaOutFunc (calma/CalmaWrite.c:1514-1606, 2829-3048).
//
// Compat decisions:
// - Q-MASK-205 (replicate): port labels sorted stably by port index over
//   the reversed label list (labels_sort.hpp), on every host.
// - Q-MASK-206 (compat): a port label whose type has no port layer is
//   omitted with magic-compat ON, as Magic; OFF writes it with the port-text
//   layer as text and rectangle layer. Both warn (GDS-WRITE-020, native
//   only: Magic prints nothing) once per type and write.
// - Q-MASK-221 (compat): MAG ignores `gds magscale` ON, as Magic; OFF
//   divides it by magscale. The two agree for magscale 1.
// - Q-MASK-122 (replicate): the write side has one size formula, MAG =
//   (size / 800) * S / E in binary64, left to right. The other two formulas
//   of the decision (the default size and the read MAG with +0.5) are read
//   formulas, in gds/read/text.cpp.
// - Q-MASK-208: nothing here changes the layout. Magic's
//   calmaOutStringRecord rewrites mapped label characters in place; the
//   string hook receives a copy.
//
// The emitters write records only; the caller decides when (the `labels`
// setting gates emit_cell_labels, never emit_label_layer) and supplies the
// scales the structure uses.
//
// Owner: GDSW (openspec/changes/m5-mask-gds-cif/design.md D2).

#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/gds/stream/records.hpp"
#include "hefesto/mask/plane/plane.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::gds {

/// Writes one string record (calmaOutStringRecord: character mapping,
/// `gds lower`, string-limit). The default writes the text as it is.
using StringRecordFn = std::function<diag::Status(RecordWriter&, RecordType, std::string_view)>;

/// The types already warned about in this write (Q-MASK-206: once per type
/// and write). The caller keeps one per write.
using PortLabelWarnings = std::bitset<tech::kMaxTileTypes>;

/// What the label emitters need from the write in progress.
struct LabelOutput {
  LabelOutput(RecordWriter& writer, const mask::OutputStyle& output_style) : out(writer), style(output_style) {}

  RecordWriter& out;
  const mask::OutputStyle& style;
  std::int64_t write_scale = 1;  // calmaWriteScale (Wscale): label points and rectangles
  std::int64_t paint_scale = 1;  // calmaPaintScale (Pscale): label-layer tiles
  double magscale = 1.0;         // `gds magscale` (Q-MASK-221 OFF)
  compat::CompatSettings compat;
  StringRecordFn string_record;              // empty: RecordWriter::write_string
  diag::DiagnosticSink* sink = nullptr;      // GDS-WRITE-020 warnings
  PortLabelWarnings* warned = nullptr;       // per-write warning memory
  const tech::Technology* technology = nullptr;  // type names in warnings
};

/// The PRESENTATION word: `((font & 3) << 4) | j` with j from the
/// justification (center 5, N 9, NE 8, E 4, SE 0, S 1, SW 2, W 6, NW 10; an
/// unknown position adds nothing, as Magic's switch).
[[nodiscard]] std::uint16_t text_presentation(std::int32_t font, db::Position justification) noexcept;

/// The MAG value of a font label: `((double)size / 800) * S / E`, then
/// divided by `magscale` with magic-compat OFF (Q-MASK-221; a magscale that
/// is not a positive finite number is ignored).
[[nodiscard]] double text_magnification(std::int32_t size, const mask::StyleScale& scale, double magscale,
                                        const compat::CompatSettings& compat) noexcept;

/// The text and rectangle layers of a label (style layer indices, or
/// mask::kNoLayer).
struct LabelLayers {
  std::int32_t text = mask::kNoLayer;
  std::int32_t rect = mask::kNoLayer;
};

/// Layers of a plain label: the type's label layer for both.
[[nodiscard]] LabelLayers plain_label_layers(const mask::OutputStyle& style, tech::TileType type) noexcept;
/// Layers of a port label (Q-MASK-206): the port-text and port layers;
/// nullopt when it is not written (ON without a port layer; OFF without
/// either layer). OFF falls back to the port-text layer for both.
[[nodiscard]] std::optional<LabelLayers> port_label_layers(const mask::OutputStyle& style, tech::TileType type,
                                                           const compat::CompatSettings& compat) noexcept;

/// calmaWriteLabelFunc: one label's TEXT element (and the BOUNDARY of its
/// rectangle on an operator-less rectangle layer). Writes nothing when the
/// text layer is kNoLayer or its GDS layer is outside 0..255.
[[nodiscard]] diag::Status emit_label(const LabelOutput& output, const db::Label& label, LabelLayers layers);

/// "Structure body order" step 4 (when `labels` is yes): the plain labels
/// in label-list order, then the port labels in port_label_order().
[[nodiscard]] diag::Status emit_cell_labels(const LabelOutput& output, std::span<const db::Label> labels);

/// "Label layers" (calmaPaintLabelFunc over the plane): one TEXT per solid
/// non-split tile of the `labellayer` at `layer_index`, at the tile centre
/// times paint_scale, its STRING the layer name. `clip` is calmaOutFunc's
/// cliprect (nullopt: the whole plane). Writes nothing for a layer whose GDS
/// layer is outside 0..255.
[[nodiscard]] diag::Status emit_label_layer(const LabelOutput& output, std::size_t layer_index,
                                            const mask::MaskPlane& plane,
                                            const std::optional<db::Rect>& clip = std::nullopt);

}  // namespace hefesto::gds
