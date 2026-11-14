// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internal to the CIF writer (src/core/src/cif/write/): the text of each
// CIF output line (capability cif-format-io "CIF output header", "CIF
// output symbol definition format", "CIF output labels", "CIF output calls
// and arrays"). Ports the fprintf formats of Magic 8.3.694 (fc5daa9e)
// cif/CIFwrite.c cifOutPreamble, cifOutFunc, cifWriteUseFunc,
// cifWriteLabelFunc and cifWritePaintFunc, and the triangle vertex order of
// graphics/grClip.c GrClipTriangle, Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md).
//
// Every product and quotient is computed in 64 bits (C integer division,
// truncating toward zero, as Magic's int division); Magic computes in int
// and overflows silently on extreme coordinates.
// Owner: CIF.

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"

namespace hefesto::cif::detail {

/// The values of the preamble lines (cifOutPreamble).
struct Preamble {
  std::string user = "?";
  std::string machine = "?";
  std::string source = "?";
  std::string tool;
  std::string compiled;
  std::string technology;
  std::string version = "unknown";
  std::optional<std::string> description;  // the line is omitted without one
  std::string style;
  std::string date;
};

/// The ten (or nine) comment lines, each ending in a newline.
[[nodiscard]] std::string preamble_text(const Preamble& p);

/// C's ctime() of `utc` in `zone` (a POSIX TZ string; empty: the host
/// zone) without the trailing newline: "Www Mmm dd hh:mm:ss yyyy".
[[nodiscard]] std::string ctime_text(std::int64_t utc, std::string_view zone);

/// The scale of the numbers of one write.
struct OutputNumbers {
  std::int64_t scale_factor = 1;  // S (labels and calls: internal units to output units)
  std::int64_t reducer = 1;       // R (> 0)
  bool area_labels = false;       // `cif arealabels`
  /// magic-compat ON: label-layer tiles are multiplied by S as well, as
  /// Magic's cifWriteLabelFunc does although they are in output units
  /// (Q-MASK-338); OFF they are only divided by R.
  bool scale_label_layers = true;
};

/// "DS <num> <R> <2E>;"
[[nodiscard]] std::string symbol_start(std::int64_t number, std::int64_t reducer, std::int64_t expander);
/// The `9` line of a cell name, or nullopt for an empty name.
[[nodiscard]] std::optional<std::string> name_line(std::string_view name, std::string_view prefix);
/// "    B <w> <h> <cx> <cy>;" of a rectangle in output-plane units.
[[nodiscard]] std::string box_line(const db::Rect& r, std::int64_t reducer);
/// "    P x1 y1 x2 y2 x3 y3;" of a split tile's solid triangle.
[[nodiscard]] std::string triangle_line(const db::TileView& tile, std::int64_t reducer);
/// A label-layer tile (output-plane units): `94 LAYER cx cy;` or the `95`
/// form with area labels; times S first when `n.scale_label_layers`.
[[nodiscard]] std::string layer_label_line(std::string_view layer, const db::Rect& r, const OutputNumbers& n);
/// A cell label (internal units): `94 TEXT cx cy [LAYER];` or the `95` form.
[[nodiscard]] std::string cell_label_line(std::string_view text, const db::Rect& r,
                                          const std::optional<std::string>& layer, const OutputNumbers& n);
/// The `91` and `C` lines of every element of `use` (child symbol `number`),
/// x outer and y inner; `ids` is `cif idcell`. `visit` receives each
/// element's text (one or two lines) and returns false to stop.
void call_lines(const db::CellUse& use, std::int64_t number, const OutputNumbers& n, bool ids,
                const std::function<bool(const std::string&)>& visit);

/// Q-MASK-322: the label text as written. With magic-compat ON it is the
/// text itself and `unsafe` tells whether it holds whitespace or `;` (the
/// file does not read back); with it OFF each space, tab, newline, carriage
/// return and `;` becomes `_` and `changed` tells whether one did.
struct LabelText {
  std::string text;
  bool unsafe = false;
  bool changed = false;
};
[[nodiscard]] LabelText label_text(std::string_view text, bool magic_compat);

}  // namespace hefesto::cif::detail
