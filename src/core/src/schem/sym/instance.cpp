// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Instance attributes, templates, unique instance names and floater texts
// (capability schematic-symbols-and-libraries "Templates and instance
// attributes", "Instance names", "Floater texts").
//
// Owner: SYMLIB.

#include "hefesto/schem/sym/instance.hpp"

#include <algorithm>

#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/net/bus.hpp"

namespace hefesto::schem {

namespace {

std::string ascii_lower(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; });
  return out;
}

}  // namespace

std::string symbol_template(const Symbol& symbol) {
  return get_token(symbol.property(), "template").value_or("");
}

std::optional<std::string> instance_attribute(std::string_view instance_props, const Symbol& symbol,
                                              std::string_view key) {
  if (auto value = get_token(instance_props, key)) return value;
  return get_token(symbol_template(symbol), key);
}

std::string new_instance_properties(const Symbol& symbol, InstanceNamer& names) {
  return names.unique_properties(symbol_template(symbol));
}

InstanceNamer::InstanceNamer(const Document& document) {
  for (const InstanceRecord& instance : document.instances) {
    if (auto name = get_token(instance.props, "name")) add(*name);
  }
}

void InstanceNamer::add(std::string_view name) {
  if (name.empty()) return;
  for (const std::string& bit : expand_bus(name)) used_.insert(ascii_lower(bit));
}

bool InstanceNamer::used(std::string_view name) const {
  if (name.empty()) return false;
  const auto bits = expand_bus(name);
  return std::any_of(bits.begin(), bits.end(), [this](const std::string& bit) { return used_.count(ascii_lower(bit)) != 0; });
}

std::string InstanceNamer::unique(std::string_view name) {
  if (name.empty()) return {};
  if (!used(name)) {
    add(name);
    return std::string{name};
  }
  auto key = split_instance_name(name);
  long long& last = last_index_.try_emplace(key, 1).first->second;
  // Terminates: every candidate is distinct and only finitely many names
  // are used.
  for (long long q = last;; ++q) {
    std::string candidate = key.first + std::to_string(q) + key.second;
    if (used(candidate)) continue;
    last = q;
    add(candidate);
    return candidate;
  }
}

std::string InstanceNamer::unique_properties(std::string_view props) {
  const auto name = get_token(props, "name");
  if (!name.has_value()) return std::string{props};
  const std::string renamed = unique(*name);
  if (renamed == *name) return std::string{props};
  return set_token(props, "name", renamed);
}

std::pair<std::string, std::string> split_instance_name(std::string_view name) {
  const std::size_t base_end = name.find_first_of("0123456789[");
  const std::size_t bracket = name.find('[');
  std::string base{name.substr(0, base_end)};
  std::string suffix = bracket == std::string_view::npos ? std::string{} : std::string{name.substr(bracket)};
  return {std::move(base), std::move(suffix)};
}

FloaterInfo floater_info(const TextRecord& text) {
  FloaterInfo info;
  if (auto name = get_token(text.props, "name")) {
    info.floater = true;
    info.owner = std::move(*name);
  } else if (auto floater = get_token(text.props, "floater")) {
    info.floater = true;
    // A true value is looked up as the text `true` ("Floater texts").
    info.owner = is_true(*floater) ? std::string{"true"} : std::move(*floater);
  }
  return info;
}

const InstanceRecord* floater_owner(const Document& document, std::string_view owner) {
  const bool index = !owner.empty() && owner.find_first_not_of("0123456789") == std::string_view::npos;
  if (index) {
    const std::size_t digits = owner.find_first_not_of('0');
    const std::string_view value = digits == std::string_view::npos ? std::string_view{"0"} : owner.substr(digits);
    // More digits than any instance count can have: out of range.
    if (value.size() > 18) return nullptr;
    const auto at = static_cast<std::size_t>(std::stoull(std::string{value}));
    return at < document.instances.size() ? &document.instances[at] : nullptr;
  }
  for (const InstanceRecord& instance : document.instances) {
    if (get_token(instance.props, "name") == owner) return &instance;
  }
  return nullptr;
}

bool resolve_without_owner(std::string_view text) noexcept {
  return text.starts_with("tcleval(") || text.find_first_of("@%") != std::string_view::npos;
}

}  // namespace hefesto::schem
