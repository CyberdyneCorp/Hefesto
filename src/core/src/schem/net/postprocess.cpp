// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// "Element line post-processing" (capability schematic-netlisting;
// Q-SCHEM-18, Q-SCHEM-22): user-code blocks, continuation lines, trailing
// m=1, comment lines, the notations of element text, vector elements,
// parametric vector names, renames, save lines and subcircuit lines. The
// rules are written from the spec as behaviour; nothing here runs awk.
//
// Owner: NETLIST.

#include "hefesto/schem/net/postprocess.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <map>
#include <optional>

#include "hefesto/schem/net/bus.hpp"
#include "hefesto/schem/net/codes.hpp"
#include "line_fields.hpp"

namespace hefesto::schem {

namespace {

using detail::ascii_lower;
using detail::iequals;
using detail::is_blank;
using detail::split_fields;
using detail::starts_with_ci;

bool is_ident_start(char c) noexcept {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool is_ident_char(char c) noexcept {
  return is_ident_start(c) || (c >= '0' && c <= '9');
}

std::string_view first_field(std::string_view line) noexcept {
  const auto fields = split_fields(line);
  return fields.empty() ? std::string_view{} : fields.front();
}

// --- Parametric vector names ---------------------------------------------

using ParamValues = std::map<std::string, std::string, std::less<>>;

/// `text` with the blanks around every `=` removed.
std::string join_equals(std::string_view text) {
  std::string joined;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '=') {
      joined += text[i];
      continue;
    }
    while (!joined.empty() && is_blank(joined.back())) {
      joined.pop_back();
    }
    joined += '=';
    while (i + 1 < text.size() && is_blank(text[i + 1])) {
      ++i;
    }
  }
  return joined;
}

/// The `name=value` pairs of one `.param` line (spaces around `=` ignored).
void collect_param_pairs(std::string_view line, ParamValues& values) {
  const std::string joined = join_equals(line.substr(6));  // the fields view it
  for (const std::string_view field : split_fields(joined)) {
    const std::size_t eq = field.find('=');
    if (eq != std::string_view::npos && eq > 0) {
      values[std::string{field.substr(0, eq)}] = std::string{field.substr(eq + 1)};
    }
  }
}

/// An identifier `[a-zA-Z_][a-zA-Z0-9_]*` of `text` from `at`; its end.
std::optional<std::size_t> identifier_end(std::string_view text, std::size_t at) noexcept {
  if (at >= text.size() || !is_ident_start(text[at])) {
    return std::nullopt;
  }
  while (at < text.size() && is_ident_char(text[at])) {
    ++at;
  }
  return at;
}

struct ParametricName {
  std::string_view base;
  std::string_view first;
  std::string_view last;
};

/// `^[a-zA-Z][^\[\]:]+\[<ident>:<ident>\]$`.
std::optional<ParametricName> match_parametric(std::string_view field) noexcept {
  const std::size_t open = field.find('[');
  if (open < 2 || open == std::string_view::npos || !std::isalpha(static_cast<unsigned char>(field[0])) ||
      field.substr(0, open).find_first_of("]:") != std::string_view::npos) {
    return std::nullopt;
  }
  const auto colon = identifier_end(field, open + 1);
  if (!colon || *colon >= field.size() || field[*colon] != ':') {
    return std::nullopt;
  }
  const auto close = identifier_end(field, *colon + 1);
  if (!close || *close + 1 != field.size() || field[*close] != ']') {
    return std::nullopt;
  }
  return ParametricName{field.substr(0, open), field.substr(open + 1, *colon - open - 1),
                        field.substr(*colon + 1, *close - *colon - 1)};
}

/// The first field of a line expanded by "Parametric vector names", or
/// nullopt when it does not match or a bound has no `.param` value.
std::optional<std::string> expand_parametric(std::string_view field, const ParamValues& values, BusBudget* budget) {
  const auto name = match_parametric(field);
  if (!name) {
    return std::nullopt;
  }
  const auto first = values.find(name->first);
  const auto last = values.find(name->last);
  if (first == values.end() || last == values.end()) {
    return std::nullopt;
  }
  const BusExpansion bus =
      parse_bus(std::string{name->base} + "[" + first->second + ":" + last->second + "]", budget);
  if (!bus.width_known) {
    return std::nullopt;
  }
  return join_bits(bus.bits, ",");
}

/// `line` with its first field replaced by its parametric expansion.
std::string with_parametric_first_field(std::string_view line, const ParamValues& values, BusBudget* budget) {
  const std::string_view field = first_field(line);
  if (field.empty()) {
    return std::string{line};
  }
  const auto expanded = expand_parametric(field, values, budget);
  if (!expanded) {
    return std::string{line};
  }
  const auto at = static_cast<std::size_t>(field.data() - line.data());
  return std::string{line.substr(0, at)} + *expanded + std::string{line.substr(at + field.size())};
}

// --- Width markers and the notations of element text --------------------

/// A field of an element line after the notations are read.
struct Field {
  bool marker = false;
  std::string key{};     // `<key>=` of a marker, with its `=`
  std::string prefix{};  // `#<p>#` prefix put before each item of the next field
  std::int64_t width = 1;
  std::string text{};  // a plain field
};

/// `^-?[0-9]+$` as a width; saturated far above any useful width.
std::optional<std::int64_t> parse_width(std::string_view digits) noexcept {
  const bool negative = !digits.empty() && digits.front() == '-';
  if (negative) {
    digits.remove_prefix(1);
  }
  if (digits.empty() || !std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; })) {
    return std::nullopt;
  }
  std::int64_t value = 0;
  const auto [end, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
  if (ec == std::errc::result_out_of_range) {
    value = INT64_MAX;
  }
  return negative ? -value : value;
}

/// `^(.*=)?\?-?[0-9]+$`.
std::optional<Field> parse_marker(std::string_view text) {
  const std::size_t q = text.rfind('?');
  if (q == std::string_view::npos || (q > 0 && text[q - 1] != '=')) {
    return std::nullopt;
  }
  const auto width = parse_width(text.substr(q + 1));
  if (!width) {
    return std::nullopt;
  }
  Field field;
  field.marker = true;
  field.key = std::string{text.substr(0, q)};
  field.width = *width;
  return field;
}

/// `text` without the occurrences of `what` found left to right; the text
/// left behind is not searched again ("PAPARAM:RAM:" gives "PARAM:"). One
/// pass, so a long run of occurrences costs linear time.
std::string erase_all(std::string_view text, std::string_view what) {
  std::string out;
  out.reserve(text.size());
  std::size_t at = 0;
  for (std::size_t hit = text.find(what); hit != std::string_view::npos; hit = text.find(what, at)) {
    out.append(text.substr(at, hit - at));
    at = hit + what.size();
  }
  out.append(text.substr(at));
  return out;
}

/// `<p>` before every comma-separated item of `rest`.
std::string prefix_items(std::string_view prefix, std::string_view rest) {
  std::string out{prefix};
  for (const char c : rest) {
    out += c;
    if (c == ',') {
      out += prefix;
    }
  }
  return out;
}

/// A `#<p>#<rest>` field: a marker with prefix `<p>`, or the items of
/// `<rest>` each prefixed, after `?1` unless a marker precedes them.
void read_prefixed(const std::string& text, std::size_t second, std::vector<Field>& out) {
  const std::string prefix = text.substr(1, second - 1);
  const std::string rest = text.substr(second + 1);
  if (auto marker = parse_marker(rest); marker && marker->key.empty()) {
    marker->prefix = prefix;
    out.push_back(std::move(*marker));
    return;
  }
  if (out.empty() || !out.back().marker) {
    out.push_back(Field{.marker = true});
  }
  out.push_back(Field{.text = prefix_items(prefix, rest)});
}

/// One field after the first, with `PARAM:`, `##`, `#<p>#` and width
/// markers read.
void read_field(std::string_view raw, std::vector<Field>& out) {
  std::string text = erase_all(raw, "PARAM:");
  if (text.starts_with("##")) {
    out.push_back(Field{.text = text.substr(2)});
    return;
  }
  const std::size_t second = text.size() > 1 && text[0] == '#' ? text.find('#', 1) : std::string::npos;
  if (second != std::string::npos) {
    read_prefixed(text, second, out);
  } else if (auto marker = parse_marker(text)) {
    out.push_back(std::move(*marker));
  } else {
    out.push_back(Field{.text = std::move(text)});
  }
}

/// The fields after the first. A `#<p>#<rest>` field not preceded by a
/// marker gets `?1`.
std::vector<Field> read_fields(std::span<const std::string_view> raw) {
  std::vector<Field> out;
  for (const std::string_view r : raw) {
    read_field(r, out);
  }
  return out;
}

std::vector<std::string_view> split_items(std::string_view text) {
  std::vector<std::string_view> items;
  std::size_t at = 0;
  while (true) {
    const std::size_t comma = text.find(',', at);
    items.push_back(text.substr(at, comma == std::string_view::npos ? std::string_view::npos : comma - at));
    if (comma == std::string_view::npos) {
      return items;
    }
    at = comma + 1;
  }
}

/// The devices whose name list repeats the first field per item of the
/// third ("Vector elements").
bool repeats_first_field(std::string_view name) noexcept {
  static constexpr std::string_view kDevices[] = {"ymemristor", "ylin", "ydelay", "ytransline",
                                                  "ypgbr", "ypowergridbranch", "yacc"};
  return iequals(name, ".model") ||
         std::any_of(std::begin(kDevices), std::end(kDevices), [&](std::string_view d) { return name.starts_with(d); });
}

/// Builds the lines of one vector element, stopping once the text would
/// pass `max_bytes` (kMaxBusBytes, or less when the run's BusBudget has less
/// left), so hostile widths cannot exhaust memory. The field after each
/// marker is split into its items once, not once per name, so the work is
/// bounded by the text written.
class VectorWriter {
 public:
  VectorWriter(std::string_view prefix, const std::vector<Field>& fields, std::uint64_t max_bytes)
      : prefix_{prefix}, fields_{fields}, items_(fields.size()), max_bytes_{max_bytes} {
    for (std::size_t j = 1; j < fields.size(); ++j) {
      if (fields[j - 1].marker && !fields[j].marker) {
        items_[j] = split_items(fields[j].text);
      }
    }
  }

  /// The lines, or nullopt when they would exceed the byte limit.
  std::optional<std::vector<std::string>> write(std::span<const std::string_view> names) {
    std::vector<std::string> lines;
    for (std::size_t i = 0; i < names.size(); ++i) {
      std::string line = std::string{prefix_} + std::string{names[i]} + " ";
      if (!append_fields(line, i)) {
        return std::nullopt;
      }
      bytes_ += line.size();
      if (bytes_ > max_bytes_) {
        return std::nullopt;
      }
      lines.push_back(std::move(line));
    }
    return lines;
  }

 private:
  bool append_fields(std::string& line, std::size_t i) const {
    const Field* marker = nullptr;
    for (std::size_t j = 0; j < fields_.size(); ++j) {
      const Field& field = fields_[j];
      if (marker != nullptr) {
        if (!append_items(line, *marker, items_[j], i)) {
          return false;
        }
        marker = nullptr;
      } else if (field.marker) {
        marker = &field;
      } else {
        line += field.text;
        line += ' ';
      }
    }
    return true;
  }

  bool append_items(std::string& line, const Field& marker, std::span<const std::string_view> items,
                    std::size_t i) const {
    const std::uint64_t k = items.size();
    const std::uint64_t w = marker.width < 0 ? k : static_cast<std::uint64_t>(marker.width);
    line += marker.key;
    const std::uint64_t start = (w % k) * (i % k) % k;  // w*(i-1) mod k, with i from 0 here
    for (std::uint64_t l = 0; l < w; ++l) {
      line += marker.prefix;
      line += items[(start + l % k) % k];
      line += ' ';
      if (bytes_ + line.size() > max_bytes_) {
        return false;
      }
    }
    return true;
  }

  std::string_view prefix_;
  const std::vector<Field>& fields_;
  std::vector<std::vector<std::string_view>> items_;  // by field, for the fields after a marker
  std::uint64_t max_bytes_;
  std::size_t bytes_ = 0;

 public:
  /// The bytes of the lines written.
  [[nodiscard]] std::size_t bytes() const noexcept { return bytes_; }
};

// --- Renames, save lines, subcircuit lines --------------------------------

/// The rename of ` value=` for an element line's first letter (`g` cur,
/// `e` vol, `r` r, `c` c); empty for other letters.
std::string_view value_rename(char first) noexcept {
  switch (detail::ascii_lower(first)) {
    case 'g': return " cur=";
    case 'e': return " vol=";
    case 'r': return " r=";
    case 'c': return " c=";
    default: return {};
  }
}

/// The first `PERI` followed by optional spaces and `=` becomes `PJ=`.
void rename_peri(std::string& line) {
  for (std::size_t at = line.find("PERI"); at != std::string::npos; at = line.find("PERI", at + 1)) {
    std::size_t eq = at + 4;
    while (eq < line.size() && line[eq] == ' ') {
      ++eq;
    }
    if (eq < line.size() && line[eq] == '=') {
      line.replace(at, eq + 1 - at, "PJ=");
      return;
    }
  }
}

/// The first case-insensitive ` value=` of an element line becomes the
/// rename of its first letter; on a line starting with `D`, the first
/// `PERI *=` becomes `PJ=`.
std::string apply_renames(std::string line) {
  if (line.empty()) {
    return line;
  }
  if (const std::string_view to = value_rename(line.front()); !to.empty()) {
    const std::size_t at = ascii_lower(line).find(" value=");
    if (at != std::string::npos) {
      line.replace(at, 7, to);
    }
  }
  if (line.front() == 'D') {
    rename_peri(line);
  }
  return line;
}

bool is_width_marker(std::string_view field) {
  if (parse_marker(field)) {
    return true;
  }
  const std::size_t second = field.size() > 1 && field[0] == '#' ? field.find('#', 1) : std::string_view::npos;
  return second != std::string_view::npos && parse_marker(field.substr(second + 1)).has_value();
}

/// The fields after the first that are not width markers, each followed by
/// a space.
std::string save_operands(std::span<const std::string_view> fields) {
  std::string rest;
  for (std::size_t i = 1; i < fields.size(); ++i) {
    if (!is_width_marker(fields[i])) {
      rest += fields[i];
      rest += ' ';
    }
  }
  return rest;
}

/// `rest` without the spaces before or after a parenthesis.
std::string tighten_parens(std::string_view rest) {
  std::string tight;
  for (std::size_t i = 0; i < rest.size(); ++i) {
    const bool space = rest[i] == ' ';
    const bool next_paren = i + 1 < rest.size() && (rest[i + 1] == '(' || rest[i + 1] == ')');
    const bool prev_paren = !tight.empty() && (tight.back() == '(' || tight.back() == ')');
    if (space && (next_paren || prev_paren)) {
      continue;
    }
    tight += rest[i];
  }
  return tight;
}

/// `.save t(a,b)` with width markers as one lower-case line per name.
std::vector<std::string> expand_save(std::span<const std::string_view> fields) {
  const std::string tight = tighten_parens(save_operands(fields));
  const std::string head = ascii_lower(fields[0]);
  std::vector<std::string> lines;
  for (const std::string_view field : split_fields(tight)) {
    const std::size_t open = field.find('(');
    if (open == std::string_view::npos || field.back() != ')') {
      lines.push_back(ascii_lower(head + " " + std::string{field}));
      continue;
    }
    const std::string_view kind = field.substr(0, open);
    for (const std::string_view name : split_items(field.substr(open + 1, field.size() - open - 2))) {
      lines.push_back(ascii_lower(head + " " + std::string{kind} + "(" + std::string{name} + ")"));
    }
  }
  return lines;
}

/// The first ` m=<digits>` replaced by one space and every comma by a space.
std::string subcircuit_line(std::string line) {
  for (std::size_t at = line.find(" m="); at != std::string::npos; at = line.find(" m=", at + 1)) {
    std::size_t end = at + 3;
    while (end < line.size() && line[end] >= '0' && line[end] <= '9') {
      ++end;
    }
    if (end > at + 3) {
      line.replace(at, end - at, " ");
      break;
    }
  }
  std::replace(line.begin(), line.end(), ',', ' ');
  return line;
}

// --- The line loop -------------------------------------------------------

struct RawLine {
  std::string_view text;
  const NetlistChunk* chunk = nullptr;
};

std::vector<RawLine> split_chunks(std::span<const NetlistChunk> chunks) {
  std::vector<RawLine> lines;
  for (const NetlistChunk& chunk : chunks) {
    std::string_view text = chunk.text;
    while (!text.empty()) {
      const std::size_t lf = text.find('\n');
      lines.push_back({text.substr(0, lf), &chunk});
      if (lf == std::string_view::npos) {
        break;
      }
      text.remove_prefix(lf + 1);
    }
  }
  return lines;
}

ParamValues collect_params(std::span<const RawLine> lines) {
  ParamValues values;
  for (const RawLine& line : lines) {
    if (starts_with_ci(line.text, ".param")) {
      collect_param_pairs(line.text, values);
    }
  }
  return values;
}

class PostProcessor {
 public:
  PostProcessor(bool xschem_compat, std::vector<diag::Diagnostic>* warnings, ParamValues params, BusBudget* budget)
      : compat_{xschem_compat}, warnings_{warnings}, params_{std::move(params)}, budget_{budget} {}

  void feed(const RawLine& line) {
    if (line.chunk->user_code) {
      flush();
      user_code(line.text, false);
    } else if (in_block_) {
      in_block_ = !line.text.starts_with(kEndUserCode);
      user_code(line.text, true);
    } else if (line.text.starts_with(kBeginUserCode)) {
      flush();
      in_block_ = true;
      out_.push_back({std::string{line.text}, true});
    } else if (line.text.starts_with('+') && pending_) {
      pending_->text += " ";
      pending_->text += line.text.substr(1);
    } else {
      flush();
      pending_ = Pending{std::string{line.text}, line.chunk->spice_prefix};
    }
  }

  std::vector<ProcessedLine> finish() {
    flush();
    return std::move(out_);
  }

 private:
  struct Pending {
    std::string text;
    std::string_view prefix;
  };

  void user_code(std::string_view text, bool architecture) {
    std::string line{text};
    if (strips_trailing_m1(line, true, compat_)) {
      line = remove_trailing_m1(line);
    }
    out_.push_back({with_parametric_first_field(line, params_, budget_), architecture});
  }

  void flush() {
    if (!pending_) {
      return;
    }
    Pending line = std::move(*pending_);
    pending_.reset();
    process(line.text, line.prefix);
  }

  void process(std::string text, std::string_view prefix) {
    if (strips_trailing_m1(text, false, compat_)) {
      text = remove_trailing_m1(text);
    }
    const std::string_view head = first_field(text);
    if (head.starts_with('*')) {
      emit(std::move(text));
      return;
    }
    if (ascii_lower(head).find(".subckt") != std::string::npos) {
      emit(subcircuit_line(std::move(text)));
      return;
    }
    text = apply_renames(with_parametric_first_field(text, params_, budget_));
    const auto fields = split_fields(text);
    if (fields.empty()) {
      emit({});
      return;
    }
    if ((iequals(fields[0], ".save") || iequals(fields[0], ".print")) &&
        std::any_of(fields.begin() + 1, fields.end(), is_width_marker)) {
      for (std::string& line : expand_save(fields)) {
        emit(std::move(line));
      }
      return;
    }
    element(fields, prefix, text);
  }

  void element(std::span<const std::string_view> fields, std::string_view prefix, const std::string& text) {
    const std::vector<Field> rest = read_fields(fields.subspan(1));
    std::vector<std::string_view> names = split_items(fields[0]);
    if (repeats_first_field(fields[0])) {
      const std::size_t count = fields.size() > 2 ? split_items(fields[2]).size() : 0;
      names.assign(count, fields[0]);
    }
    if (names.empty()) {
      emit({});  // an empty name list is an empty line
      return;
    }
    // Only a vector element (several names) is bus expansion text (Q-SCHEM-41).
    BusBudget* budget = names.size() > 1 ? budget_ : nullptr;
    const std::uint64_t limit =
        budget == nullptr ? kMaxBusBytes : std::min<std::uint64_t>(kMaxBusBytes, budget->remaining());
    VectorWriter writer{prefix, rest, limit};
    auto lines = writer.write(names);
    if (!lines) {
      warn_size(text, limit < kMaxBusBytes);
      emit(std::string{prefix} + text);
      return;
    }
    if (budget != nullptr) {
      (void)budget->take(writer.bytes());
    }
    for (std::string& line : *lines) {
      emit(std::move(line));
    }
  }

  void warn_size(std::string_view text, bool over_budget) {
    if (warnings_ == nullptr) {
      return;
    }
    const std::string shown = text.size() > 80 ? std::string{text.substr(0, 80)} + "..." : std::string{text};
    const std::string limit = over_budget ? "the total of " + std::to_string(kMaxBusTotalBytes) +
                                                " bytes of expanded bus text per netlist run"
                                          : std::to_string(kMaxBusBytes) + " bytes";
    warnings_->push_back(
        diag::make(net_codes::kSizeLimit, "vector element " + shown + " exceeds " + limit + " when expanded; written unexpanded"));
  }

  void emit(std::string text) { out_.push_back({std::move(text), false}); }

  bool compat_;
  std::vector<diag::Diagnostic>* warnings_;
  ParamValues params_;
  BusBudget* budget_;
  bool in_block_ = false;
  std::optional<Pending> pending_;
  std::vector<ProcessedLine> out_;
};

}  // namespace

bool strips_trailing_m1(std::string_view line, bool in_user_code, bool xschem_compat) noexcept {
  if (xschem_compat) {
    return true;
  }
  if (in_user_code) {
    return false;
  }
  const std::string_view head = first_field(line);
  return !head.starts_with('*') && !head.starts_with('.');
}

std::string remove_trailing_m1(std::string_view line) {
  std::size_t i = line.size();
  const auto skip_spaces = [&] {
    while (i > 0 && line[i - 1] == ' ') {
      --i;
    }
  };
  const auto take = [&](auto predicate) {
    if (i > 0 && predicate(line[i - 1])) {
      --i;
      return true;
    }
    return false;
  };
  skip_spaces();
  if (!take([](char c) { return c == '1'; })) {
    return std::string{line};
  }
  skip_spaces();
  if (!take([](char c) { return c == '='; })) {
    return std::string{line};
  }
  skip_spaces();
  if (!take([](char c) { return c == 'm' || c == 'M'; }) || !take([](char c) { return c == ' '; })) {
    return std::string{line};
  }
  return std::string{line.substr(0, i)};
}

std::vector<ProcessedLine> postprocess(std::span<const NetlistChunk> chunks, bool xschem_compat,
                                       std::vector<diag::Diagnostic>* warnings, BusBudget* budget) {
  const std::vector<RawLine> lines = split_chunks(chunks);
  PostProcessor processor{xschem_compat, warnings, collect_params(lines), budget};
  for (const RawLine& line : lines) {
    processor.feed(line);
  }
  return processor.finish();
}

std::string finish_netlist(std::span<const NetlistChunk> chunks, bool xschem_compat,
                           std::vector<diag::Diagnostic>* warnings, BusBudget* budget) {
  const std::vector<ProcessedLine> lines = postprocess(chunks, xschem_compat, warnings, budget);
  return break_lines(lines);
}

}  // namespace hefesto::schem
