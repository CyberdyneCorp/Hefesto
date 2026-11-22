// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `@` and `%` tokens in displayed texts (capability
// schematic-symbols-and-libraries "Text substitution tokens"; Q-SCHEM-26:
// an undefined token whose identifier prefix is defined is reported).
//
// Owner: SYMLIB.

#include "hefesto/schem/sym/tokens.hpp"

#include <algorithm>
#include <optional>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/sym/instance.hpp"
#include "hefesto/schem/sym/symbol.hpp"

namespace hefesto::schem {

namespace {

constexpr std::string_view kUndefined = "--UNDEF--";

// The predefined tokens that always have a value (Resolver::predefined();
// `spice_get_*` is matched by prefix).
constexpr std::string_view kPredefinedNames[] = {
    "name",       "symname",           "symname_ext",       "symref",
    "path",       "schname",           "schname_ext",       "topschname",
    "index",      "prop_ptr",          "sch_last_modified", "sym_last_modified",
    "time_last_modified", "lvs_ignore"};
constexpr std::string_view kSpiceGet = "spice_get_";

bool is_blank(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool is_identifier(char c) noexcept {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

bool all_digits(std::string_view text) noexcept {
  return !text.empty() && text.find_first_not_of("0123456789") == std::string_view::npos;
}

// The end of the token that starts at `text[start]` (an `@` or `%`).
std::size_t token_end(std::string_view text, std::size_t start) {
  std::size_t at = std::min(start + 2, text.size());  // the second character always belongs
  int depth = start + 1 < text.size() && text[start + 1] == '(' ? 1 : 0;
  for (; at < text.size(); ++at) {
    const char c = text[at];
    if (c == '\\') break;
    if (depth == 0 && (is_blank(c) || c == '@' || c == '%')) break;
    if (c == '(') ++depth;
    if (c == ')' && depth > 0) --depth;
  }
  return at;
}

std::string file_name(const platform::Path& file, bool extension) {
  return (extension ? file.filename() : file.stem()).generic_string();
}

std::string without_directory(std::string_view reference) {
  const std::size_t slash = reference.rfind('/');
  return std::string{slash == std::string_view::npos ? reference : reference.substr(slash + 1)};
}

std::string without_extension(const std::string& name) {
  const std::size_t dot = name.rfind('.');
  return dot == std::string::npos || dot == 0 ? name : name.substr(0, dot);
}

// The k-th `:` item of a pin number for the slotted instance name `U3:k`.
std::string slot_item(const std::string& value, std::string_view instance_name) {
  const std::size_t colon = instance_name.rfind(':');
  if (colon == std::string_view::npos || value.find(':') == std::string::npos) return value;
  const std::string_view slot = instance_name.substr(colon + 1);
  if (!all_digits(slot) || slot.size() > 6) return value;
  std::size_t k = std::stoul(std::string{slot});
  std::size_t begin = 0;
  while (k > 1 && begin != std::string::npos) {
    begin = value.find(':', begin);
    if (begin != std::string::npos) ++begin;
    --k;
  }
  if (k != 1 || begin == std::string::npos) return value;
  return value.substr(begin, value.find(':', begin) - begin);
}

class Resolver {
 public:
  explicit Resolver(const TextTokenContext& context) : context_(context) {}

  std::string resolve(std::string_view token) {
    if (token.size() < 2) return {};  // a lone `@` or `%`
    const char marker = token[0];
    const std::string_view name = token.substr(1);
    if (marker == '@' && name.starts_with('@')) return net_of(pin_named(name.substr(1)));
    if (name.starts_with('#')) return pin_token(name.substr(1));
    if (auto value = predefined(name)) return *value;
    if (auto value = attribute(name)) return *value;
    if (marker == '%') return std::string{name};
    warn_prefix(token, name);
    return {};
  }

 private:
  [[nodiscard]] const std::string& props() const {
    static const std::string kNone;
    return context_.instance != nullptr ? context_.instance->props : kNone;
  }

  [[nodiscard]] std::string reference() const {
    if (context_.instance != nullptr) return context_.instance->symbol;
    return context_.symbol != nullptr ? context_.symbol->reference : std::string{};
  }

  std::optional<std::string> attribute(std::string_view key) const {
    if (context_.instance == nullptr) return std::nullopt;
    if (context_.symbol == nullptr) return get_token(props(), key);
    return instance_attribute(props(), *context_.symbol, key);
  }

  std::optional<std::string> predefined(std::string_view name) const {
    if (name == "name") return context_.instance != nullptr ? attribute("name").value_or("") : std::string{};
    if (name == "symname") return without_extension(without_directory(reference()));
    if (name == "symname_ext") return without_directory(reference());
    if (name == "symref") return reference();
    if (name == "path") return context_.path;
    if (name == "schname") return file_name(context_.schematic, false);
    if (name == "schname_ext") return file_name(context_.schematic, true);
    if (name == "topschname") return file_name(context_.top_schematic, false);
    if (name == "index") return std::to_string(context_.index);
    if (name == "prop_ptr") return props();
    return predefined_state(name);
  }

  std::optional<std::string> predefined_state(std::string_view name) const {
    if (name == "sch_last_modified") return context_.sch_last_modified;
    if (name == "sym_last_modified") return context_.sym_last_modified;
    if (name == "time_last_modified") return context_.time_last_modified;
    if (name == "lvs_ignore") return std::string{context_.lvs_ignore ? "1" : "0"};
    // Simulation values are M9's; nothing is shown before results load.
    if (name.starts_with(kSpiceGet)) return std::string{};
    return std::nullopt;
  }

  [[nodiscard]] std::optional<PinIndex> pin_named(std::string_view name) const {
    if (context_.symbol == nullptr) return std::nullopt;
    const auto& pins = context_.symbol->pins;
    const auto it = std::find_if(pins.begin(), pins.end(), [name](const Pin& pin) { return pin.name == name; });
    if (it == pins.end()) return std::nullopt;
    return static_cast<PinIndex>(it - pins.begin());
  }

  // `n` (0-based in pin order) or a pin name.
  [[nodiscard]] std::optional<PinIndex> pin_ref(std::string_view ref) const {
    if (!all_digits(ref)) return pin_named(ref);
    if (context_.symbol == nullptr || ref.size() > 9) return std::nullopt;
    const std::vector<PinIndex>& order = symbol_pin_order();
    const std::size_t n = std::stoul(std::string{ref});
    if (n >= order.size()) return std::nullopt;
    return order[n];
  }

  // The symbol's pin order, computed once per text.
  [[nodiscard]] const std::vector<PinIndex>& symbol_pin_order() const {
    if (!pin_order_) pin_order_ = pin_order(*context_.symbol);
    return *pin_order_;
  }

  [[nodiscard]] std::string net_of(std::optional<PinIndex> pin) const {
    if (!pin || !context_.net) return {};
    return context_.net(*pin);
  }

  // `@#n`, `@#name`, `@#n:attr`.
  std::string pin_token(std::string_view rest) const {
    const std::size_t colon = rest.find(':');
    const std::optional<PinIndex> pin = pin_ref(rest.substr(0, colon));
    if (colon == std::string_view::npos) return net_of(pin);
    if (!pin) return {};
    return pin_attribute(*pin, rest.substr(0, colon), rest.substr(colon + 1));
  }

  std::string pin_attribute(PinIndex pin, std::string_view ref, std::string_view attr) const {
    if (attr == "net_name") return net_of(pin);
    const Pin& box = context_.symbol->pins[pin];
    const std::string key = std::string{attr};
    std::optional<std::string> value = get_token(props(), key + "(" + box.name + ")");
    if (!value) value = get_token(props(), key + "(" + std::string{ref} + ")");
    if (!value) value = get_token(box.box.props, attr);
    if (!value) return std::string{kUndefined};
    if (attr == "pinnumber") return slot_item(*value, attribute("name").value_or(""));
    return *value;
  }

  // Q-SCHEM-26: the longest identifier prefix that names a defined key.
  void warn_prefix(std::string_view token, std::string_view name) const {
    if (context_.warnings == nullptr || context_.instance == nullptr) return;
    std::size_t length = 0;
    while (length < name.size() && is_identifier(name[length])) ++length;
    const std::size_t n = longest_defined_prefix(name, std::min(length, name.size() - 1));
    if (n == 0) return;
    context_.warnings->emit(diag::make("SCHEM-SYM-002", "Token " + std::string{token} + " is not defined, but " +
                                                            std::string{name.substr(0, n)} +
                                                            " is: a token ends only before a blank, @ or %"));
  }

  // The length of the longest prefix of `name`, at most `bound` long, that
  // attribute() or predefined() defines; 0 for none. The defined keys are
  // listed once and each is checked as a prefix, so the work is linear in
  // the token and the property strings (trying every prefix of the token
  // would re-read the properties once per prefix).
  [[nodiscard]] std::size_t longest_defined_prefix(std::string_view name, std::size_t bound) const {
    if (name.starts_with(kSpiceGet) && bound >= kSpiceGet.size()) return bound;
    std::size_t best = 0;
    const auto consider = [&](std::string_view key) {
      if (key.size() > best && key.size() <= bound && name.starts_with(key)) best = key.size();
    };
    for (const Property& property : parse_properties(props())) consider(property.key);
    if (context_.symbol != nullptr) {
      for (const Property& property : parse_properties(symbol_template(*context_.symbol))) consider(property.key);
    }
    for (const std::string_view key : kPredefinedNames) consider(key);
    return best;
  }

  const TextTokenContext& context_;
  mutable std::optional<std::vector<PinIndex>> pin_order_;
};

}  // namespace

std::string substitute_text(std::string_view text, const TextTokenContext& context) {
  Resolver resolver(context);
  std::string out;
  out.reserve(text.size());
  std::size_t at = 0;
  while (at < text.size()) {
    const char c = text[at];
    if (c == '\\') {
      if (at + 1 < text.size()) out.push_back(text[at + 1]);
      at += 2;
      continue;
    }
    if (c != '@' && c != '%') {
      out.push_back(c);
      ++at;
      continue;
    }
    const std::size_t end = token_end(text, at);
    out += resolver.resolve(text.substr(at, end - at));
    at = end;
  }
  return out;
}

}  // namespace hefesto::schem
