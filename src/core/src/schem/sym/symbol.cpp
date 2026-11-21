// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The symbol model, pins and pin order (capability
// schematic-symbols-and-libraries "Pins", "Pin order"; Q-SCHEM-5).
//
// Owner: SYMLIB.

#include "hefesto/schem/sym/symbol.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <numeric>
#include <utility>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/schem/file/properties.hpp"

namespace hefesto::schem {

namespace {

bool all_numbered(const std::vector<Pin>& pins) {
  return std::all_of(pins.begin(), pins.end(), [](const Pin& pin) { return pin.sim_pinnumber.has_value(); });
}

std::string symbol_label(const Symbol& symbol) {
  if (!symbol.reference.empty()) return symbol.reference;
  return symbol.file.generic_string();
}

// One SCHEM-SYM-001 per duplicated value, in ascending order of the value.
void report_duplicates(const Symbol& symbol, const std::vector<long long>& values, diag::DiagnosticSink& warnings) {
  std::map<long long, std::size_t> counts;
  for (const long long value : values) ++counts[value];
  for (const auto& [value, count] : counts) {
    if (count < 2) continue;
    warnings.emit(diag::make("SCHEM-SYM-001", "Symbol " + symbol_label(symbol) + " has " + std::to_string(count) +
                                                  " pins with sim_pinnumber " + std::to_string(value) +
                                                  "; they keep their file order"));
  }
}

}  // namespace

Pin make_pin(const BoxRecord& box) {
  Pin pin;
  pin.name = get_token(box.props, "name").value_or("");
  pin.direction = get_token(box.props, "dir").value_or("");
  pin.box = box;
  pin.sim_pinnumber = get_token(box.props, "sim_pinnumber");
  return pin;
}

Symbol make_symbol(std::string reference, platform::Path file, Document document) {
  Symbol symbol;
  symbol.reference = std::move(reference);
  symbol.file = std::move(file);
  symbol.document = std::move(document);
  for (const BoxRecord& box : symbol.document.boxes) {
    if (box.layer == kPinLayer) symbol.pins.push_back(make_pin(box));
  }
  return symbol;
}

long long sim_pinnumber_value(std::string_view text) noexcept {
  std::size_t at = text.find_first_not_of(" \t\n");
  if (at == std::string_view::npos) return 0;
  const bool negative = text[at] == '-';
  if (text[at] == '-' || text[at] == '+') ++at;
  constexpr long long kMax = std::numeric_limits<long long>::max();
  long long value = 0;
  for (; at < text.size() && text[at] >= '0' && text[at] <= '9'; ++at) {
    const int digit = text[at] - '0';
    if (value > (kMax - digit) / 10) return negative ? std::numeric_limits<long long>::min() : kMax;
    value = value * 10 + digit;
  }
  return negative ? -value : value;
}

std::vector<PinIndex> pin_order(const Symbol& symbol, diag::DiagnosticSink* warnings) {
  std::vector<PinIndex> order(symbol.pins.size());
  std::iota(order.begin(), order.end(), PinIndex{0});
  if (symbol.pins.empty() || !all_numbered(symbol.pins)) return order;

  std::vector<long long> values;
  values.reserve(symbol.pins.size());
  for (const Pin& pin : symbol.pins) values.push_back(sim_pinnumber_value(*pin.sim_pinnumber));
  std::stable_sort(order.begin(), order.end(), [&values](PinIndex a, PinIndex b) { return values[a] < values[b]; });
  if (warnings != nullptr) report_duplicates(symbol, values, *warnings);
  return order;
}

std::vector<PinIndex> pin_order(const Symbol& symbol) {
  return pin_order(symbol, nullptr);
}

}  // namespace hefesto::schem
