// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `property` (lvs-setup-file "property default", "property add and type
// declarations", "property remove, tolerance and associate", "property
// topology and derive"). The series and parallel controls are in
// property_merge.cpp. Q-LVS-6: the single-word `add` rule is copied in both
// netgen-compat modes, and a `{key tol}` tolerance that is not a number is
// an error in both.
//
// Owner: LVSSETUP.

#include <array>
#include <cmath>

#include "hefesto/lvs/net/names.hpp"
#include "hefesto/lvs/net/si_value.hpp"
#include "hefesto/lvs/prop/properties.hpp"
#include "runtime.hpp"

namespace hefesto::lvs::setup {

namespace {

constexpr std::array<std::string_view, 5> kTypes{"integer", "double", "value", "string", "expression"};
enum TypeWord : std::size_t { kInteger, kDouble, kValue, kString, kExpression };

constexpr std::string_view kUsage = "property default|parallel|series|topology|<valid_cellname> [option] ...";

/// The option words after the cell (exact words: a property key such as `m`
/// is never taken for `merge`).
enum class Option : std::uint8_t { add, remove, tolerance, merge, associate, series, parallel, derive };

std::optional<Option> option_word(std::string_view word) {
  struct Entry {
    std::string_view word;
    Option option;
  };
  static constexpr std::array<Entry, 11> kOptions{{{"add", Option::add},
                                                   {"create", Option::add},
                                                   {"remove", Option::remove},
                                                   {"delete", Option::remove},
                                                   {"tolerance", Option::tolerance},
                                                   {"merge", Option::merge},
                                                   {"associate", Option::associate},
                                                   {"series", Option::series},
                                                   {"serial", Option::series},
                                                   {"parallel", Option::parallel},
                                                   {"derive", Option::derive}}};
  for (const auto& entry : kOptions) {
    if (entry.word == word) {
      return entry.option;
    }
  }
  return std::nullopt;
}

std::string_view type_name(PropertyType type) noexcept {
  switch (type) {
    case PropertyType::integer:
      return "integer";
    case PropertyType::real:
      return "double";
    case PropertyType::value:
      return "value";
    case PropertyType::expression:
      return "expression";
    case PropertyType::string:
      break;
  }
  return "string";
}

PropertyType type_of(std::size_t word) noexcept {
  switch (word) {
    case kInteger:
      return PropertyType::integer;
    case kDouble:
      return PropertyType::real;
    case kValue:
      return PropertyType::value;
    case kExpression:
      return PropertyType::expression;
    default:
      return PropertyType::string;
  }
}

/// The definition `{key type}` declares (types' default tolerances).
PropertyDef typed(std::string key, std::size_t word) {
  PropertyDef def;
  def.key = std::move(key);
  def.type = type_of(word);
  def.tolerance = (word == kDouble || word == kValue) ? 0.01 : 0.0;
  if (word == kInteger) {
    def.default_value = "0";
  }
  return def;
}

diag::Outcome<double> tolerance_number(std::string_view key, std::string_view text, bool integer) {
  if (integer) {
    if (const auto n = parse_integer(text)) {
      return static_cast<double>(*n);
    }
  } else if (const auto x = parse_real(text)) {
    return *x;
  }
  return command_error("Property " + std::string{key} + ": tolerance \"" + std::string{text} + "\" is not a number")
      .diagnostics.front();
}

/// `{key type tol}`.
diag::Outcome<PropertyDef> three_word_spec(const std::vector<std::string>& spec) {
  const auto word = match_keyword(spec[1], kTypes);
  if (!word) {
    return command_error(bad_option("type", spec[1], kTypes)).diagnostics.front();
  }
  PropertyDef def = typed(spec[0], *word);
  if (*word == kExpression) {
    def.default_value = spec[2];
    return def;
  }
  const bool integer = *word == kInteger || *word == kString;
  auto tol = tolerance_number(spec[0], spec[2], integer);
  if (!tol) {
    return tol.error();
  }
  def.tolerance = tol.value();
  return def;
}

/// One `add` spec (Q-LVS-6).
diag::Outcome<PropertyDef> parse_spec(std::string_view text) {
  auto list = split_list(text);
  if (!list) {
    return list.error();
  }
  const auto& spec = list.value();
  if (spec.size() == 1) {
    PropertyDef def;
    def.key = spec[0];
    const bool numeric = parse_real(spec[0]).has_value();  // netgen's single-word rule, copied
    def.type = numeric ? PropertyType::real : PropertyType::string;
    def.tolerance = numeric ? 0.01 : 0.0;
    return def;
  }
  if (spec.size() == 2) {
    if (const auto word = match_keyword(spec[1], kTypes)) {
      return typed(spec[0], *word);
    }
    auto tol = tolerance_number(spec[0], spec[1], false);  // Q-LVS-6 fix: an error, never a garbage value
    if (!tol) {
      return tol.error();
    }
    PropertyDef def;
    def.key = spec[0];
    def.type = PropertyType::real;
    def.tolerance = tol.value();
    return def;
  }
  if (spec.size() == 3) {
    return three_word_spec(spec);
  }
  return command_error("Bad property specification \"" + std::string{text} + "\"").diagnostics.front();
}

cmd::CommandResult add(Runtime& runtime, const Resolved& cells, std::span<const std::string> specs) {
  for (const auto& text : specs) {
    auto def = parse_spec(text);
    if (!def) {
      auto result = cmd::CommandResult::failure(def.error());
      result.raises = true;
      return result;
    }
    for (const auto& target : cells.targets) {
      Cell& cell = target.get();
      if (cell.find_property_def(def.value().key) != nullptr) {
        runtime.print("Device " + cell.name + " already has property named \"" + def.value().key + "\"");
        continue;
      }
      cell.properties.push_back(def.value());
    }
  }
  return cmd::CommandResult::success();
}

cmd::Value list_of(const Cell& cell) {
  cmd::Value::List out;
  for (const auto& def : cell.properties) {
    cmd::Value tolerance = def.tolerance;
    if (def.type != PropertyType::real && def.type != PropertyType::value) {
      tolerance = static_cast<std::int64_t>(def.tolerance);
    }
    out.emplace_back(cmd::Value::List{def.key, std::string{type_name(def.type)}, tolerance});
  }
  return out;
}

cmd::CommandResult remove(Runtime& runtime, const Resolved& cells, std::span<const std::string> keys) {
  for (const auto& target : cells.targets) {
    Cell& cell = target.get();
    if (keys.empty()) {
      cell.properties.clear();
      continue;
    }
    for (const auto& key : keys) {
      const auto removed = std::erase_if(
          cell.properties, [&key](const PropertyDef& def) { return names_equal(def.key, key, true); });
      if (removed == 0) {
        report_missing_property(runtime, cell, key);
      }
    }
  }
  return cmd::CommandResult::success();
}

/// The tolerance `tol` gives `def` ("property remove, tolerance and associate").
void set_tolerance(PropertyDef& def, std::string_view tol) {
  const bool integer = def.type == PropertyType::integer || def.type == PropertyType::string;
  const bool infinite = tol.size() >= 3 && (tol[0] | 0x20) == 'i' && (tol[1] | 0x20) == 'n' && (tol[2] | 0x20) == 'f';
  if (infinite) {
    def.tolerance = integer ? static_cast<double>(1U << 30U) : 1e300;
    return;
  }
  if (const auto n = parse_integer(tol)) {
    def.tolerance = static_cast<double>(*n);
  } else if (const auto x = parse_real(tol)) {
    def.tolerance = integer ? std::trunc(*x) : *x;
  }
}

/// Applies `{key value}` pairs with `apply`; a malformed pair sets `bad` as
/// the result and processing continues.
template <typename Apply>
cmd::CommandResult each_pair(Runtime& runtime, const Resolved& cells, std::span<const std::string> pairs,
                             std::string_view bad, Apply apply) {
  cmd::CommandResult result = cmd::CommandResult::success();
  for (const auto& text : pairs) {
    auto pair = split_list(text);
    if (!pair || pair.value().size() != 2) {
      result.value = std::string{bad};
      continue;
    }
    for (const auto& target : cells.targets) {
      Cell& cell = target.get();
      PropertyDef* def = find_definition(cell, pair.value()[0]);
      if (def == nullptr) {
        report_missing_property(runtime, cell, pair.value()[0]);
        continue;
      }
      apply(*def, pair.value()[1]);
    }
  }
  return result;
}

cmd::CommandResult topology(Runtime& runtime, const Words& words) {
  static constexpr std::array<std::string_view, 2> kWords{"strict", "relaxed"};
  if (words.size() == 1) {
    return cmd::CommandResult::success(runtime.state().strict_topology ? "Strict topology property matching."
                                                                        : "Relaxed topology property matching.");
  }
  if (words.size() != 2) {
    return wrong_args("property topology [strict|relaxed]");
  }
  const auto word = match_keyword(words[1], kWords);
  if (!word) {
    return command_error(bad_option("option", words[1], kWords));
  }
  runtime.state().strict_topology = *word == 0;
  return cmd::CommandResult::success();
}

void derive_in(NetlistDb& db, CellId klass, std::string_view derived, std::string_view wkey, std::string_view lkey) {
  const bool area = derived == "area";
  for (auto& cell : db.cells) {
    for (auto& instance : cell.instances) {
      if (instance.cell != klass) {
        continue;
      }
      for (auto& record : instance.properties) {
        const Property* w = find_property(record, wkey);
        const Property* l = find_property(record, lkey);
        if (w == nullptr || l == nullptr || find_property(record, derived) != nullptr) {
          continue;
        }
        const auto wv = parse_si_value(w->value);
        const auto lv = parse_si_value(l->value);
        if (!wv || !lv) {
          continue;
        }
        const double value = area ? *wv * *lv : 2.0 * (*wv + *lv);
        record.push_back(Property{std::string{derived}, format_g(value, 15)});
      }
    }
  }
}

cmd::CommandResult derive(Runtime& runtime, const Resolved& cells, std::span<const std::string> args) {
  static constexpr std::array<std::string_view, 2> kDerived{"area", "perimeter"};
  if (args.size() < 3) {
    return wrong_args("property <valid_cellname> derive area|perimeter <wkey> <lkey>");
  }
  const auto word = match_keyword(args[0], kDerived);
  if (!word) {
    return command_error(bad_option("option", args[0], kDerived));
  }
  const std::string key{kDerived.at(*word)};
  for (const auto& target : cells.targets) {
    Cell& cell = target.get();
    if (cell.find_property_def(key) == nullptr) {
      cell.properties.push_back(PropertyDef{key, PropertyType::real, 0.01, "", 0, ""});
    }
    derive_in(target.circuit->db, target.cell, key, args[1], args[2]);
  }
  (void)runtime;
  return cmd::CommandResult::success();
}

cmd::CommandResult cell_option(Runtime& runtime, const Resolved& cells, const Words& words) {
  const auto option = option_word(words[1]);
  const std::span<const std::string> rest{words.begin() + (option ? 2 : 1), words.end()};
  switch (option.value_or(Option::add)) {
    case Option::add:
      return add(runtime, cells, rest);
    case Option::remove:
      return remove(runtime, cells, rest);
    case Option::tolerance:
      return each_pair(runtime, cells, rest, "Not a {key tolerance} pair list.",
                       [](PropertyDef& def, const std::string& tol) { set_tolerance(def, tol); });
    case Option::associate:
      return each_pair(runtime, cells, rest, "Not a {key pin} pair list.",
                       [](PropertyDef& def, const std::string& pin) { def.pin = pin; });
    case Option::merge:
      return merge_option(runtime, cells, rest);
    case Option::series:
    case Option::parallel:
      return combine_option(runtime, cells, *option == Option::series, rest);
    case Option::derive:
      return derive(runtime, cells, rest);
  }
  return cmd::CommandResult::success();
}

}  // namespace

PropertyDef* find_definition(Cell& cell, std::string_view key) noexcept {
  for (auto& def : cell.properties) {
    if (names_equal(def.key, key, true)) {
      return &def;
    }
  }
  return nullptr;
}

void report_missing_property(Runtime& runtime, const Cell& cell, std::string_view key) {
  runtime.print("No property " + std::string{key} + " found for device " + cell.name);
}

void apply_property_default(Runtime& runtime) {
  using namespace merge_rule;
  struct Rule {
    std::string_view key;
    std::uint16_t merge;
  };
  static constexpr std::array<Rule, 1> kTransistor{{{"w", kParallelAdd | kParallelCritical}}};
  static constexpr std::array<Rule, 3> kResistor{{{"w", kParallelPar | kSeriesCritical},
                                                  {"l", kSeriesAdd | kParallelCritical},
                                                  {"value", kSeriesAdd | kParallelPar}}};
  static constexpr std::array<Rule, 2> kCapacitor{
      {{"area", kParallelAdd | kSeriesPar}, {"value", kParallelAdd | kSeriesPar}}};
  static constexpr std::array<Rule, 1> kInductor{{{"value", kSeriesAdd | kParallelPar}}};
  const auto apply = [&runtime](Cell& cell, std::span<const Rule> rules) {
    for (const auto& rule : rules) {
      if (PropertyDef* def = find_definition(cell, rule.key)) {
        def->merge |= rule.merge;
      } else {
        report_missing_property(runtime, cell, rule.key);
      }
    }
  };
  for (Circuit* circuit : runtime.circuits()) {
    for (auto& cell : circuit->db.cells) {
      const DeviceClass c = cell.device_class;
      if (is_transistor(c)) {
        apply(cell, kTransistor);
        std::erase_if(cell.properties, [](const PropertyDef& def) {
          return names_equal(def.key, "as", true) || names_equal(def.key, "ad", true) ||
                 names_equal(def.key, "ps", true) || names_equal(def.key, "pd", true);
        });
      } else if (is_resistor(c) || is_capacitor(c) || c == DeviceClass::inductor) {
        apply(cell, is_resistor(c) ? std::span<const Rule>{kResistor}
                    : is_capacitor(c) ? std::span<const Rule>{kCapacitor}
                                      : std::span<const Rule>{kInductor});
        cell.flags.series_combinable = true;
      }
    }
  }
}

cmd::CommandResult property_command(Runtime& runtime, const Words& words) {
  if (words.empty()) {
    return wrong_args(kUsage);
  }
  if (words[0] == "default" && words.size() == 1) {
    apply_property_default(runtime);
    return cmd::CommandResult::success();
  }
  if (words[0] == "topology") {
    return topology(runtime, words);
  }
  if ((words[0] == "parallel" || words[0] == "series") && words.size() == 2) {
    return global_combine(runtime, words[0] == "series", words[1]);
  }
  auto cells = runtime.resolve(words[0]);
  if (!cells) {
    return command_error(cells.error().text);
  }
  if (words.size() == 1) {
    return cmd::CommandResult::success(list_of(cells.value().targets.front().get()));
  }
  return cell_option(runtime, cells.value(), words);
}

}  // namespace hefesto::lvs::setup
