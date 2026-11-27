// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The text of the `comp.out` sections (capability lvs-reports "Two-column
// layout", "Subcircuit summary section", "Net mismatch report", "Device
// mismatch report", "Pin table"). Each function returns whole lines, each
// ending in '\n'. Text is written literally: a '%' in a name is a '%'
// (Q-LVS-12). lvs/hier calls these while a run proceeds.
//
// Owner: LVSREP.

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/lvs/report/model.hpp"

namespace hefesto::lvs {

/// LVS-REPORT-001 unless both widths are > 0 and at most 4096 (Q-LVS-16).
[[nodiscard]] diag::Status validate_widths(const ColumnWidths& widths);

/// L = w1 + 2 and R = L + w2 + 3.
[[nodiscard]] int left_end(const ColumnWidths& widths) noexcept;
[[nodiscard]] int right_end(const ColumnWidths& widths) noexcept;

/// One row: the left text truncated to L−1 bytes and padded to L, `|`, the
/// right text truncated to R−L−2 bytes (its own column, Q-LVS-16) and
/// padded to R−L−1; R bytes and '\n'.
[[nodiscard]] std::string column_row(std::string_view left, std::string_view right, const ColumnWidths& widths);
/// R '-' characters (with `|` at L for a column divider) and '\n'.
[[nodiscard]] std::string divider(const ColumnWidths& widths, bool column);
/// `Circuit 1: <cell1>` | `Circuit 2: <cell2>`.
[[nodiscard]] std::string header_row(std::string_view cell1, std::string_view cell2, const ColumnWidths& widths);
/// Spaces with `|` at L.
[[nodiscard]] std::string blank_row(const ColumnWidths& widths);

/// One row of the subcircuit summary: the texts of the two sides.
struct SummaryRow {
  std::string left;
  std::string right;
};

/// "Subcircuit summary section": the empty line, the title, the header,
/// the rows, the device and net counts and the closing divider.
[[nodiscard]] std::string summary_section(std::string_view cell1, std::string_view cell2,
                                          const std::vector<SummaryRow>& rows, const PairReport& counts,
                                          const ColumnWidths& widths);

/// "Net mismatch report" for `classes` (title, header, classes).
[[nodiscard]] std::string net_report(std::string_view cell1, std::string_view cell2,
                                     const std::vector<MismatchClass<NetFragment>>& classes,
                                     const ColumnWidths& widths);
/// "Device mismatch report".
[[nodiscard]] std::string device_report(std::string_view cell1, std::string_view cell2,
                                        const std::vector<MismatchClass<InstanceFragment>>& classes,
                                        const ColumnWidths& widths);

/// "Pin table": the empty line, `Subcircuit pins:`, the header, the rows
/// (rows whose text is `(no pins)` are dropped) and the closing divider.
[[nodiscard]] std::string pin_table(std::string_view cell1, std::string_view cell2, const std::vector<PinRow>& rows,
                                    const ColumnWidths& widths);

/// A device-fragment row text: `  <pin> = <fanout>` or
/// `  (<p1>,<p2>) = (<f1>,<f2>)`, fanouts in decreasing order.
[[nodiscard]] std::string pin_fanout_text(const PinFanout& pin);

/// The two fragment lists of one class, re-ordered so that equal names sit
/// at the same index where possible; the shorter side is padded with
/// nullptr ("Fragments of the two circuits SHALL be paired").
template <typename Fragment>
struct FragmentRows {
  std::vector<const Fragment*> first;
  std::vector<const Fragment*> second;
};

[[nodiscard]] FragmentRows<NetFragment> pair_fragments(const MismatchClass<NetFragment>& cls);
[[nodiscard]] FragmentRows<InstanceFragment> pair_fragments(const MismatchClass<InstanceFragment>& cls);

}  // namespace hefesto::lvs
