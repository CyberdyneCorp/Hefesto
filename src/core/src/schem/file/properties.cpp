// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The property-string grammar (capability schematic-file-format "Property
// strings", "Boolean values"). Attributes are found by one scanner that
// records where each key and value sit, so lookups and updates work on the
// stored text and leave every other byte as it was.
//
// Owner: FORMAT.

#include "hefesto/schem/file/properties.hpp"

#include <cstddef>

namespace hefesto::schem {

namespace {

bool is_separator(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\n' || c == ';';
}

// Where one attribute sits in its property string.
struct AttributeSpan {
  std::size_t key_begin = 0;
  std::size_t key_end = 0;
  bool has_value = false;
  std::size_t value_begin = 0;
  std::size_t value_end = 0;

  [[nodiscard]] std::size_t end() const noexcept { return has_value ? value_end : key_end; }
};

class Scanner {
 public:
  explicit Scanner(std::string_view text) noexcept : text_(text) {}

  // The next attribute, or false at the end of the string.
  bool next(AttributeSpan& span) noexcept {
    skip_separators(pos_);
    if (pos_ >= text_.size()) {
      return false;
    }
    span = AttributeSpan{};
    span.key_begin = pos_;
    span.key_end = scan_token(pos_, true);
    std::size_t look = span.key_end;
    skip_separators(look);
    if (look < text_.size() && text_[look] == '=') {
      ++look;
      skip_separators(look);
      span.has_value = true;
      span.value_begin = look;
      span.value_end = scan_token(look, false);
    }
    pos_ = span.end();
    if (pos_ == span.key_begin) {
      ++pos_;  // cannot happen for a non-separator byte; keeps the scan finite regardless
    }
    return true;
  }

 private:
  void skip_separators(std::size_t& at) const noexcept {
    while (at < text_.size() && is_separator(text_[at])) {
      ++at;
    }
  }

  // The end of a key (`stop_at_equals`) or value: the first unquoted,
  // unescaped separator (or `=` for a key).
  [[nodiscard]] std::size_t scan_token(std::size_t at, bool stop_at_equals) const noexcept {
    bool quoted = false;
    while (at < text_.size()) {
      const char c = text_[at];
      if (c == '\\') {
        at = at + 2 < text_.size() ? at + 2 : text_.size();
        continue;
      }
      if (c == '"') {
        quoted = !quoted;
      } else if (!quoted && (is_separator(c) || (stop_at_equals && c == '='))) {
        break;
      }
      ++at;
    }
    return at;
  }

  std::string_view text_;
  std::size_t pos_ = 0;
};

std::optional<AttributeSpan> find_attribute(std::string_view props, std::string_view key) noexcept {
  Scanner scanner{props};
  AttributeSpan span;
  while (scanner.next(span)) {
    if (props.substr(span.key_begin, span.key_end - span.key_begin) == key) {
      return span;
    }
  }
  return std::nullopt;
}

std::string remove_attribute(std::string_view props, const AttributeSpan& span) {
  std::size_t end = span.end();
  if (end < props.size() && (props[end] == ' ' || props[end] == '\n')) {
    ++end;
  }
  std::string out{props.substr(0, span.key_begin)};
  out.append(props.substr(end));
  return out;
}

// Rule 1: a value with a separator is quoted, except for `name`.
std::string quoted_value(std::string_view key, std::string_view value) {
  const bool enclosed = value.size() >= 2 && value.front() == '"' && value.back() == '"';
  const bool needs_quotes = value.find_first_of(" \t\n;") != std::string_view::npos;
  if (!needs_quotes || enclosed || key == "name") {
    return std::string{value};
  }
  return "\"" + std::string{value} + "\"";
}

// Rule 2: replace the value of the first occurrence.
std::string replace_value(std::string_view props, const AttributeSpan& span, std::string_view value) {
  std::string out;
  if (span.has_value) {
    out.append(props.substr(0, span.value_begin)).append(value).append(props.substr(span.value_end));
    return out;
  }
  out.append(props.substr(0, span.key_end));
  if (span.key_end < props.size()) {  // the key is followed by a separator
    out.push_back(props[span.key_end]);
    out.append("=").append(value).append(" ");
    out.append(props.substr(span.key_end + 1));
    return out;
  }
  out.append("=").append(value).append(" ");  // at the end of the string: key, `=v` and one space
  return out;
}

// Rules 3 and 4: append a new attribute.
std::string append_attribute(std::string_view props, std::string_view key, std::string_view value) {
  std::string out{props};
  if (!props.empty() && props.back() != ' ' && props.back() != '\n') {
    out.push_back('\n');
  }
  out.append(key).append("=").append(value);
  return out;
}

char ascii_lower(char c) noexcept {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool equals_ignoring_case(std::string_view a, std::string_view b) noexcept {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (ascii_lower(a[i]) != ascii_lower(b[i])) {
      return false;
    }
  }
  return true;
}

// nullopt when `value` is not made of decimal digits only, else whether one
// of them is not zero.
std::optional<bool> digits_value(std::string_view value) noexcept {
  if (value.empty()) {
    return std::nullopt;
  }
  bool non_zero = false;
  for (const char c : value) {
    if (c < '0' || c > '9') {
      return std::nullopt;
    }
    non_zero = non_zero || c != '0';
  }
  return non_zero;
}

}  // namespace

PropertyList parse_properties(std::string_view props) {
  PropertyList out;
  Scanner scanner{props};
  AttributeSpan span;
  while (scanner.next(span)) {
    Property property;
    property.key = std::string{props.substr(span.key_begin, span.key_end - span.key_begin)};
    property.has_value = span.has_value;
    if (span.has_value) {
      property.raw_value = std::string{props.substr(span.value_begin, span.value_end - span.value_begin)};
    }
    out.push_back(std::move(property));
  }
  return out;
}

std::string decode_value(std::string_view raw) {
  std::string out;
  out.reserve(raw.size());
  bool escaped = false;
  for (const char c : raw) {
    if (escaped) {
      out.push_back(c);
      escaped = false;
    } else if (c == '\\') {
      escaped = true;
    } else if (c != '"') {
      out.push_back(c);
    }
  }
  return out;
}

std::optional<std::string> get_token(std::string_view props, std::string_view key, ValueForm form) {
  const auto span = find_attribute(props, key);
  if (!span) {
    return std::nullopt;
  }
  if (!span->has_value) {
    return std::string{};
  }
  const std::string_view raw = props.substr(span->value_begin, span->value_end - span->value_begin);
  return form == ValueForm::raw ? std::string{raw} : decode_value(raw);
}

std::string set_token(std::string_view props, std::string_view key, std::optional<std::string_view> value) {
  const auto span = find_attribute(props, key);
  if (!value || value->empty()) {
    return span ? remove_attribute(props, *span) : std::string{props};
  }
  const std::string written = quoted_value(key, *value);
  if (span) {
    return replace_value(props, *span, written);
  }
  return append_attribute(props, key, written);
}

bool is_true(std::string_view value) noexcept {
  if (const auto digits = digits_value(value)) {
    return *digits;
  }
  return equals_ignoring_case(value, "true") || equals_ignoring_case(value, "on") ||
         equals_ignoring_case(value, "yes");
}

bool is_false(std::string_view value) noexcept {
  if (const auto digits = digits_value(value)) {
    return !*digits;
  }
  return equals_ignoring_case(value, "false") || equals_ignoring_case(value, "off") ||
         equals_ignoring_case(value, "no");
}

}  // namespace hefesto::schem
