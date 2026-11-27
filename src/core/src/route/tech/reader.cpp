// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The reader of the technology `mzrouter` section (capability router-maze
// "mzrouter section grammar", "Style defaults and lookup"; design D6).
//
// One parser per statement. Each parses its line into a local value and
// reports Magic's messages in Magic's order; the value is added to the
// style only when the statement reported nothing, in both magic-compat
// settings (Q-LEGACY-31, Q-LEGACY-39: Magic registers a route type before
// checking the rest of its line, and keeps the valid parts of `notactive`,
// `spacing` and `search` lines). A bare `style` line reports an error and
// the lines up to the next `style` are skipped (Magic copies a NULL name).
// The `search` penalty is range-checked (Q-LEGACY-47) and stored as
// Magic's mantissa floor(p * 2) with exponent 1 (Q-LEGACY-32).
//
// Numbers follow StrIsInt and atoi, except that a value outside the int
// range is reported as non-numeric (atoi's behaviour there is undefined).
// The penalty follows sscanf("%f"): a float, read from the longest numeric
// prefix.
//
// Ported from Magic VLSI 8.3.694 mzrouter/mzTech.c (MZTechLine,
// mzTechStyle, mzTechLayer, mzTechContact, mzTechNotActive,
// mzTechSpacing, mzTechSearch, mzTechWidth), Copyright (C) 1988, 1990
// Michael H. Arnold and the Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: MZTECH.

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/route/tech/styles.hpp"
#include "hefesto/tech/technology.hpp"
#include "tech/build_state.hpp"

namespace hefesto::route {

namespace {

using Tokens = std::vector<std::string>;

constexpr std::string_view kMissingStyle = "ROUTE-TECH-001";
constexpr std::string_view kBareStyle = "ROUTE-TECH-002";
constexpr std::string_view kUnknownKeyword = "ROUTE-TECH-003";
constexpr std::string_view kLayerError = "ROUTE-TECH-004";
constexpr std::string_view kContactError = "ROUTE-TECH-005";
constexpr std::string_view kNotActiveError = "ROUTE-TECH-006";
constexpr std::string_view kSpacingError = "ROUTE-TECH-007";
constexpr std::string_view kSearchError = "ROUTE-TECH-008";
constexpr std::string_view kPenaltyError = "ROUTE-TECH-009";
constexpr std::string_view kPenaltyRange = "ROUTE-TECH-010";
constexpr std::string_view kWidthError = "ROUTE-TECH-011";

// StrIsInt: an optional sign, then only digits (none at all included).
bool str_is_int(std::string_view text) noexcept {
  if (!text.empty() && (text.front() == '-' || text.front() == '+')) {
    text.remove_prefix(1);
  }
  for (const char c : text) {
    if (std::isdigit(static_cast<unsigned char>(c)) == 0) {
      return false;
    }
  }
  return true;
}

// StrIsInt then atoi; nullopt when the text is not an int or does not fit
// in one.
std::optional<int> magic_int(std::string_view text) {
  if (!str_is_int(text)) {
    return std::nullopt;
  }
  const bool negative = !text.empty() && text.front() == '-';
  if (!text.empty() && (text.front() == '-' || text.front() == '+')) {
    text.remove_prefix(1);
  }
  std::int64_t value = 0;
  for (const char c : text) {
    value = value * 10 + (c - '0');
    if (value > std::int64_t{std::numeric_limits<int>::max()} + 1) {
      return std::nullopt;
    }
  }
  value = negative ? -value : value;
  if (value > std::numeric_limits<int>::max()) {
    return std::nullopt;
  }
  return static_cast<int>(value);
}

// "%.20s".
std::string first20(std::string_view text) {
  return std::string{text.substr(0, 20)};
}

std::string quoted(std::string_view text) {
  return "\"" + std::string{text} + "\"";
}

// LookupStruct(name, {"subcell"}): a case-insensitive prefix of "subcell".
bool is_subcell(std::string_view name) noexcept {
  constexpr std::string_view kSubcell = "subcell";
  if (name.empty() || name.size() > kSubcell.size()) {
    return false;
  }
  for (std::size_t i = 0; i < name.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(name[i])) != kSubcell[i]) {
      return false;
    }
  }
  return true;
}

// Stamps diagnostics located on a line with the section `mzrouter`, so the
// magic rendering prints TechPrintLine's context (as lef/tech does).
class SectionStamp final : public diag::DiagnosticSink {
 public:
  explicit SectionStamp(diag::DiagnosticSink& inner) : inner_(inner) {}

  void emit(const diag::Diagnostic& diagnostic) override {
    const auto* where = std::get_if<diag::SourceLocation>(&diagnostic.location);
    if (where == nullptr || where->line == 0 || diagnostic.related.has_value()) {
      inner_.emit(diagnostic);
      return;
    }
    diag::Diagnostic stamped = diagnostic;
    stamped.related = diag::ObjectRef{diag::ObjectKind::section, "mzrouter"};
    inner_.emit(stamped);
  }

 private:
  diag::DiagnosticSink& inner_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

// The search parameters of one `search` line.
struct SearchValues {
  std::int64_t rate = 0;
  std::int64_t width = 0;
  std::int64_t mantissa = 0;
};

class SectionReader {
 public:
  SectionReader(const tech::Technology& tech, diag::DiagnosticSink& sink)
      : tech_(tech), stamp_(sink), reporter_(stamp_) {}

  // MZTechLine.
  void line(const tech::RawLine& raw) {
    const Tokens& t = raw.tokens;
    if (t.empty()) {
      return;
    }
    reporter_.set_location(diag::SourceLocation{raw.file, raw.line}, "mzrouter");
    if (t[0] == "style") {
      style_line(t);
    } else if (skipping_) {
      return;  // after a bare `style` line (Q-LEGACY-39)
    } else if (styles_.styles.empty()) {
      error(kMissingStyle, "Missing style line.");
    } else {
      statement(t);
    }
  }

  MazeStyles take() { return std::move(styles_); }

 private:
  void statement(const Tokens& t) {
    const std::string& keyword = t[0];
    if (keyword == "layer") {
      layer_line(t);
    } else if (keyword == "contact") {
      contact_line(t);
    } else if (keyword == "notactive") {
      notactive_line(t);
    } else if (keyword == "spacing") {
      spacing_line(t);
    } else if (keyword == "search") {
      search_line(t);
    } else if (keyword == "width") {
      width_line(t);
    } else {
      error(kUnknownKeyword, "Unrecognized keyword: " + quoted(keyword));
    }
  }

  void error(std::string_view code, std::string text) { reporter_.emit(code, std::move(text)); }

  // TxError: printed without the technology line context.
  void tx_error(std::string_view code, std::string text) { stamp_.emit(diag::make(code, std::move(text))); }

  [[nodiscard]] MazeStyle& style() { return styles_.styles.back(); }

  // DBTechNoisyNameType: a type, or nullopt after its message.
  std::optional<tech::TileType> noisy_type(const std::string& name) {
    const int type = tech::noisy_name_type(tech_.data(), name, &reporter_, tech::Noise::error);
    if (type < 0) {
      return std::nullopt;
    }
    return static_cast<tech::TileType>(type);
  }

  // mzFindRouteType: the most recently defined route type of `type` (Magic
  // prepends to its list).
  [[nodiscard]] std::optional<RouteTypeRef> find_route_type(tech::TileType type) {
    const MazeStyle& s = style();
    for (auto it = s.types.rbegin(); it != s.types.rend(); ++it) {
      if (s.route_type(*it).type == type) {
        return *it;
      }
    }
    return std::nullopt;
  }

  // mzFindRouteLayer, most recent first.
  [[nodiscard]] std::optional<std::size_t> find_route_layer(tech::TileType type) {
    const auto& layers = style().layers;
    for (std::size_t i = layers.size(); i-- > 0;) {
      if (layers[i].route.type == type) {
        return i;
      }
    }
    return std::nullopt;
  }

  // The route type named by a `notactive`, `spacing` or `width` line, or
  // nullopt after Magic's messages.
  std::optional<RouteTypeRef> named_route_type(const std::string& name, std::string_view code) {
    const std::optional<tech::TileType> type = noisy_type(name);
    if (!type) {
      return std::nullopt;
    }
    const std::optional<RouteTypeRef> found = find_route_type(*type);
    if (!found) {
      error(code, "Unrecognized route type: " + quoted(first20(name)));
    }
    return found;
  }

  // mzTechStyle (and mzStyleEnd: Hefesto's lists are already in file order).
  void style_line(const Tokens& t) {
    if (t.size() < 2) {
      error(kBareStyle, "\"style\" lines need a style name");
      skipping_ = true;
      return;
    }
    skipping_ = false;
    MazeStyle fresh;  // mzSetParmDefaults
    fresh.name = t[1];
    styles_.styles.push_back(std::move(fresh));
  }

  // One cost of a `layer` line.
  std::optional<int> layer_cost(const std::string& text, std::string_view name) {
    const std::optional<int> value = magic_int(text);
    if (!value) {
      error(kLayerError, "Cost arguments to \"layer\" line must be numeric");
      return std::nullopt;
    }
    if (*value <= 0) {
      error(kLayerError, std::string{name} + " must be > 0");
      return std::nullopt;
    }
    return value;
  }

  // The plane checks of mzTechLayer.
  bool layer_plane_ok(tech::TileType type, const std::string& name) {
    const int plane = tech_.data().home(type);
    for (const RouteLayer& other : style().layers) {
      if (tech_.data().home(other.route.type) == plane) {
        error(kLayerError, "Attempt to define two route layers on same plane:  " +
                               std::string{tech_.type_name(type)} + " and " +
                               std::string{tech_.type_name(other.route.type)});
        return false;
      }
    }
    if (plane < 0) {
      error(kLayerError, "Type " + quoted(name) + " appears on more than one plane");
      return false;
    }
    return true;
  }

  // mzTechLayer.
  void layer_line(const Tokens& t) {
    if (t.size() < 4 || t.size() > 7) {
      error(kLayerError,
            "Malformed \"layer\" line, should be:  \"layer name hCost vCost [jogCost [hintCost [overCost]]]\"");
      return;
    }
    const std::optional<tech::TileType> type = noisy_type(t[1]);
    if (!type || !layer_plane_ok(*type, t[1])) {
      return;
    }
    static constexpr std::array<std::string_view, 5> kNames = {"hCost", "vCost", "jogCost", "hintCost", "overCost"};
    std::array<int, 5> costs = {1, 1, 1, 1, 1};
    for (std::size_t i = 2; i < t.size(); ++i) {
      const std::optional<int> cost = layer_cost(t[i], kNames[i - 2]);
      if (!cost) {
        return;
      }
      costs[i - 2] = *cost;
    }
    RouteLayer layer;
    layer.route.type = *type;
    layer.h_cost = costs[0];
    layer.v_cost = costs[1];
    layer.jog_cost = costs[2];
    layer.hint_cost = costs[3];
    layer.over_cost = costs[4];
    MazeStyle& s = style();
    s.layers.push_back(std::move(layer));
    s.types.push_back(RouteTypeRef{RouteTypeRef::Kind::layer, s.layers.size() - 1});
  }

  // One route layer of a `contact` line.
  std::optional<std::size_t> contact_layer(const std::string& name) {
    const std::optional<tech::TileType> type = noisy_type(name);
    if (!type) {
      return std::nullopt;
    }
    const std::optional<std::size_t> layer = find_route_layer(*type);
    if (!layer) {
      error(kContactError, "route layer must be declared before used in contact.");
    }
    return layer;
  }

  // mzTechContact.
  void contact_line(const Tokens& t) {
    if (t.size() != 5) {
      error(kContactError, "Malformed \"contact\" line, should be:  contact layer layer1 layer2 cost");
      return;
    }
    const std::optional<tech::TileType> type = noisy_type(t[1]);
    if (!type) {
      return;
    }
    const std::optional<std::size_t> layer1 = contact_layer(t[2]);
    const std::optional<std::size_t> layer2 = layer1 ? contact_layer(t[3]) : std::nullopt;
    if (!layer2) {
      return;
    }
    const std::optional<int> cost = magic_int(t[4]);
    if (!cost) {
      error(kContactError, "Cost argument to \"contact\" line must be numeric");
      return;
    }
    if (*cost <= 0) {
      error(kContactError, "Cost must be > 0");
      return;
    }
    RouteContact contact;
    contact.route.type = *type;
    contact.layer1 = *layer1;
    contact.layer2 = *layer2;
    contact.cost = *cost;
    MazeStyle& s = style();
    s.contacts.push_back(std::move(contact));
    s.types.push_back(RouteTypeRef{RouteTypeRef::Kind::contact, s.contacts.size() - 1});
  }

  // mzTechNotActive: every unknown entry is reported, as in Magic; the
  // line then marks nothing.
  void notactive_line(const Tokens& t) {
    if (t.size() < 2) {
      error(kNotActiveError, "Bad form on mzroute notactive.");
      error(kNotActiveError, "Usage: notactive routeType1 ... [routeTypen]");
      return;
    }
    std::vector<RouteTypeRef> found;
    bool failed = false;
    for (std::size_t i = 1; i < t.size(); ++i) {
      const std::optional<RouteTypeRef> ref = named_route_type(t[i], kNotActiveError);
      failed = failed || !ref;
      if (ref) {
        found.push_back(*ref);
      }
    }
    if (failed) {
      return;
    }
    for (const RouteTypeRef ref : found) {
      style().route_type(ref).active = false;
    }
  }

  // The value of one spacing pair: a non-negative int or NIL (-1); nullopt
  // after Magic's two messages.
  std::optional<int> spacing_value(const std::string& text) {
    const std::optional<int> value = magic_int(text);
    std::string bad;
    if (value && *value >= 0) {
      return value;
    }
    if (value) {
      bad = "Bad spacing value: " + std::to_string(*value);
    } else if (text == "NIL") {
      return -1;
    } else {
      bad = "Bad spacing value: " + text;
    }
    error(kSpacingError, std::move(bad));
    error(kSpacingError, "Valid values are nonnegative integers and \"NIL\".");
    return std::nullopt;
  }

  // The second word of a spacing pair: a type (DBTechNameType) or SUBCELL.
  std::optional<SpacingStatement> spacing_target(RouteTypeRef route, const std::string& name) {
    const int type = tech::name_type(tech_.data(), name);
    if (type >= 0) {
      return SpacingStatement{route, static_cast<tech::TileType>(type), false, 0};
    }
    if (is_subcell(name)) {
      return SpacingStatement{route, tech::kSpace, true, 0};
    }
    error(kSpacingError, "Unrecognized layer (type): " + quoted(first20(name)));
    return std::nullopt;
  }

  // mzTechSpacing: an unknown type is reported and the next pair read, a
  // bad value ends the line, as in Magic; the line then defines nothing.
  void spacing_line(const Tokens& t) {
    if (t.size() < 4 || t.size() % 2 != 0) {
      error(kSpacingError, "Bad form on mzroute spacing.");
      error(kSpacingError, "Usage: spacing routeType type1 spacing1 ... [typen spacingn]");
      return;
    }
    const std::optional<RouteTypeRef> route = named_route_type(t[1], kSpacingError);
    if (!route) {
      return;
    }
    std::vector<SpacingStatement> entries;
    bool failed = false;
    for (std::size_t i = 2; i < t.size(); i += 2) {
      std::optional<SpacingStatement> entry = spacing_target(*route, t[i]);
      if (!entry) {
        failed = true;
        continue;
      }
      const std::optional<int> value = spacing_value(t[i + 1]);
      if (!value) {
        return;
      }
      entry->value = *value;
      entries.push_back(*entry);
    }
    if (!failed) {
      style().spacings.insert(style().spacings.end(), entries.begin(), entries.end());
    }
  }

  // The rate or the window width of a `search` line (positive int).
  std::optional<int> search_count(const std::string& text, std::string_view what, std::string_view rule) {
    const std::optional<int> value = magic_int(text);
    if (value && *value > 0) {
      return value;
    }
    error(kSearchError, "Bad " + std::string{what} + ": " + (value ? std::to_string(*value) : text));
    error(kSearchError, std::string{rule});
    return std::nullopt;
  }

  // The penalty of a `search` line: Magic's mantissa (int)(p * 2), or
  // nullopt after the messages (TxError in Magic).
  std::optional<std::int64_t> search_penalty(const std::string& text) {
    const char* begin = text.c_str();
    char* end = nullptr;
    const float value = std::strtof(begin, &end);  // sscanf("%f")
    std::string bad;
    if (end == begin) {
      bad = "Bad penalty value: " + text;
    } else if (value < 0) {
      std::array<char, 64> buffer{};
      std::snprintf(buffer.data(), buffer.size(), "%f", static_cast<double>(value));
      bad = "Bad penalty value: " + std::string{buffer.data()};
    } else {
      const float scaled = value * static_cast<float>(1 << Penalty{}.exponent);
      if (scaled < 2147483648.0F) {  // also false for NaN (Q-LEGACY-47)
        return static_cast<std::int64_t>(static_cast<int>(scaled));
      }
      tx_error(kPenaltyRange, "Penalty value out of range: " + text);
      return std::nullopt;
    }
    tx_error(kPenaltyError, std::move(bad));
    tx_error(kPenaltyError, "Penalty must be non-negative decimal.");
    return std::nullopt;
  }

  // mzTechSearch: the three values are checked and reported independently,
  // as in Magic; the line sets them only when all three are valid.
  void search_line(const Tokens& t) {
    if (t.size() != 4) {
      error(kSearchError, "Bad form on search.");
      error(kSearchError, "Usage: search <rate> <width> <penalty>");
      return;
    }
    const std::optional<int> rate = search_count(t[1], "rate", "Rate must be a positive integer.");
    const std::optional<int> width = search_count(t[2], "width", "Width must be a positive integer.");
    const std::optional<std::int64_t> mantissa = search_penalty(t[3]);
    if (!rate || !width || !mantissa) {
      return;
    }
    MazeStyle& s = style();
    s.window_rate = *rate;
    s.window_width = *width;
    s.penalty.mantissa = *mantissa;
  }

  // A width or minimum length of a `width` line.
  std::optional<int> width_value(const std::string& text, std::string_view what, std::string_view rule) {
    const std::optional<int> value = magic_int(text);
    if (value && *value > 0) {
      return value;
    }
    error(kWidthError, "Bad " + std::string{what} + ": " + (value ? std::to_string(*value) : text));
    error(kWidthError, std::string{rule});
    return std::nullopt;
  }

  // mzTechWidth: the statement is kept for derive_after_load
  // (Q-LEGACY-33).
  void width_line(const Tokens& t) {
    if (t.size() != 3 && t.size() != 4) {
      error(kWidthError, "Bad form on mzroute width.");
      error(kWidthError, "Usage: width <routeType> <width> [<length>]");
      return;
    }
    const std::optional<RouteTypeRef> route = named_route_type(t[1], kWidthError);
    if (!route) {
      return;
    }
    const std::optional<int> width = width_value(t[2], "width", "Width must be a positive integer.");
    if (!width) {
      return;
    }
    std::optional<int> length = width;
    if (t.size() == 4) {
      length = width_value(t[3], "minimum length", "Length must be a positive integer.");
    }
    if (length) {
      style().widths.push_back(WidthStatement{*route, *width, *length});
    }
  }

  const tech::Technology& tech_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  SectionStamp stamp_;
  tech::Reporter reporter_;
  MazeStyles styles_;
  bool skipping_ = false;
};

}  // namespace

MazeStyles read_mzrouter_section(const tech::RawSection* section, const tech::Technology& tech,
                                 diag::DiagnosticSink& sink) {
  if (section == nullptr) {
    return MazeStyles{};
  }
  SectionReader reader(tech, sink);
  for (const tech::RawLine& raw : section->lines) {
    reader.line(raw);
  }
  return reader.take();
}

}  // namespace hefesto::route
