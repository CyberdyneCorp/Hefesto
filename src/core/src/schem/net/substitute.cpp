// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Property sources and hierarchical parameters (capability
// schematic-netlisting "Hierarchical parameters", "Format string
// substitution"; schematic-file-format "Boolean values";
// schematic-symbols-and-libraries "Templates and instance attributes",
// "Symbol generators").
//
// Owner: NETLIST.

#include "hefesto/schem/net/substitute.hpp"

#include <array>
#include <initializer_list>

#include "format_scan.hpp"

namespace hefesto::schem {

namespace {

bool has_sigil(std::string_view text) noexcept {
  return text.find_first_of("@%") != std::string_view::npos;
}

std::optional<std::string> get_from(const PropertySource* source, std::string_view key, ValueForm form) {
  return source != nullptr ? source->get(key, form) : std::nullopt;
}

/// "Lookup in S1, S2, ...": the first source that defines `key` gives v;
/// each later source that defines a key equal to v replaces v with its value.
std::optional<std::string> lookup_chain(std::string_view key, std::span<const PropertySource* const> sources,
                                        ValueForm form) {
  std::optional<std::string> value;
  for (const PropertySource* source : sources) {
    if (source == nullptr) {
      continue;
    }
    if (!value) {
      value = source->get(key, form);
    } else if (auto next = source->get(*value, form)) {
      value = std::move(next);
    }
  }
  return value;
}

/// `text` with every token replaced by its chain lookup; unresolved tokens
/// are kept as written.
std::string substitute_tokens(std::string_view text, std::span<const PropertySource* const> sources,
                              ValueForm form) {
  std::string out;
  detail::scan_format(
      text, false, [&](char c) { out += c; },
      [&](char /*sigil*/, std::string key, std::string_view source) {
        const auto value = lookup_chain(key, sources, form);
        out += value ? *value : std::string{source};
      });
  return out;
}

bool is_generator(std::string_view reference) noexcept {
  while (!reference.empty() && (reference.back() == ' ' || reference.back() == '\t')) {
    reference.remove_suffix(1);
  }
  return !reference.empty() && reference.back() == ')' && reference.find('(') != std::string_view::npos;
}

bool is_symname_separator(char c) noexcept {
  return c == ' ' || c == '.' || c == '(' || c == ')' || c == ',';
}

}  // namespace

std::optional<std::string> PropertyString::get(std::string_view key, ValueForm form) const {
  return get_token(props_, key, form);
}

std::optional<std::string> PropertyMap::get(std::string_view key, ValueForm /*form*/) const {
  const auto it = values_.find(key);
  if (it == values_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<bool> boolean_value(std::string_view value) noexcept {
  if (is_true(value)) {
    return true;
  }
  if (is_false(value)) {
    return false;
  }
  return std::nullopt;
}

std::optional<std::string> select_format(const ElementContext& context, bool lvs_netlist) {
  if (lvs_netlist) {
    if (auto f = get_from(context.instance, "lvs_format", ValueForm::decoded)) {
      return f;
    }
    if (auto f = get_from(context.symbol, "lvs_format", ValueForm::decoded)) {
      return f;
    }
  }
  if (auto f = get_from(context.instance, "format", ValueForm::decoded)) {
    return f;
  }
  return get_from(context.symbol, "format", ValueForm::decoded);
}

std::string instance_name(const ElementContext& context) {
  if (auto name = get_from(context.instance, "name", ValueForm::decoded)) {
    return *name;
  }
  return get_from(context.templ, "name", ValueForm::decoded).value_or(std::string{});
}

std::optional<std::string> resolve_parameter(const ElementContext& context, std::string_view key) {
  const std::array<const PropertySource*, 4> everywhere{context.instance, context.templ, context.parent_instance,
                                                        context.parent_template};
  bool defined = false;
  for (const PropertySource* source : everywhere) {
    defined = defined || (source != nullptr && source->defines(key));
  }
  if (!defined) {
    return std::nullopt;
  }
  // 1. The instance, then the template ("Templates and instance attributes");
  //    an unresolved token is kept as text.
  std::string value = get_from(context.instance, key, ValueForm::decoded)
                          .value_or(get_from(context.templ, key, ValueForm::decoded).value_or("@" + std::string{key}));
  // 2. The instance, the parent instance, the symbol template.
  if (has_sigil(value)) {
    const std::array<const PropertySource*, 3> sources{context.instance, context.parent_instance, context.templ};
    value = substitute_tokens(value, sources, ValueForm::decoded);
  }
  // 3. The instance alone, then the instance and the parent symbol's
  //    template, keeping backslash escapes.
  if (has_sigil(value)) {
    const std::array<const PropertySource*, 1> instance_only{context.instance};
    value = substitute_tokens(value, instance_only, ValueForm::decoded);
  }
  if (has_sigil(value)) {
    const std::array<const PropertySource*, 2> sources{context.instance, context.parent_template};
    value = substitute_tokens(value, sources, ValueForm::raw);
  }
  // 4. Still a token: empty.
  if (!value.empty() && value.front() == '@') {
    value.clear();
  }
  return value;
}

std::string substitute_from_parents(std::string_view text, std::span<const PropertySource* const> parents) {
  std::string value{text};
  for (const PropertySource* parent : parents) {
    if (!has_sigil(value)) {
      break;
    }
    const std::array<const PropertySource*, 1> one{parent};
    value = substitute_tokens(value, one, ValueForm::decoded);
  }
  return value;
}

std::string symbol_name(std::string_view reference) {
  const std::size_t slash = reference.rfind('/');
  if (slash != std::string_view::npos) {
    reference.remove_prefix(slash + 1);
  }
  if (!is_generator(reference)) {
    const std::size_t dot = reference.rfind('.');
    if (dot != std::string_view::npos) {
      reference = reference.substr(0, dot);
    }
  }
  std::string out;
  for (std::size_t i = 0; i < reference.size();) {
    if (!is_symname_separator(reference[i])) {
      out += reference[i++];
      continue;
    }
    while (i < reference.size() && is_symname_separator(reference[i])) {
      ++i;
    }
    out += '_';
  }
  if (!out.empty() && out.back() == '_') {
    out.pop_back();
  }
  return out;
}

}  // namespace hefesto::schem
