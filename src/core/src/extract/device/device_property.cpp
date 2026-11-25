// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `device` cell property: a primitive cell's parameters, or a device
// line that replaces the cell's device output. Capability extract-devices
// "Device cell property" (Q-EXTRACT-16: the dimension tokens are passed
// through in written order, scaled by units-per-lambda and truncated, in
// both modes); openspec/changes/m7-extraction/design.md D10.
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtBasic.c:327-556 and
// utils/lookup.c Lookup, Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: EXTDEV.

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "device_internal.hpp"
#include "hefesto/db/property.hpp"

namespace hefesto::extract {

namespace {

constexpr std::array<std::string_view, 17> kDevTable = {
    "fet",    "mosfet", "asymmetric", "bjt",     "devres",  "devcap",  "devcaprev", "vsource", "diode",
    "pdiode", "ndiode", "subckt",     "rsubckt", "msubckt", "csubckt", "dsubckt",   "veriloga"};

bool same_letter(char a, char b) {
  return a == b || std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
}

// Magic's Lookup: an exact or unique-prefix match, case-insensitive; -1
// when ambiguous, -2 when none. A leading `::magic::` or `magic::` is
// skipped.
int lookup(std::string_view str) {
  if (str.starts_with("::magic::")) {
    str.remove_prefix(9);
  } else if (str.starts_with("magic::")) {
    str.remove_prefix(7);
  }
  int match = -2;
  for (std::size_t pos = 0; pos < kDevTable.size(); ++pos) {
    const std::string_view entry = kDevTable[pos];
    if (str.size() > entry.size() || !std::equal(str.begin(), str.end(), entry.begin(), same_letter)) {
      continue;
    }
    if (str.size() == entry.size()) {
      return static_cast<int>(pos);
    }
    match = match == -2 ? static_cast<int>(pos) : -1;
  }
  return match;
}

std::vector<std::string> tokens(std::string_view text) {
  std::vector<std::string> out;
  std::size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && text[i] == ' ') {
      ++i;
    }
    const std::size_t start = i;
    while (i < text.size() && text[i] != ' ') {
      ++i;
    }
    if (i > start) {
      out.emplace_back(text.substr(start, i - start));
    }
  }
  return out;
}

// sscanf("%d").
std::optional<std::int64_t> scan_int(const std::string& s) {
  long v = 0;
  if (std::sscanf(s.c_str(), "%ld", &v) != 1) {  // NOLINT(cert-err34-c): Magic's sscanf
    return std::nullopt;
  }
  return std::clamp<std::int64_t>(v, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max());
}

struct Parse {
  std::vector<std::string> t;
  std::size_t next = 0;
  float upl = 1.0F;

  std::optional<std::int64_t> scaled(float by) {
    if (next >= t.size()) {
      return std::nullopt;
    }
    const auto v = scan_int(t[next++]);
    if (!v) {
      return std::nullopt;
    }
    return device_detail::mul_trunc(*v, by);
  }
};

// The two dimension tokens of the class (Q-EXTRACT-16: in written order).
// false on a syntax error; `dims` stays empty for a class without them.
bool dimensions(int devidx, const std::string& model, Parse& p, std::vector<std::int64_t>& dims) {
  const auto c = static_cast<DeviceClass>(devidx);
  float first = 0.0F;
  if (c == DeviceClass::fet) {
    first = p.upl * p.upl;  // area, perimeter
  } else if (c == DeviceClass::mosfet || c == DeviceClass::asymmetric || c == DeviceClass::bjt) {
    first = p.upl;  // width, length
  } else if (c == DeviceClass::devres && model != "None") {
    first = p.upl;
  } else if ((c == DeviceClass::devcap || c == DeviceClass::devcaprev) && model != "None") {
    first = p.upl * p.upl;
  } else {
    return true;
  }
  const auto a = p.scaled(first);
  const auto b = p.scaled(p.upl);
  if (!a || !b) {
    return false;
  }
  dims = {*a, *b};
  return true;
}

// The device line, or nullopt on a syntax error.
std::optional<std::string> device_line(int devidx, Parse& p) {
  if (p.next >= p.t.size()) {
    return std::nullopt;
  }
  const std::string model = p.t[p.next++];
  std::array<std::int64_t, 4> box{};
  for (std::size_t i = 0; i < box.size(); ++i) {
    const auto v = p.scaled(p.upl);
    if (!v) {
      return std::nullopt;
    }
    box[i] = *v;
    if (i >= 2 && box[i] <= box[i - 2]) {
      ++box[i];  // a degenerate box is widened
    }
  }
  std::vector<std::int64_t> dims;
  if (!dimensions(devidx, model, p, dims)) {
    return std::nullopt;
  }
  std::string line = devidx == 0 ? "fet" : "device " + std::string(kDevTable[static_cast<std::size_t>(devidx)]);
  line += " " + model;
  for (const std::int64_t v : box) {
    line += " " + std::to_string(v);
  }
  for (const std::int64_t v : dims) {
    line += " " + std::to_string(v);
  }
  for (; p.next < p.t.size(); ++p.next) {
    line += " " + p.t[p.next];  // the rest verbatim
  }
  return line;
}

}  // namespace

DeviceProperty device_property(const db::CellDef& cell, const ExtStyle& style) {
  DeviceProperty out;
  const db::PropertyValue* value = cell.properties().find("device");
  if (value == nullptr) {
    return out;
  }
  out.found = true;
  const std::string text = db::property_text(*value);
  if (text.starts_with("primitive")) {
    out.primitive = true;
    if (text.starts_with("primitive ")) {
      out.parameters_line = "parameters :" + cell.name() + " " + text.substr(10);
    }
    return out;
  }
  Parse p{tokens(text), 0, style.units_per_lambda};
  const int devidx = p.t.empty() ? -2 : lookup(p.t[0]);
  p.next = 1;
  if (devidx < 0) {
    out.messages.push_back(diag::make("EXTRACT-DEV-010", "Extract error:  \"device\" property has unknown device type: " +
                                                             (p.t.empty() ? std::string{} : p.t[0])));
    return out;
  }
  if (const auto line = device_line(devidx, p)) {
    out.device_line = *line;
  } else {
    out.messages.push_back(diag::make("EXTRACT-DEV-010", "Extract error:  \"device " +
                                                             std::string(kDevTable[static_cast<std::size_t>(devidx)]) +
                                                             "\" property syntax error"));
  }
  return out;
}

}  // namespace hefesto::extract
