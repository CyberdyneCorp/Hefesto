// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Ports of Tcl 8.6.16 generic/tclUtil.c (FindElement, TclCopyAndCollapse,
// TclScanElement, TclConvertElement with COMPAT 1) and generic/tclParse.c
// (TclParseBackslash), restricted to what list values need.

#include "hefesto/pcell/framework/tcl_list.hpp"

#include <cstddef>
#include <cstdint>
#include <variant>

#include "pcell/framework/codes.hpp"

namespace hefesto::pcell {

namespace {

// TclIsSpaceProcM: space, \t, \n, \v, \f, \r.
bool is_space(char c) {
  return c == ' ' || (c >= '\t' && c <= '\r');
}

bool is_octal(char c) {
  return c >= '0' && c <= '7';
}

int hex_value(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

void append_utf8(std::string& out, std::uint32_t cp) {
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

// ParseHex: up to `max_digits` hex digits; returns the count read.
std::size_t parse_hex(std::string_view s, std::size_t max_digits, std::uint32_t& value) {
  std::size_t n = 0;
  value = 0;
  while (n < max_digits && n < s.size() && hex_value(s[n]) >= 0) {
    value = (value << 4) | static_cast<std::uint32_t>(hex_value(s[n]));
    ++n;
  }
  return n;
}

// The octal escape \o, \oo or \ooo (at most 0377); `s` starts after '\'.
std::size_t parse_octal(std::string_view s, std::uint32_t& value) {
  value = static_cast<std::uint32_t>(s[0] - '0');
  std::size_t n = 1;
  if (s.size() > 1 && is_octal(s[1])) {
    value = (value << 3) + static_cast<std::uint32_t>(s[1] - '0');
    n = 2;
    if (s.size() > 2 && is_octal(s[2]) && value < 0x20) {
      value = ((value << 3) + static_cast<std::uint32_t>(s[2] - '0')) & 0xFF;
      n = 3;
    }
  }
  return n;
}

// The length of the UTF-8 sequence starting with `lead` (1 for a stray byte).
std::size_t utf8_length(unsigned char lead) {
  if (lead >= 0xF0 && lead < 0xF8) {
    return 4;
  }
  if (lead >= 0xE0) {
    return lead < 0xF0 ? 3 : 1;
  }
  return lead >= 0xC0 ? 2 : 1;
}

struct Backslash {
  std::size_t read = 0;  // bytes consumed, the backslash included
  std::string text;      // the substituted UTF-8 text
};

Backslash simple_escape(char c) {
  switch (c) {
    case 'a': return {2, "\a"};
    case 'b': return {2, "\b"};
    case 'f': return {2, "\f"};
    case 'n': return {2, "\n"};
    case 'r': return {2, "\r"};
    case 't': return {2, "\t"};
    case 'v': return {2, "\v"};
    default: return {};
  }
}

// \xHH, \uHHHH, \UHHHHHHHH (`c` is x, u or U); without digits the letter itself.
Backslash hex_escape(std::string_view s, char c) {
  const std::size_t max_digits = c == 'x' ? 2 : (c == 'u' ? 4 : 8);
  std::uint32_t value = 0;
  const std::size_t n = parse_hex(s.substr(2), max_digits, value);
  Backslash out{2 + n, {}};
  if (n == 0) {
    out.text = std::string(1, c);
  } else {
    append_utf8(out.text, c == 'x' ? (value & 0xFF) : value);
  }
  return out;
}

// Backslash-newline and the spaces and tabs after it become one space.
Backslash newline_escape(std::string_view s) {
  std::size_t i = 2;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
    ++i;
  }
  return {i, " "};
}

// TclParseBackslash for `s` starting at the backslash (TCL_UTF_MAX 3).
Backslash parse_backslash(std::string_view s) {
  if (s.size() == 1) {
    return {1, "\\"};
  }
  if (Backslash simple = simple_escape(s[1]); simple.read != 0) {
    return simple;
  }
  const char c = s[1];
  if (c == 'x' || c == 'u' || c == 'U') {
    return hex_escape(s, c);
  }
  if (c == '\n') {
    return newline_escape(s);
  }
  if (is_octal(c)) {
    std::uint32_t value = 0;
    const std::size_t n = parse_octal(s.substr(1), value);
    Backslash out{1 + n, {}};
    append_utf8(out.text, value);
    return out;
  }
  const std::size_t n = std::min(utf8_length(static_cast<unsigned char>(c)), s.size() - 1);
  return {1 + n, std::string{s.substr(1, n)}};
}

// TclCopyAndCollapse: the element value of an unbraced element.
std::string collapse(std::string_view s) {
  std::string out;
  std::size_t i = 0;
  while (i < s.size()) {
    if (s[i] != '\\') {
      out += s[i++];
      continue;
    }
    const Backslash b = parse_backslash(s.substr(i));
    out += b.text;
    i += b.read;
  }
  return out;
}

diag::Diagnostic list_error(std::string text) {
  return diag::make(codes::kListSyntax, std::move(text));
}

std::string junk_after(std::string_view s, std::size_t at, std::string_view kind) {
  std::size_t end = at;
  while (end < s.size() && !is_space(s[end]) && end < at + 20) {
    ++end;
  }
  return "list element in " + std::string{kind} + " followed by \"" + std::string{s.substr(at, end - at)} +
         "\" instead of space";
}

struct Element {
  std::size_t next = 0;  // position after the element and the white space after it
  std::string value;
};

// FindElement for a braced element starting after its '{' at `p`.
diag::Outcome<Element> braced_element(std::string_view s, std::size_t p) {
  const std::size_t start = p;
  int depth = 1;
  for (; p < s.size(); ++p) {
    if (s[p] == '\\') {
      p += parse_backslash(s.substr(p)).read - 1;
    } else if (s[p] == '{') {
      ++depth;
    } else if (s[p] == '}' && --depth == 0) {
      if (p + 1 < s.size() && !is_space(s[p + 1])) {
        return list_error(junk_after(s, p + 1, "braces"));
      }
      return Element{p + 1, std::string{s.substr(start, p - start)}};
    }
  }
  return list_error("unmatched open brace in list");
}

// FindElement for a quoted element starting after its '"' at `p`, or a bare
// element starting at `p` (quoted = false).
diag::Outcome<Element> plain_element(std::string_view s, std::size_t p, bool quoted) {
  const std::size_t start = p;
  for (; p < s.size(); ++p) {
    if (s[p] == '\\') {
      p += parse_backslash(s.substr(p)).read - 1;
    } else if (quoted && s[p] == '"') {
      if (p + 1 < s.size() && !is_space(s[p + 1])) {
        return list_error(junk_after(s, p + 1, "quotes"));
      }
      return Element{p + 1, collapse(s.substr(start, p - start))};
    } else if (!quoted && is_space(s[p])) {
      break;
    }
  }
  if (quoted) {
    return list_error("unmatched open quote in list");
  }
  return Element{p, collapse(s.substr(start, p - start))};
}

diag::Outcome<Element> find_element(std::string_view s, std::size_t p) {
  if (s[p] == '{') {
    return braced_element(s, p + 1);
  }
  if (s[p] == '"') {
    return plain_element(s, p + 1, true);
  }
  return plain_element(s, p, false);
}

// TclScanElement's verdict (COMPAT 1).
enum class Conversion : std::uint8_t { none, brace, escape, mask };

struct Scan {
  bool forbid_none = false;
  bool require_escape = false;
  bool prefer_escape = false;
  bool prefer_brace = false;
  int depth = 0;
};

void scan_backslash(std::string_view s, std::size_t& i, Scan& scan) {
  if (i + 1 == s.size()) {
    scan.require_escape = true;  // final backslash
    return;
  }
  const char next = s[i + 1];
  if (next == '\n') {
    scan.require_escape = true;  // backslash-newline
    ++i;
    return;
  }
  if (next == '{' || next == '}' || next == '\\') {
    ++i;
  }
  scan.forbid_none = true;
  scan.prefer_brace = true;
}

void scan_char(std::string_view s, std::size_t& i, Scan& scan) {
  switch (s[i]) {
    case '{':
      ++scan.depth;
      break;
    case '}':
      if (--scan.depth < 0) {
        scan.require_escape = true;
      }
      break;
    case ']':
    case '"':
      scan.forbid_none = true;
      scan.prefer_escape = true;
      break;
    case '[':
    case '$':
    case ';':
      scan.forbid_none = true;
      scan.prefer_brace = true;
      break;
    case '\\':
      scan_backslash(s, i, scan);
      break;
    default:
      if (is_space(s[i])) {
        scan.forbid_none = true;
        scan.prefer_brace = true;
      }
      break;
  }
}

Conversion scan_element(std::string_view s, bool quote_hash) {
  if (s.empty()) {
    return Conversion::brace;
  }
  Scan scan;
  scan.prefer_brace = s.front() == '#' && quote_hash;
  if (s.front() == '{' || s.front() == '"') {
    scan.forbid_none = true;
    scan.prefer_brace = true;
  }
  for (std::size_t i = 0; i < s.size(); ++i) {
    scan_char(s, i, scan);
  }
  if (scan.require_escape || scan.depth != 0) {
    return Conversion::escape;
  }
  if (scan.forbid_none) {
    return scan.prefer_escape && !scan.prefer_brace ? Conversion::mask : Conversion::brace;
  }
  return Conversion::none;
}

// TclConvertElement's escape and mask modes.
std::string escaped(std::string_view s, Conversion conversion) {
  std::string out;
  for (const char c : s) {
    switch (c) {
      case ']': case '[': case '$': case ';': case ' ': case '\\': case '"':
        out += '\\';
        break;
      case '{':
      case '}':
        if (conversion == Conversion::escape) {
          out += '\\';
        }
        break;
      case '\f': out += "\\f"; continue;
      case '\n': out += "\\n"; continue;
      case '\r': out += "\\r"; continue;
      case '\t': out += "\\t"; continue;
      case '\v': out += "\\v"; continue;
      default:
        break;
    }
    out += c;
  }
  return out;
}

}  // namespace

diag::Outcome<std::vector<std::string>> split_tcl_list(std::string_view text) {
  std::vector<std::string> elements;
  std::size_t p = 0;
  while (true) {
    while (p < text.size() && is_space(text[p])) {
      ++p;
    }
    if (p == text.size()) {
      return elements;
    }
    auto element = find_element(text, p);
    if (!element.ok()) {
      return element.error();
    }
    elements.push_back(std::move(element.value().value));
    p = element.value().next;
  }
}

std::string tcl_list_element(std::string_view element, bool first) {
  Conversion conversion = scan_element(element, first);
  std::string prefix;
  if (!element.empty() && element.front() == '#' && first) {
    if (conversion == Conversion::escape) {
      prefix = "\\#";
      element.remove_prefix(1);
    } else {
      conversion = Conversion::brace;
    }
  }
  switch (conversion) {
    case Conversion::none:
      return std::string{element};
    case Conversion::brace:
      return "{" + std::string{element} + "}";
    case Conversion::escape:
    case Conversion::mask:
      break;
  }
  return prefix + escaped(element, conversion);
}

namespace {

struct ValueText {
  std::string operator()(std::monostate) const { return {}; }
  std::string operator()(std::int64_t v) const { return std::to_string(v); }
  std::string operator()(double v) const { return cmd::Value{v}.to_string(); }
  std::string operator()(const std::string& v) const { return v; }
  std::string operator()(const cmd::Value::List& list) const {
    std::vector<std::string> words;
    words.reserve(list.size());
    for (const auto& element : list) {
      words.push_back(tcl_string(element));
    }
    return merge_tcl_list(words);
  }
};

}  // namespace

std::string tcl_string(const cmd::Value& value) {
  return std::visit(ValueText{}, value.data());
}

std::string merge_tcl_list(std::span<const std::string> elements) {
  std::string out;
  bool first = true;
  for (const auto& element : elements) {
    if (!first) {
      out += ' ';
    }
    out += tcl_list_element(element, first);
    first = false;
  }
  return out;
}

}  // namespace hefesto::pcell
