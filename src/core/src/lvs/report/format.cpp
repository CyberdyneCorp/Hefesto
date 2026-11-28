// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The text of the comp.out sections (lvs-reports "Two-column layout",
// "Subcircuit summary section", "Net mismatch report", "Device mismatch
// report", "Pin table"). Names are copied byte for byte (Q-LVS-12: never a
// format string); each column is cut at its own width (Q-LVS-16).
//
// Owner: LVSREP.

#include "hefesto/lvs/report/format.hpp"

#include <algorithm>
#include <functional>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/lvs/report/codes.hpp"

namespace hefesto::lvs {

namespace {

constexpr int kMaxWidth = 4096;

/// `text` cut to `width` bytes and padded with spaces to `field` bytes.
void put_field(std::string& out, std::string_view text, std::size_t width, std::size_t field) {
  const std::size_t n = std::min(text.size(), width);
  out.append(text.substr(0, n));
  out.append(field - n, ' ');
}

}  // namespace

diag::Status validate_widths(const ColumnWidths& widths) {
  const auto bad = [](int w) { return w <= 0 || w > kMaxWidth; };
  if (bad(widths.left) || bad(widths.right)) {
    return diag::make(report_codes::kBadWidth, "Format widths must be between 1 and " + std::to_string(kMaxWidth) +
                                                   " (got " + std::to_string(widths.left) + " and " +
                                                   std::to_string(widths.right) + ").");
  }
  return diag::Done{};
}

int left_end(const ColumnWidths& widths) noexcept {
  return widths.left + 2;
}

int right_end(const ColumnWidths& widths) noexcept {
  return left_end(widths) + widths.right + 3;
}

std::string column_row(std::string_view left, std::string_view right, const ColumnWidths& widths) {
  const auto l = static_cast<std::size_t>(left_end(widths));
  const auto r = static_cast<std::size_t>(right_end(widths));
  std::string out;
  out.reserve(r + 1);
  put_field(out, left, l - 1, l);
  out.push_back('|');
  put_field(out, right, r - l - 2, r - l - 1);
  out.push_back('\n');
  return out;
}

std::string divider(const ColumnWidths& widths, bool column) {
  std::string out(static_cast<std::size_t>(right_end(widths)), '-');
  if (column) {
    out[static_cast<std::size_t>(left_end(widths))] = '|';
  }
  out.push_back('\n');
  return out;
}

std::string header_row(std::string_view cell1, std::string_view cell2, const ColumnWidths& widths) {
  return column_row("Circuit 1: " + std::string{cell1}, "Circuit 2: " + std::string{cell2}, widths);
}

std::string blank_row(const ColumnWidths& widths) {
  return column_row("", "", widths);
}

namespace {

std::string count_row(std::string_view what, std::uint64_t n1, std::uint64_t n2, const ColumnWidths& widths) {
  const std::string mark = n1 == n2 ? "" : " **Mismatch**";
  const std::string prefix{what};
  return column_row(prefix + std::to_string(n1) + mark, prefix + std::to_string(n2) + mark, widths);
}

std::uint64_t total(const std::vector<DeviceCount>& devices) {
  std::uint64_t n = 0;
  for (const auto& d : devices) {
    n += d.count;
  }
  return n;
}

}  // namespace

std::string summary_section(std::string_view cell1, std::string_view cell2, const std::vector<SummaryRow>& rows,
                            const PairReport& counts, const ColumnWidths& widths) {
  std::string out = "\nSubcircuit summary:\n";
  out += header_row(cell1, cell2, widths);
  out += divider(widths, true);
  for (const auto& row : rows) {
    out += column_row(row.left, row.right, widths);
  }
  out += count_row("Number of devices: ", total(counts.devices1), total(counts.devices2), widths);
  out += count_row("Number of nets: ", counts.nets1, counts.nets2, widths);
  out += divider(widths, false);
  return out;
}

namespace {

template <typename Fragment>
FragmentRows<Fragment> pair_by_name(const MismatchClass<Fragment>& cls) {
  FragmentRows<Fragment> rows;
  std::vector<bool> used(cls.second.size(), false);
  std::vector<const Fragment*> left_only;
  for (const auto& f : cls.first) {
    std::size_t j = 0;
    while (j < cls.second.size() && (used[j] || cls.second[j].name != f.name)) {
      ++j;
    }
    if (j == cls.second.size()) {
      left_only.push_back(&f);
      continue;
    }
    used[j] = true;
    rows.first.push_back(&f);
    rows.second.push_back(&cls.second[j]);
  }
  std::vector<const Fragment*> right_only;
  for (std::size_t j = 0; j < cls.second.size(); ++j) {
    if (!used[j]) {
      right_only.push_back(&cls.second[j]);
    }
  }
  const std::size_t extra = std::max(left_only.size(), right_only.size());
  for (std::size_t k = 0; k < extra; ++k) {
    rows.first.push_back(k < left_only.size() ? left_only[k] : nullptr);
    rows.second.push_back(k < right_only.size() ? right_only[k] : nullptr);
  }
  return rows;
}

std::string fanout_text(const FanoutEntry& entry) {
  return "  " + entry.device_class + "/" + entry.pin + " = " + std::to_string(entry.count);
}

/// One illegal class: the empty line, each fragment (its title, then its
/// entry rows from `text`) and the closing divider.
template <typename Fragment>
std::string class_block(const MismatchClass<Fragment>& cls, const ColumnWidths& widths, std::string_view absent,
                        const std::function<std::vector<std::string>(const Fragment&)>& text) {
  std::string out = "\n";
  const auto rows = pair_by_name(cls);
  for (std::size_t n = 0; n < rows.first.size(); ++n) {
    out += n == 0 ? divider(widths, false) : blank_row(widths);
    const auto left = rows.first[n] != nullptr ? text(*rows.first[n]) : std::vector<std::string>{std::string{absent}};
    const auto right =
        rows.second[n] != nullptr ? text(*rows.second[n]) : std::vector<std::string>{std::string{absent}};
    const std::size_t lines = std::max(left.size(), right.size());
    for (std::size_t k = 0; k < lines; ++k) {
      out += column_row(k < left.size() ? left[k] : "", k < right.size() ? right[k] : "", widths);
    }
  }
  out += divider(widths, false);
  return out;
}

}  // namespace

FragmentRows<NetFragment> pair_fragments(const MismatchClass<NetFragment>& cls) {
  return pair_by_name(cls);
}

FragmentRows<InstanceFragment> pair_fragments(const MismatchClass<InstanceFragment>& cls) {
  return pair_by_name(cls);
}

std::string net_report(std::string_view cell1, std::string_view cell2,
                       const std::vector<MismatchClass<NetFragment>>& classes, const ColumnWidths& widths) {
  std::string out = "NET mismatches: Class fragments follow (with fanout counts):\n";
  out += header_row(cell1, cell2, widths);
  const std::function<std::vector<std::string>(const NetFragment&)> text = [](const NetFragment& f) {
    std::vector<std::string> lines{"Net: " + f.name};
    for (const auto& entry : f.entries) {
      lines.push_back(fanout_text(entry));
    }
    return lines;
  };
  for (const auto& cls : classes) {
    out += class_block(cls, widths, "(no matching net)", text);
  }
  return out;
}

std::string pin_fanout_text(const PinFanout& pin) {
  if (pin.pins.size() == 1 && pin.fanouts.size() == 1) {
    return "  " + pin.pins.front() + " = " + std::to_string(pin.fanouts.front());
  }
  std::vector<std::uint64_t> fanouts = pin.fanouts;
  std::sort(fanouts.begin(), fanouts.end(), std::greater<>{});
  std::string names;
  for (const auto& name : pin.pins) {
    names += (names.empty() ? "" : ",") + name;
  }
  std::string counts;
  for (const auto f : fanouts) {
    counts += (counts.empty() ? "" : ",") + std::to_string(f);
  }
  return "  (" + names + ") = (" + counts + ")";
}

std::string device_report(std::string_view cell1, std::string_view cell2,
                          const std::vector<MismatchClass<InstanceFragment>>& classes, const ColumnWidths& widths) {
  std::string out = "DEVICE mismatches: Class fragments follow (with node fanout counts):\n";
  out += header_row(cell1, cell2, widths);
  const std::function<std::vector<std::string>(const InstanceFragment&)> text = [](const InstanceFragment& f) {
    std::vector<std::string> lines{"Instance: " + f.name};
    for (const auto& pin : f.pins) {
      lines.push_back(pin_fanout_text(pin));
    }
    return lines;
  };
  for (const auto& cls : classes) {
    out += class_block(cls, widths, "(no matching instance)", text);
  }
  return out;
}

std::string pin_table(std::string_view cell1, std::string_view cell2, const std::vector<PinRow>& rows,
                      const ColumnWidths& widths) {
  std::string out = "\nSubcircuit pins:\n";
  out += header_row(cell1, cell2, widths);
  out += divider(widths, true);
  for (const auto& row : rows) {
    if (row.left == "(no pins)" || row.right == "(no pins)") {
      continue;
    }
    out += column_row(row.left, row.right, widths);
  }
  out += divider(widths, false);
  return out;
}

}  // namespace hefesto::lvs
