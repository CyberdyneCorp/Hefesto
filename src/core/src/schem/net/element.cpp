// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The element text of one instance (capability schematic-netlisting "Format
// string substitution", "Unnamed nets", "expr evaluation"; Q-SCHEM-26).
//
// Owner: NETLIST.

#include <charconv>
#include <initializer_list>

#include "format_scan.hpp"
#include "hefesto/schem/net/bus.hpp"
#include "hefesto/schem/net/codes.hpp"
#include "hefesto/schem/net/expr.hpp"
#include "hefesto/schem/net/substitute.hpp"

namespace hefesto::schem {

namespace {

constexpr std::string_view kUndefined = "--UNDEF--";

bool is_identifier_char(char c) noexcept {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

std::optional<std::size_t> parse_index(std::string_view text) noexcept {
  std::size_t value = 0;
  const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (text.empty() || ec != std::errc{} || end != text.data() + text.size()) {
    return std::nullopt;
  }
  return value;
}

/// The `k`-th (1-based) `:`-separated item of `value`, or `value` itself.
std::string slot_item(std::string_view value, std::size_t k) {
  std::size_t at = 0;
  for (std::size_t i = 1; i < k; ++i) {
    at = value.find(':', at);
    if (at == std::string_view::npos) {
      return std::string{value};
    }
    ++at;
  }
  return std::string{value.substr(at, value.find(':', at) - at)};
}

class ElementBuilder {
 public:
  ElementBuilder(const ElementContext& context, const ElementOptions& options, ElementRunState& state)
      : ctx_{context}, opts_{options}, state_{state}, name_{instance_name(context)} {}

  ElementText run() {
    const auto format = select_format(ctx_, opts_.lvs_netlist);
    if (!format) {
      return std::move(out_);
    }
    std::string text;
    detail::scan_format(
        *format, true, [&](char c) { text += c; },
        [&](char sigil, std::string key, std::string_view source) { text += token(sigil, key, source); });
    text += '\n';
    if (text.starts_with("tcleval(")) {
      text = evaluate_tcl(text, false);
    }
    out_.text = expr(text);
    return std::move(out_);
  }

 private:
  std::string token(char sigil, const std::string& key, std::string_view source) {
    if (auto pin = pin_token(key)) {
      return *pin;
    }
    if (auto named = name_token(key)) {
      return *named;
    }
    if (key.starts_with("tcleval(")) {
      return evaluate_tcl(key, true);
    }
    return parameter(sigil, key, source);
  }

  std::optional<std::string> pin_token(const std::string& key) {
    if (key == "pinlist") {
      return pin_list();
    }
    if (key.starts_with("@")) {
      const std::string_view pin = std::string_view{key}.substr(1);
      for (std::size_t i = 0; i < ctx_.pins.size(); ++i) {
        if (ctx_.pins[i].name == pin) {
          return ctx_.pins[i].spice_ignore ? std::string{} : pin_entry(i);
        }
      }
      return std::string{};
    }
    if (key.starts_with("#")) {
      return numbered_pin(std::string_view{key}.substr(1));
    }
    return std::nullopt;
  }

  std::string pin_list() {
    std::string out;
    for (std::size_t i = 0; i < ctx_.pins.size(); ++i) {
      if (!ctx_.pins[i].spice_ignore && first_of_name(i)) {
        out += pin_entry(i);
      }
    }
    return out;
  }

  [[nodiscard]] bool first_of_name(std::size_t i) const {
    for (std::size_t j = 0; j < i; ++j) {
      if (ctx_.pins[j].name == ctx_.pins[i].name) {
        return false;
      }
    }
    return true;
  }

  /// `?<w> <nets> ` for pin `i`.
  std::string pin_entry(std::size_t i) {
    const ElementPin& pin = ctx_.pins[i];
    std::string nets = pin.net;
    if (nets.empty()) {
      nets = unconnected(i);
    }
    return "?" + std::to_string(pin.width) + " " + nets + " ";
  }

  std::string unconnected(std::size_t i) {
    const ElementPin& pin = ctx_.pins[i];
    const std::string base = "__UNCONNECTED_PIN__" + std::to_string(state_.unconnected++);
    out_.diagnostics.push_back(diag::make(net_codes::kUnconnectedPin,
                                          "unconnected pin,  Inst idx: " + std::to_string(ctx_.index) +
                                              ", Pin idx: " + std::to_string(i) + "  Inst:" + name_));
    if (pin.width <= 1) {
      return base;
    }
    return join_bits(expand_bus(base + "_[" + std::to_string(pin.width - 1) + "..0]"), ",");
  }

  /// `@#<n>`, `@#<pin>` and `@#<n>:<attr>`.
  std::string numbered_pin(std::string_view spec) {
    const std::size_t colon = spec.find(':');
    const auto pin = find_pin(spec.substr(0, colon));
    if (colon == std::string_view::npos) {
      return pin ? pin_entry(*pin) : std::string{};
    }
    if (!pin) {
      return std::string{};  // no such pin: nothing ("Format string substitution")
    }
    return pin_attribute(*pin, spec.substr(colon + 1));
  }

  [[nodiscard]] std::optional<std::size_t> find_pin(std::string_view ref) const {
    if (const auto n = parse_index(ref)) {
      return *n < ctx_.pins.size() ? std::optional<std::size_t>{*n} : std::nullopt;
    }
    for (std::size_t i = 0; i < ctx_.pins.size(); ++i) {
      if (ctx_.pins[i].name == ref) {
        return i;
      }
    }
    return std::nullopt;
  }

  std::string pin_attribute(std::size_t i, std::string_view attr) {
    const ElementPin& pin = ctx_.pins[i];
    if (attr == "net_name") {
      return pin.net;
    }
    const std::string by_name = std::string{attr} + "(" + pin.name + ")";
    const std::string by_index = std::string{attr} + "(" + std::to_string(i) + ")";
    auto value = ctx_.instance->get(by_name, ValueForm::decoded);
    if (!value) {
      value = ctx_.instance->get(by_index, ValueForm::decoded);
    }
    if (!value && pin.attributes != nullptr) {
      value = pin.attributes->get(attr, ValueForm::decoded);
    }
    if (!value) {
      return std::string{kUndefined};
    }
    return attr == "pinnumber" ? slotted(*value) : *value;
  }

  /// For an instance name ending in `:<k>`, the k-th item of a `:` list.
  std::string slotted(const std::string& value) const {
    const std::size_t colon = name_.rfind(':');
    if (colon == std::string::npos || value.find(':') == std::string::npos) {
      return value;
    }
    const auto k = parse_index(std::string_view{name_}.substr(colon + 1));
    return k && *k > 0 ? slot_item(value, *k) : value;
  }

  std::optional<std::string> name_token(const std::string& key) {
    if (key == "name") {
      return expanded(name_);
    }
    if (key == "symname") {
      return ctx_.symname ? *ctx_.symname : symbol_name(ctx_.symref);
    }
    if (key == "symname_ext") {
      const std::size_t slash = ctx_.symref.rfind('/');
      return slash == std::string::npos ? ctx_.symref : ctx_.symref.substr(slash + 1);
    }
    if (key == "symref") {
      return ctx_.symref;
    }
    return schematic_token(key);
  }

  std::optional<std::string> schematic_token(const std::string& key) {
    if (key == "schname_ext") {
      const std::size_t slash = ctx_.schname.rfind('/');
      return slash == std::string::npos ? ctx_.schname : ctx_.schname.substr(slash + 1);
    }
    if (key == "schname") {
      std::string name = *schematic_token("schname_ext");
      const std::size_t dot = name.rfind('.');
      return dot == std::string::npos ? name : name.substr(0, dot);
    }
    if (key == "topschname") {
      return ctx_.topschname;
    }
    if (key == "index") {
      return std::to_string(ctx_.index);
    }
    return special_token(key);
  }

  std::optional<std::string> special_token(const std::string& key) {
    if (key == "savecurrent") {
      auto flag = ctx_.instance->get("savecurrent", ValueForm::decoded);
      if (!flag && ctx_.templ != nullptr) {
        flag = ctx_.templ->get("savecurrent", ValueForm::decoded);
      }
      const bool on = flag && boolean_value(*flag).value_or(false);
      return on ? "\n.save I( ?1 " + expanded(name_) + " )" : std::string{};
    }
    if (key == "spiceprefix") {
      if (opts_.spiceprefix) {
        out_.spice_prefix = resolve_parameter(ctx_, "spiceprefix").value_or(std::string{});
      }
      return std::string{};
    }
    return std::nullopt;
  }

  std::string parameter(char sigil, const std::string& key, std::string_view source) {
    auto value = resolve_parameter(ctx_, key);
    if (!value) {
      warn_prefix(source, key);
      return sigil == '%' ? key : std::string{};
    }
    if (value->starts_with("tcleval(")) {
      *value = evaluate_tcl(*value, false);
    }
    if (key == "lab") {
      *value = expanded(*value);
    }
    return expr(*value);
  }

  /// Q-SCHEM-26: a defined identifier prefix of an undefined token.
  void warn_prefix(std::string_view source, std::string_view key) {
    std::size_t n = 0;
    while (n < key.size() && is_identifier_char(key[n])) {
      ++n;
    }
    const std::size_t len = longest_defined_prefix(key, std::min(n, key.size() - 1));
    if (len == 0) {
      return;
    }
    out_.diagnostics.push_back(diag::make(net_codes::kTokenPrefix, "token '" + std::string{source} +
                                                                       "' is undefined while '" +
                                                                       std::string{key.substr(0, len)} +
                                                                       "' is defined"));
  }

  /// The length of the longest prefix of `key`, at most `bound` long, that
  /// resolve_parameter() finds (a key one of its sources defines); 0 for
  /// none. Each source's keys are read once and checked as prefixes, so the
  /// work is linear in the token and the property strings.
  [[nodiscard]] std::size_t longest_defined_prefix(std::string_view key, std::size_t bound) const {
    std::size_t best = 0;
    for (const PropertySource* source : {ctx_.instance, ctx_.templ, ctx_.parent_instance, ctx_.parent_template}) {
      if (source == nullptr) {
        continue;
      }
      for (const std::string& defined : source->keys()) {
        if (defined.size() > best && defined.size() <= bound && key.starts_with(defined)) {
          best = defined.size();
        }
      }
    }
    return best;
  }

  /// `name` bus-expanded with the bus replacement characters.
  std::string expanded(std::string_view name) {
    const BusExpansion bus = parse_bus(name);
    if (auto warning = bus_diagnostic(bus, name)) {
      out_.diagnostics.push_back(std::move(*warning));
    }
    return replace_bus_chars(join_bits(bus.bits, ","), opts_.bus_replacement);
  }

  std::string evaluate_tcl(std::string_view text, bool with_instance) {
    if (opts_.tcl == nullptr) {
      return std::string{kTclFailure};
    }
    TclContext context = ctx_.tcl;
    if (with_instance) {
      context.instname = name_;
      context.symname = ctx_.symref;
    }
    return opts_.tcl->evaluate(text, context);
  }

  std::string expr(std::string_view text) {
    ExprText result = substitute_expr(text, ExprOptions{opts_.xschem_compat, opts_.ev_precision});
    for (auto& warning : result.warnings) {
      out_.diagnostics.push_back(std::move(warning));
    }
    return std::move(result.text);
  }

  const ElementContext& ctx_;
  const ElementOptions& opts_;
  ElementRunState& state_;
  std::string name_;
  ElementText out_;
};

}  // namespace

ElementText build_element(const ElementContext& context, const ElementOptions& options, ElementRunState& state) {
  static const PropertyMap kEmpty;
  if (context.instance == nullptr) {
    ElementContext copy = context;
    copy.instance = &kEmpty;
    return ElementBuilder{copy, options, state}.run();
  }
  return ElementBuilder{context, options, state}.run();
}

}  // namespace hefesto::schem
