// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// SPICE/CDL element cards (lvs-netlist-input "SPICE primitive device
// cards", "SPICE subcircuit calls and placeholders"). The primitive cards
// are driven by one table: terminals, model position, value token, model
// cell ports, class and properties of interest.
//
// Owner: LVSREAD.

#include <array>
#include <span>
#include <utility>

#include "hefesto/lvs/net/expression.hpp"
#include "hefesto/lvs/net/names.hpp"
#include "hefesto/lvs/net/si_value.hpp"
#include "hefesto/lvs/read/codes.hpp"
#include "lvs/read/spice_reader.hpp"

namespace hefesto::lvs::spice {

namespace codes = read_codes;

namespace {

struct PropertySpec {
  std::string_view key;
  PropertyType type;
  double tolerance;
  std::string_view default_value;
};

constexpr std::array<PropertySpec, 3> kMosProperties{{
    {"L", PropertyType::real, 0.01, "0"},
    {"W", PropertyType::real, 0.01, "0"},
    {"M", PropertyType::integer, 0.0, "1"},
}};
constexpr std::array<PropertySpec, 2> kValueProperties{{
    {"value", PropertyType::value, 0.01, "0"},
    {"M", PropertyType::integer, 0.0, "1"},
}};
constexpr std::array<PropertySpec, 1> kMultiplier{{{"M", PropertyType::integer, 0.0, "1"}}};
constexpr std::array<PropertySpec, 1> kBuiltinResistor{{{"value", PropertyType::real, 0.01, "0"}}};

struct CardSpec {
  char letter;
  std::size_t terminals;
  bool model_first;  // the token after the terminals is the model (M, Q, D)
  bool takes_value;  // a number or expression token is property `value`
  std::array<std::string_view, 4> ports;
  DeviceClass device_class;
  std::string_view default_model;  // empty: a model is required
  std::span<const PropertySpec> properties;
};

// The model of an L, T, V, I or E card without one is the card letter in
// lower case, as `c` and `r` are for C and R (lvs-netlist-input names only
// the C and R defaults).
const std::array<CardSpec, 10> kCards{{
    {'M', 4, true, false, {"drain", "gate", "source", "bulk"}, DeviceClass::fet, "", kMosProperties},
    {'Q', 3, true, false, {"collector", "base", "emitter", ""}, DeviceClass::bjt, "", kMultiplier},
    {'D', 2, true, false, {"anode", "cathode", "", ""}, DeviceClass::diode, "", kMultiplier},
    {'C', 2, false, true, {"top", "bottom", "", ""}, DeviceClass::capacitor, "c", kValueProperties},
    {'R', 2, false, true, {"end_a", "end_b", "", ""}, DeviceClass::resistor, "r", kValueProperties},
    {'L', 2, false, true, {"end_a", "end_b", "", ""}, DeviceClass::inductor, "l", kMultiplier},
    {'T', 4, false, false, {"node1", "node2", "node3", "node4"}, DeviceClass::xline, "t", kMultiplier},
    {'V', 2, false, true, {"pos", "neg", "", ""}, DeviceClass::vsource, "v", kMultiplier},
    {'I', 2, false, true, {"pos", "neg", "", ""}, DeviceClass::isource, "i", kMultiplier},
    {'E', 4, false, true, {"pos", "neg", "ctrlp", "ctrln"}, DeviceClass::module, "e", kMultiplier},
}};

char upper(char c) noexcept {
  return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
}

const CardSpec& card_spec(char letter) {
  for (const auto& spec : kCards) {
    if (spec.letter == upper(letter)) {
      return spec;
    }
  }
  return kCards.front();  // unreachable: process() dispatches only these letters
}

struct ParsedCard {
  std::vector<std::string_view> nodes;
  std::string model;
  bool default_model = false;
  PropertyRecord record;
};

/// Splits `key=value`; nullopt when the token has no `=` after its first byte.
std::optional<Property> key_value(std::string_view token) {
  const auto eq = token.find('=');
  if (eq == std::string_view::npos || eq == 0) {
    return std::nullopt;
  }
  return Property{std::string{token.substr(0, eq)}, std::string{token.substr(eq + 1)}};
}

/// The terminals, model, value and key=value properties of a primitive
/// card; nullopt when it is badly formed.
std::optional<ParsedCard> parse_card(const CardSpec& spec, const Tokens& tokens) {
  if (tokens.size() < 1 + spec.terminals) {
    return std::nullopt;
  }
  ParsedCard card;
  for (std::size_t i = 1; i <= spec.terminals; ++i) {
    card.nodes.emplace_back(tokens[i]);
  }
  bool value_seen = false;
  for (std::size_t i = 1 + spec.terminals; i < tokens.size(); ++i) {
    const std::string& token = tokens[i];
    if (token.starts_with("$[") && token.ends_with(']') && token.size() > 3) {
      card.model = token.substr(2, token.size() - 3);
    } else if (auto property = key_value(token)) {
      card.record.push_back(std::move(*property));
    } else if (spec.model_first && card.model.empty()) {
      card.model = token;
    } else if (spec.takes_value && !value_seen && card.model.empty() &&
               (is_si_number(token) || is_expression_text(token))) {
      card.record.push_back(Property{"value", token});
      value_seen = true;
    } else if (card.model.empty()) {
      card.model = token;
    }
  }
  if (card.model.empty()) {
    if (spec.default_model.empty()) {
      return std::nullopt;
    }
    card.model = spec.default_model;
    card.default_model = true;
  }
  return card;
}

}  // namespace

diag::Status SpiceReader::primitive(FileState& file, const Tokens& tokens) {
  const CardSpec& spec = card_spec(tokens.front().front());
  if (options().ignore_rc && (spec.letter == 'C' || spec.letter == 'R')) {
    return diag::Done{};
  }
  auto card = parse_card(spec, tokens);
  if (!card) {
    warn(file, codes::kBadlyFormed, "Badly formed line in input.");
    return diag::Done{};
  }
  auto model_id = db().find_cell(card->model, file_number());
  if (model_id && cell(*model_id).ports.size() != spec.terminals) {
    warn(file, codes::kBadlyFormed,
         "Device \"" + card->model + "\" has wrong number of ports for a " +
             std::string{device_class_name(spec.device_class)} + ".");
    warn(file, codes::kBadlyFormed, "Badly formed line in input.");
    return diag::Done{};
  }
  if (!model_id) {
    model_id = new_cell(card->model, spec.device_class);
    Cell& model = cell(*model_id);
    for (std::size_t i = 0; i < spec.terminals; ++i) {
      model.ports.push_back(Port{std::string{spec.ports.at(i)}, model.add_net(std::string{spec.ports.at(i)}), false});
    }
    const auto properties = (spec.letter == 'R' && card->default_model) ? std::span<const PropertySpec>{kBuiltinResistor}
                                                                         : spec.properties;
    for (const auto& property : properties) {
      model.properties.push_back(
          PropertyDef{std::string{property.key}, property.type, property.tolerance, std::string{property.default_value}});
    }
  }
  if (auto status = add_device(file); !status) {
    return status;
  }
  const CellId owner = current_cell();
  Instance instance;
  instance.cell = *model_id;
  instance.name = card->model + ":" + tokens.front().substr(1);
  for (const auto node : card->nodes) {
    instance.connections.push_back(cell(owner).net_named(node));
  }
  instance.properties.push_back(std::move(card->record));
  cell(owner).instances.push_back(std::move(instance));
  return diag::Done{};
}

namespace {

struct CallParts {
  std::vector<std::string> positional;
  PropertyRecord parameters;
};

}  // namespace

diag::Status SpiceReader::call(FileState& file, const Tokens& tokens) {
  CallParts parts;
  bool in_parameters = false;
  for (std::size_t i = 1; i < tokens.size(); ++i) {
    const std::string& token = tokens[i];
    if (token == "/" || keyword_equal(token, "params:")) {
      continue;
    }
    auto property = key_value(token);
    if (in_parameters && !property) {
      warn(file, codes::kNotParameter, "Token \"" + token + "\" is not a parameter!");
    } else if (property && (in_parameters || !db().find_cell_exact(token, file_number()))) {
      in_parameters = true;
      parts.parameters.push_back(std::move(*property));
    } else {
      parts.positional.push_back(token);
    }
  }
  if (parts.positional.empty()) {
    warn(file, codes::kBadlyFormed, "Badly formed line in input.");
    return diag::Done{};
  }
  std::string class_name = std::move(parts.positional.back());
  parts.positional.pop_back();
  if (class_name.starts_with('/') && class_name.size() > 1) {
    class_name.erase(0, 1);
  }
  if (const auto self = defining(); self && names_equal(cell(*self).name, class_name, case_insensitive())) {
    return error(file, codes::kSelfInstance, "Fatal: Class \"" + class_name + "\" is instanced inside of itself!");
  }
  for (const auto& entry : options().ignore) {
    if (!names_equal(entry.name, class_name, case_insensitive())) {
      continue;
    }
    if (entry.kind == IgnoredClass::Kind::class_) {
      note(file, codes::kIgnoredCall, "Class '" + class_name + "' instanced in input but is being ignored.");
      return diag::Done{};
    }
    bool shorted = !parts.positional.empty();
    for (const auto& node : parts.positional) {
      shorted = shorted && names_equal(node, parts.positional.front(), true);
    }
    if (shorted) {
      note(file, codes::kIgnoredCall, "Instance of '" + class_name + "' is shorted, ignoring.");
      return diag::Done{};
    }
  }
  return add_call(file, tokens.front().substr(1), class_name, parts.positional, std::move(parts.parameters));
}

diag::Status SpiceReader::add_call(FileState& file, const std::string& name, const std::string& class_name,
                                   const std::vector<std::string>& nodes, PropertyRecord parameters) {
  auto class_id = db().find_cell(class_name, file_number());
  if (!class_id) {
    warn(file, codes::kUndefinedCell, "Call to undefined subcircuit " + class_name);
    warn(file, codes::kUndefinedCell, "Creating placeholder cell definition.");
    class_id = new_cell(class_name, DeviceClass::module);
    Cell& placeholder = cell(*class_id);
    placeholder.flags.placeholder = true;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
      const std::string pin = std::to_string(i + 1);
      placeholder.ports.push_back(Port{pin, placeholder.add_net(pin), false});
    }
    if (nodes.empty()) {
      placeholder.ports.push_back(Port{"", placeholder.add_net(""), true});
    }
    placeholder.properties.push_back(PropertyDef{"M", PropertyType::integer, 0.0, "1"});
  }
  const Cell& klass = cell(*class_id);
  const bool proxy_only = klass.ports.size() == 1 && klass.ports.front().proxy;
  const std::size_t expected = proxy_only ? 0 : klass.ports.size();
  const std::string instance_name =
      name_starts_with(name, class_name, case_insensitive()) ? "/" + name : class_name + ":" + name;
  if (nodes.size() != expected) {
    warn(file, codes::kParameterMismatch,
         "Parameter list mismatch in " + instance_name + ": " +
             (nodes.size() > expected ? "Too many parameters in call!" : "Not enough parameters in call!"));
  }
  if (auto status = add_device(file); !status) {
    return status;
  }
  const std::size_t ports = klass.ports.size();
  const CellId owner = current_cell();
  Instance instance;
  instance.cell = *class_id;
  instance.name = instance_name;
  for (std::size_t i = 0; i < nodes.size() && i < ports; ++i) {
    instance.connections.push_back(cell(owner).net_named(nodes[i]));
  }
  instance.connections.resize(ports, kNoNet);
  instance.properties.push_back(std::move(parameters));
  cell(owner).instances.push_back(std::move(instance));
  return diag::Done{};
}

}  // namespace hefesto::lvs::spice
