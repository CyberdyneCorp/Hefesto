// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Bus and vector notation (capability schematic-netlisting "Bus and vector
// notation"): a recursive-descent parser over
//
//   list   = term { "," term }
//   term   = factor { "*" int }           each element repeated n times
//   factor = int "*" factor | "(" list ")" | item
//   item   = name [ "[" index "]" [ trailer ] ]
//
// with "*" binding tighter than ",". Spaces and tabs between tokens are
// skipped. A name is a run of characters other than `,*()[]`, space and tab.
// Every step checks kMaxBusBits before it allocates, and integer arithmetic
// is checked, so hostile names cannot overflow or exhaust memory.
//
// Owner: NETLIST.

#include "hefesto/schem/net/bus.hpp"

#include <charconv>
#include <cstdint>
#include <optional>

namespace hefesto::schem {

namespace {

using Bits = std::vector<std::string>;

/// Deepest `(` nesting accepted; deeper input is treated as too large.
constexpr int kMaxParenDepth = 256;

bool is_space(char c) noexcept {
  return c == ' ' || c == '\t';
}

bool is_digit(char c) noexcept {
  return c >= '0' && c <= '9';
}

bool is_name_char(char c) noexcept {
  switch (c) {
    case ',':
    case '*':
    case '(':
    case ')':
    case '[':
    case ']':
    case ' ':
    case '\t':
      return false;
    default:
      return true;
  }
}

bool needs_parsing(std::string_view name) noexcept {
  if (!name.empty() && name.front() == '$') {
    return false;
  }
  return name.find_first_of("*,.:") != std::string_view::npos;
}

/// One range of an index: `first`, then `count` values `step` apart, and
/// the whole sequence repeated `repeat` times with `extend` added each time.
struct Range {
  std::int64_t first = 0;
  std::int64_t step = 1;  // signed
  std::uint64_t count = 1;
  std::int64_t extend = 0;
  std::uint64_t repeat = 1;
};

std::optional<std::int64_t> checked_add(std::int64_t a, std::int64_t b) noexcept {
  std::int64_t out = 0;
  if (__builtin_add_overflow(a, b, &out)) {
    return std::nullopt;
  }
  return out;
}

std::optional<std::int64_t> checked_mul(std::int64_t a, std::int64_t b) noexcept {
  std::int64_t out = 0;
  if (__builtin_mul_overflow(a, b, &out)) {
    return std::nullopt;
  }
  return out;
}

std::uint64_t distance(std::int64_t a, std::int64_t b) noexcept {
  return a <= b ? static_cast<std::uint64_t>(b) - static_cast<std::uint64_t>(a)
                : static_cast<std::uint64_t>(a) - static_cast<std::uint64_t>(b);
}

enum class Failure : std::uint8_t { none, syntax, too_large };

class Parser {
 public:
  explicit Parser(std::string_view text) : text_{text} {}

  std::optional<Bits> parse() {
    auto bits = list(0);
    skip_space();
    if (bits && pos_ != text_.size()) {
      fail(Failure::syntax);
    }
    if (failure_ != Failure::none) {
      return std::nullopt;
    }
    return bits;
  }

  [[nodiscard]] Failure failure() const noexcept { return failure_; }
  [[nodiscard]] bool literal_index() const noexcept { return literal_index_; }

 private:
  std::optional<Bits> list(int depth) {
    auto out = term(depth);
    while (out) {
      skip_space();
      if (!accept(',')) {
        break;
      }
      auto more = term(depth);
      if (!more || !fits(out->size() + more->size())) {
        return std::nullopt;
      }
      out->insert(out->end(), more->begin(), more->end());
    }
    return out;
  }

  std::optional<Bits> term(int depth) {
    auto out = factor(depth);
    while (out) {
      skip_space();
      if (!accept('*')) {
        break;
      }
      const auto n = count();
      if (!n || !fits_product(out->size(), *n)) {
        return std::nullopt;
      }
      Bits each;
      each.reserve(out->size() * *n);
      for (const auto& bit : *out) {
        each.insert(each.end(), *n, bit);
      }
      out = std::move(each);
    }
    return out;
  }

  std::optional<Bits> factor(int depth) {
    skip_space();
    if (const auto n = leading_repeat()) {
      auto inner = factor(depth);
      if (!inner || !fits_product(inner->size(), *n)) {
        return std::nullopt;
      }
      Bits out;
      out.reserve(inner->size() * *n);
      for (std::uint64_t i = 0; i < *n; ++i) {
        out.insert(out.end(), inner->begin(), inner->end());
      }
      return out;
    }
    if (accept('(')) {
      return group(depth + 1);
    }
    return item();
  }

  std::optional<Bits> group(int depth) {
    if (depth > kMaxParenDepth) {
      fail(Failure::too_large);
      return std::nullopt;
    }
    auto out = list(depth);
    skip_space();
    if (out && !accept(')')) {
      fail(Failure::syntax);
      return std::nullopt;
    }
    return out;
  }

  /// `n *` before a factor: digits followed by `*`.
  std::optional<std::uint64_t> leading_repeat() {
    std::size_t end = pos_;
    while (end < text_.size() && is_digit(text_[end])) {
      ++end;
    }
    std::size_t star = end;
    while (star < text_.size() && is_space(text_[star])) {
      ++star;
    }
    if (end == pos_ || star >= text_.size() || text_[star] != '*') {
      return std::nullopt;
    }
    const auto n = count();
    skip_space();
    accept('*');
    return n;
  }

  std::optional<Bits> item() {
    const std::size_t start = pos_;
    while (pos_ < text_.size() && is_name_char(text_[pos_])) {
      ++pos_;
    }
    const std::string_view name = text_.substr(start, pos_ - start);
    if (pos_ >= text_.size() || text_[pos_] != '[') {
      if (name.empty()) {
        fail(Failure::syntax);
        return std::nullopt;
      }
      return Bits{std::string{name}};
    }
    return indexed(start, name);
  }

  std::optional<Bits> indexed(std::size_t start, std::string_view name) {
    const std::size_t close = text_.find(']', pos_);
    if (close == std::string_view::npos) {
      fail(Failure::syntax);
      return std::nullopt;
    }
    const std::string_view index = text_.substr(pos_ + 1, close - pos_ - 1);
    pos_ = close + 1;
    const std::string_view trailer = take_trailer();
    if (!starts_numeric(index)) {
      literal_index_ = true;
      return Bits{std::string{text_.substr(start, pos_ - start)}};
    }
    return expand_index(name, index, trailer);
  }

  std::string_view take_trailer() {
    const std::size_t start = pos_;
    while (pos_ < text_.size() && text_[pos_] != ',' && text_[pos_] != '*' && text_[pos_] != '(' &&
           text_[pos_] != ')' && !is_space(text_[pos_])) {
      ++pos_;
    }
    return text_.substr(start, pos_ - start);
  }

  static bool starts_numeric(std::string_view index) noexcept {
    std::size_t i = 0;
    while (i < index.size() && is_space(index[i])) {
      ++i;
    }
    return i < index.size() && (is_digit(index[i]) || index[i] == '-');
  }

  std::optional<Bits> expand_index(std::string_view name, std::string_view index, std::string_view trailer) {
    const bool bracketless = index.find("..") != std::string_view::npos;
    if (bracketless && index.find(':') != std::string_view::npos) {
      fail(Failure::syntax);
      return std::nullopt;
    }
    Bits out;
    std::size_t at = 0;
    while (true) {
      const std::size_t comma = index.find(',', at);
      const std::string_view part = index.substr(at, comma == std::string_view::npos ? index.npos : comma - at);
      if (!append_range(out, name, part, bracketless, trailer)) {
        return std::nullopt;
      }
      if (comma == std::string_view::npos) {
        return out;
      }
      at = comma + 1;
    }
  }

  bool append_range(Bits& out, std::string_view name, std::string_view part, bool bracketless,
                    std::string_view trailer) {
    const auto range = parse_range(part, bracketless ? std::string_view{".."} : std::string_view{":"});
    if (!range) {
      fail(Failure::syntax);
      return false;
    }
    if (range->count > kMaxBusBits || range->repeat > kMaxBusBits ||
        !fits_product(range->count, range->repeat) || !fits(out.size() + range->count * range->repeat)) {
      fail(Failure::too_large);
      return false;
    }
    for (std::uint64_t r = 0; r < range->repeat; ++r) {
      for (std::uint64_t k = 0; k < range->count; ++k) {
        const auto value = range_value(*range, r, k);
        if (!value) {
          fail(Failure::syntax);
          return false;
        }
        out.push_back(bit_name(name, *value, bracketless, trailer));
      }
    }
    return true;
  }

  static std::optional<std::int64_t> range_value(const Range& range, std::uint64_t r, std::uint64_t k) noexcept {
    const auto offset = checked_mul(static_cast<std::int64_t>(k), range.step);
    const auto shift = checked_mul(static_cast<std::int64_t>(r), range.extend);
    if (!offset || !shift) {
      return std::nullopt;
    }
    const auto base = checked_add(range.first, *offset);
    return base ? checked_add(*base, *shift) : std::nullopt;
  }

  static std::string bit_name(std::string_view name, std::int64_t value, bool bracketless, std::string_view trailer) {
    std::string out{name};
    out += bracketless ? std::to_string(value) : "[" + std::to_string(value) + "]";
    out += trailer;
    return out;
  }

  /// `i`, `i:j`, `i:j:step` or `i:j:ext:rep` (or the same with `..`).
  static std::optional<Range> parse_range(std::string_view part, std::string_view sep) {
    std::vector<std::int64_t> fields;
    std::size_t at = 0;
    while (fields.size() < 5) {
      const std::size_t next = part.find(sep, at);
      const auto value = parse_int(part.substr(at, next == std::string_view::npos ? part.npos : next - at));
      if (!value) {
        return std::nullopt;
      }
      fields.push_back(*value);
      if (next == std::string_view::npos) {
        break;
      }
      at = next + sep.size();
    }
    return make_range(fields);
  }

  static std::optional<Range> make_range(const std::vector<std::int64_t>& f) {
    Range range;
    range.first = f[0];
    if (f.size() == 1) {
      return range;
    }
    const std::int64_t sign = f[1] < f[0] ? -1 : 1;
    const std::uint64_t span = distance(f[0], f[1]);
    if (f.size() == 2 || f.size() == 4) {
      range.step = sign;
      range.count = span >= kMaxBusBits ? kMaxBusBits + 1 : span + 1;
    }
    if (f.size() == 3) {
      if (f[2] <= 0) {
        return std::nullopt;
      }
      range.step = sign * f[2];
      const std::uint64_t steps = span / static_cast<std::uint64_t>(f[2]);
      range.count = steps >= kMaxBusBits ? kMaxBusBits + 1 : steps + 1;
    }
    if (f.size() == 4) {
      if (f[3] <= 0) {
        return std::nullopt;
      }
      range.extend = f[2];
      range.repeat = static_cast<std::uint64_t>(f[3]);
    }
    if (f.size() > 4) {
      return std::nullopt;
    }
    return range;
  }

  static std::optional<std::int64_t> parse_int(std::string_view text) {
    while (!text.empty() && is_space(text.front())) {
      text.remove_prefix(1);
    }
    while (!text.empty() && is_space(text.back())) {
      text.remove_suffix(1);
    }
    std::int64_t value = 0;
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || ec != std::errc{} || end != text.data() + text.size()) {
      return std::nullopt;
    }
    return value;
  }

  /// A repetition count: digits, at least 1.
  std::optional<std::uint64_t> count() {
    skip_space();
    const std::size_t start = pos_;
    while (pos_ < text_.size() && is_digit(text_[pos_])) {
      ++pos_;
    }
    std::uint64_t n = 0;
    const auto [end, ec] = std::from_chars(text_.data() + start, text_.data() + pos_, n);
    if (start == pos_ || ec == std::errc::invalid_argument || (ec == std::errc{} && n == 0)) {
      fail(Failure::syntax);  // a count of 0 would leave no name at all
      return std::nullopt;
    }
    if (ec == std::errc::result_out_of_range || n > kMaxBusBits) {
      fail(Failure::too_large);
      return std::nullopt;
    }
    return n;
  }

  bool fits(std::uint64_t size) {
    if (size > kMaxBusBits) {
      fail(Failure::too_large);
      return false;
    }
    return true;
  }

  bool fits_product(std::uint64_t a, std::uint64_t b) {
    if (b != 0 && a > kMaxBusBits / b) {
      fail(Failure::too_large);
      return false;
    }
    return fits(a * b);
  }

  void skip_space() noexcept {
    while (pos_ < text_.size() && is_space(text_[pos_])) {
      ++pos_;
    }
  }

  bool accept(char c) noexcept {
    if (pos_ < text_.size() && text_[pos_] == c) {
      ++pos_;
      return true;
    }
    return false;
  }

  void fail(Failure failure) noexcept {
    if (failure_ == Failure::none) {
      failure_ = failure;
    }
  }

  std::string_view text_;
  std::size_t pos_ = 0;
  Failure failure_ = Failure::none;
  bool literal_index_ = false;
};

}  // namespace

BusExpansion parse_bus(std::string_view name) {
  BusExpansion out;
  if (!needs_parsing(name)) {
    out.bits.emplace_back(name);
    return out;
  }
  Parser parser{name};
  auto bits = parser.parse();
  if (!bits) {
    out.bits.emplace_back(name);
    out.width_known = false;
    out.syntax_error = parser.failure() == Failure::syntax;
    out.too_large = parser.failure() == Failure::too_large;
    return out;
  }
  out.bits = std::move(*bits);
  out.width_known = !parser.literal_index();
  return out;
}

std::vector<std::string> expand_bus(std::string_view name) {
  return parse_bus(name).bits;
}

std::string replace_bus_chars(std::string_view name, std::string_view replacement) {
  std::string out{name};
  if (replacement.size() != 2) {
    return out;
  }
  for (char& c : out) {
    if (c == '[') {
      c = replacement[0];
    } else if (c == ']') {
      c = replacement[1];
    }
  }
  return out;
}

std::string join_bits(std::span<const std::string> bits, std::string_view separator) {
  std::string out;
  for (std::size_t i = 0; i < bits.size(); ++i) {
    if (i != 0) {
      out += separator;
    }
    out += bits[i];
  }
  return out;
}

}  // namespace hefesto::schem
