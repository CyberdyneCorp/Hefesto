// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ExtWriter and format_value (capability extract-file-format "Lexical
// conventions", "Record grammar", "Header records", "Use records", "Record
// order", "Reader constraints honored by the writer"; openspec/changes/
// m7-extraction design.md D8). One emitter per record with Magic's
// fprintf spacing; finish() writes the record classes in "Record order".
// (Magic ref: extract/ExtCell.c:552-675, extract/ExtBasic.c:681-860,
// extract/ExtCouple.c:289-322, extract/ExtHier.c:1111-1180,
// extract/ExtLength.c:303-305, resis/ResPrint.c:87,255)
//
// Owner: EXTNODE (design D3).

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>

#include "hefesto/extract/file/writer.hpp"

namespace hefesto::extract {

namespace {

constexpr std::string_view kCodePairs = "EXTRACT-FILE-001";
constexpr std::string_view kCodeNoHeader = "EXTRACT-FILE-002";

void append_int(std::string& out, std::int64_t value) {
  out += ' ';
  out += std::to_string(value);
}

void append_quoted(std::string& out, std::string_view name) {
  out += " \"";
  out += name;
  out += '"';
}

void append_rect(std::string& out, const db::Rect& r) {
  append_int(out, r.x0);
  append_int(out, r.y0);
  append_int(out, r.x1);
  append_int(out, r.y1);
}

void append_pairs(std::string& out, std::span<const std::int64_t> values) {
  for (const std::int64_t v : values) {
    append_int(out, v);
  }
}

std::size_t section_index(ExtWriter::Section s) {
  return static_cast<std::size_t>(s);
}

std::size_t cap_index(CapClass cls) {
  return static_cast<std::size_t>(cls);
}

ExtWriter::Section cap_section(std::size_t cls) {
  switch (cls) {
    case 1:
      return ExtWriter::Section::subcell_cap;
    case 2:
      return ExtWriter::Section::array_cap;
    default:
      return ExtWriter::Section::flat_cap;
  }
}

ExtWriter::Section merge_section(MergeClass cls) {
  return cls == MergeClass::array ? ExtWriter::Section::array_merge : ExtWriter::Section::subcell_merge;
}

std::string cap_line(const std::string& a, const std::string& b, double value) {
  std::string line = "cap";
  append_quoted(line, a);
  append_quoted(line, b);
  line += ' ';
  line += format_value(value);
  return line;
}

}  // namespace

std::string format_value(double value) {
  // `%g`: 6 significant digits in the C locale; std::to_chars with
  // chars_format::general and precision 6 is specified as printf("%.6g")
  // in the "C" locale.
  if (std::isnan(value)) {
    return std::signbit(value) ? "-nan" : "nan";
  }
  if (std::isinf(value)) {
    return value < 0 ? "-inf" : "inf";
  }
  std::array<char, 64> buffer{};
  const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                    std::chars_format::general, 6);
  if (result.ec != std::errc{}) {
    return {};
  }
  return std::string{buffer.data(), result.ptr};
}

void ExtWriter::add(Section section, std::string line) {
  line += '\n';
  sections_[section_index(section)].push_back(std::move(line));
}

void ExtWriter::header(const ExtHeader& header) {
  has_header_ = true;
  resist_classes_ = header.resist_classes.size();
  add(Section::header, "timestamp " + std::to_string(header.timestamp));
  add(Section::header, "version " + header.version);
  add(Section::header, "tech " + header.tech);
  add(Section::header, "style " + header.style);
  const double lambda = header.units_per_lambda.value_or(static_cast<double>(header.lscale));
  add(Section::header, "scale " + std::to_string(header.rscale) + ' ' + std::to_string(header.cscale) + ' ' +
                           format_value(lambda));
  std::string classes = "resistclasses";
  append_pairs(classes, header.resist_classes);
  add(Section::header, std::move(classes));
  if (header.cell_parameter) {
    add(Section::header, "parameters :" + header.cell_name + ' ' + *header.cell_parameter);
  }
}

void ExtWriter::node(std::string_view name, std::int64_t resistance, double cap, db::Point at,
                     std::string_view type, std::span<const std::int64_t> area_perimeter) {
  std::string line = "node";
  append_quoted(line, name);
  append_int(line, resistance);
  line += ' ';
  line += format_value(cap);
  append_int(line, at.x);
  append_int(line, at.y);
  line += ' ';
  line += type;
  append_pairs(line, area_perimeter);
  pair_checks_.push_back({area_perimeter.size(), line});
  add(Section::node, std::move(line));
}

void ExtWriter::attr(std::string_view node, db::Rect at, std::string_view type, std::string_view text) {
  std::string line = "attr ";
  line += node;
  append_rect(line, at);
  line += ' ';
  line += type;
  append_quoted(line, text);
  add(Section::node, std::move(line));
}

void ExtWriter::equiv(std::string_view a, std::string_view b) {
  std::string line = "equiv";
  append_quoted(line, a);
  append_quoted(line, b);
  add(Section::node, std::move(line));
}

void ExtWriter::port(std::string_view name, std::int32_t index, db::Rect at, std::string_view type) {
  std::string line = "port";
  append_quoted(line, name);
  append_int(line, index);
  append_rect(line, at);
  line += ' ';
  line += type;
  add(Section::port, std::move(line));
}

void ExtWriter::substrate(std::string_view name, std::int64_t resistance, double /*cap*/) {
  std::string line = "substrate";
  append_quoted(line, name);
  append_int(line, resistance);
  line += " 0";
  add(Section::node, std::move(line));
}

void ExtWriter::substrate(std::string_view name, std::int64_t resistance, db::Point at, std::string_view type,
                          std::span<const std::int64_t> area_perimeter) {
  std::string line = "substrate";
  append_quoted(line, name);
  append_int(line, resistance);
  line += " 0";
  append_int(line, at.x);
  append_int(line, at.y);
  line += ' ';
  line += type;
  append_pairs(line, area_perimeter);
  pair_checks_.push_back({area_perimeter.size(), line});
  add(Section::node, std::move(line));
}

void ExtWriter::device(std::string_view device_class, std::string_view model, db::Rect at,
                       std::span<const std::string> params, std::string_view substrate,
                       std::span<const ExtTerminal> terminals) {
  std::string line = device_class == "fet" ? "fet " : "device ";
  if (device_class != "fet") {
    line += device_class;
    line += ' ';
  }
  line += model;
  append_rect(line, at);
  for (const std::string& p : params) {
    line += ' ';
    line += p;
  }
  if (!substrate.empty()) {
    append_quoted(line, substrate);
  }
  for (const ExtTerminal& t : terminals) {
    append_quoted(line, t.node);
    append_int(line, t.length);
    line += ' ';
    line += t.attributes;
  }
  add(Section::device, std::move(line));
}

void ExtWriter::device_line(std::string_view text) {
  add(Section::device, std::string{text});
}

void ExtWriter::cell_parameters(std::string_view cell, std::string_view text) {
  add(Section::cell_parameters, "parameters :" + std::string{cell} + ' ' + std::string{text});
}

void ExtWriter::abstract() {
  add(Section::abstract, "abstract");
}

void ExtWriter::parameters(std::string_view model, std::span<const std::string> params) {
  std::string line = "parameters ";
  line += model;
  for (const std::string& p : params) {
    line += ' ';
    line += p;
  }
  add(Section::parameters, std::move(line));
}

void ExtWriter::cap(std::string_view a, std::string_view b, double value) {
  cap(a, b, value, CapClass::flat);
}

void ExtWriter::cap(std::string_view a, std::string_view b, double value, CapClass cls) {
  // Q-EXTRACT-7: the smaller name first; the records are sorted in finish().
  std::string first{a};
  std::string second{b};
  if (second < first) {
    std::swap(first, second);
  }
  caps_[cap_index(cls)].push_back({std::move(first), std::move(second), value});
}

void ExtWriter::resist(std::string_view a, std::string_view b, double value) {
  std::string line = "resist";
  append_quoted(line, a);
  append_quoted(line, b);
  line += ' ';
  line += format_value(value);
  add(Section::resist, std::move(line));
}

void ExtWriter::use(std::string_view def, std::string_view id, std::span<const std::int32_t> transform) {
  std::string line = "use ";
  line += def;
  line += ' ';
  line += id;
  for (const std::int32_t c : transform) {
    append_int(line, c);
  }
  add(Section::use, std::move(line));
}

void ExtWriter::merge(std::string_view a, std::string_view b, double cap,
                      std::span<const std::int64_t> area_perimeter) {
  merge(a, b, cap, area_perimeter, MergeClass::subcell);
}

void ExtWriter::merge(std::string_view a, std::string_view b, double cap,
                      std::span<const std::int64_t> area_perimeter, MergeClass cls) {
  std::string line = "merge";
  append_quoted(line, a);
  append_quoted(line, b);
  line += ' ';
  line += format_value(cap);
  append_pairs(line, area_perimeter);
  pair_checks_.push_back({area_perimeter.size(), line});
  add(merge_section(cls), std::move(line));
}

void ExtWriter::merge_more(std::string_view a, std::string_view b, MergeClass cls) {
  std::string line = "merge";
  append_quoted(line, a);
  append_quoted(line, b);
  add(merge_section(cls), std::move(line));
}

void ExtWriter::killnode(std::string_view name) {
  std::string line = "killnode";
  append_quoted(line, name);
  add(Section::resist, std::move(line));
}

void ExtWriter::distance(std::string_view driver, std::string_view receiver, std::int64_t min, std::int64_t max) {
  std::string line = "distance ";
  line += driver;
  line += ' ';
  line += receiver;
  append_int(line, min);
  append_int(line, max);
  add(Section::distance, std::move(line));
}

diag::Outcome<std::string> ExtWriter::finish() {
  ExtWriter done = std::move(*this);
  *this = ExtWriter{};
  if (!done.has_header_) {
    return diag::make(kCodeNoHeader, "The .ext file has no header records.");
  }
  for (const PairCheck& check : done.pair_checks_) {
    if (check.pairs != 2 * done.resist_classes_) {
      return diag::make(kCodePairs, "Record `" + check.record + "` has " + std::to_string(check.pairs) +
                                        " area and perimeter values; the header declares " +
                                        std::to_string(done.resist_classes_) + " resistance classes.");
    }
  }
  for (std::size_t cls = 0; cls < done.caps_.size(); ++cls) {
    auto& caps = done.caps_[cls];
    std::stable_sort(caps.begin(), caps.end(), [](const CapRecord& x, const CapRecord& y) {
      return std::tie(x.a, x.b) < std::tie(y.a, y.b);
    });
    for (const CapRecord& c : caps) {
      done.add(cap_section(cls), cap_line(c.a, c.b, c.value));
    }
  }
  std::string text;
  for (const auto& section : done.sections_) {
    for (const std::string& line : section) {
      text += line;
    }
  }
  return text;
}

}  // namespace hefesto::extract
