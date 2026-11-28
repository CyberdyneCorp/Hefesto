// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `extract` section reader (see style_reader.hpp): ExtTechLine's
// keyword table as a dispatch table (design D7: keyword, argument range,
// usage, when the line applies, handler), the style selection of
// ExtLoadStyle and the end-of-style pass. The handlers of the rule
// keywords live in one file per group (rule_context.hpp).
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtTech.c keyTable,
// ExtTechLine and extTechFinalStyle, Copyright (C) 1985, 1990 Regents of
// the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: EXTTECH (openspec/changes/m7-extraction/design.md D7), on the
// pass M6's EXTSUB and M10's TECH (`lambda`, `units`, `planeorder`,
// `noplaneordering`, `height`, finish_heights) wrote.

#include "style_reader.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>

#include "hefesto/extract/tech/height_view.hpp"
#include "rule_context.hpp"

namespace hefesto::extract::detail {

namespace {

constexpr std::string_view kMalformed = "EXTRACT-STYLE-001";
constexpr std::string_view kIllegalDevice = "EXTRACT-STYLE-002";
constexpr std::string_view kBadHeight = "EXTRACT-STYLE-003";
constexpr std::string_view kBadUnits = "EXTRACT-STYLE-004";
constexpr std::string_view kBadPlaneOrder = "EXTRACT-STYLE-005";
constexpr std::string_view kPlaneOrderFallback = "EXTRACT-STYLE-006";
constexpr std::string_view kIllegalKeyword = "EXTRACT-STYLE-020";

enum class Key : std::uint8_t {
  areacap, connect, contact, cscale, defaultareacap, defaultoverlap, defaultperimeter, defaultsideoverlap,
  defaultsidewall, device, devresist, disconnect, fet, fetresist, fringeshieldhalo, height, antenna, model,
  tiedown, lambda, overlap, perimc, planeorder, noplaneordering, resist, rscale, sidehalo, sideoverlap, sidewall,
  step, style, substrate, units, variants,
};

class StyleReader;
using Handler = void (*)(StyleReader&, const Tokens&);

// When ExtTechLine applies a keyword's line.
enum class Scope : std::uint8_t {
  always,    // every line of the section (`style`)
  loading,   // in the selected style, also while a `variants` line suspends it (`variants`)
  selected,  // in the selected style, unless a `variants` line suspends it (every other keyword)
};

// One entry of the dispatch table (design D7): keyTable's name, argument
// range and usage, when the line applies, and its handler.
struct Keyword {
  std::string_view name;
  Key key;
  std::size_t min_args;
  std::size_t max_args;
  std::string_view usage;
  Scope scope;
  Handler handler;
};

constexpr std::size_t kKeywordCount = 34;

enum class Dev : std::uint8_t { mosfet, bjt, cap, caprev, res, diode, pdiode, ndiode, subckt, rsubckt, msubckt,
                               csubckt, dsubckt, veriloga };

struct DeviceClass {
  std::string_view name;
  Dev dev;
  std::size_t min_args;  // arguments after the keyword
};

// devTable of ExtTech.c, in its order.
constexpr std::array<DeviceClass, 14> kDevices{{
    {"mosfet", Dev::mosfet, 5},       {"bjt", Dev::bjt, 5},
    {"capacitor", Dev::cap, 4},       {"capreverse", Dev::caprev, 4},
    {"resistor", Dev::res, 4},        {"diode", Dev::diode, 4},
    {"pdiode", Dev::pdiode, 4},       {"ndiode", Dev::ndiode, 4},
    {"subcircuit", Dev::subckt, 3},   {"rsubcircuit", Dev::rsubckt, 4},
    {"msubcircuit", Dev::msubckt, 3}, {"csubcircuit", Dev::csubckt, 4},
    {"dsubcircuit", Dev::dsubckt, 4}, {"veriloga", Dev::veriloga, 3},
}};

constexpr char ascii_lower(char c) noexcept {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// LookupStruct over a table of lower-case names: an exact match wins, a
// unique prefix matches, an ambiguous prefix matches nothing.
template <typename T, std::size_t N>
const T* lookup(const std::array<T, N>& table, std::string_view word) {
  const T* match = nullptr;
  bool ambiguous = false;
  for (const T& entry : table) {
    std::size_t i = 0;
    while (i < word.size() && i < entry.name.size() && ascii_lower(word[i]) == entry.name[i]) {
      ++i;
    }
    if (i != word.size()) {
      continue;
    }
    if (word.size() == entry.name.size()) {
      return &entry;
    }
    ambiguous = ambiguous || match != nullptr;
    match = &entry;
  }
  return ambiguous ? nullptr : match;
}

// The comma-separated elements of a `style ... variants` list, as
// ExtTechLine walks them: an empty element ends the list.
std::vector<std::string> variant_list(std::string_view list) {
  std::vector<std::string> out;
  while (!list.empty()) {
    const std::size_t comma = list.find(',');
    out.emplace_back(list.substr(0, comma));
    if (comma == std::string_view::npos) {
      break;
    }
    list.remove_prefix(comma + 1);
  }
  return out;
}

bool ends_with(std::string_view name, std::string_view suffix) noexcept {
  return suffix.size() <= name.size() && name.substr(name.size() - suffix.size()) == suffix;
}

// A `variants` line's elements, with the white space before each comma
// removed.
std::vector<std::string> variants_line(std::string_view list) {
  std::vector<std::string> out = variant_list(list);
  for (std::size_t i = 0; i + 1 < out.size(); ++i) {
    while (!out[i].empty() && (out[i].back() == ' ' || out[i].back() == '\t')) {
      out[i].pop_back();
    }
  }
  return out;
}

enum class Status : std::uint8_t { not_loaded, pending, suspended, loaded };

// ExtTechLine for one style.
class StyleReader {
 public:
  StyleReader(const tech::Technology& tech, diag::DiagnosticSink& sink, const ReadOptions& options)
      : tech_(tech), sink_(sink), target_(options.style), report_(options.report),
        ctx_{tech_, style_, mentioned_, types1_, heights_.plane_order, seen_plane_order_,
             [this](std::string_view code, std::string text, Print print) {
               report(code, std::move(text), Subset::all, print);
             }} {
    heights_.plane_order.assign(tech.plane_count(), kNoPlaneOrder);
    init_style(style_, tech, options.magic_compat);
  }
  StyleReader(const StyleReader&) = delete;
  StyleReader& operator=(const StyleReader&) = delete;

  void line(const tech::RawLine& line);
  [[nodiscard]] std::optional<StyleTables> finish();

  /// The keyword `word` names (LookupStruct), nullptr when unknown or
  /// ambiguous.
  [[nodiscard]] static const Keyword* find(std::string_view word) { return lookup(keywords(), word); }

 private:
  [[nodiscard]] static const std::array<Keyword, kKeywordCount>& keywords();
  [[nodiscard]] static std::string keyword_list();
  [[nodiscard]] bool applies(Scope scope) const noexcept;

  // exts_planeOrderStatus.
  enum class Order : std::uint8_t { needed, seen, not_needed };

  void height(const Tokens& t);
  void units(const Tokens& t);
  void plane_order(const Tokens& t);
  void no_plane_order();
  void finish_heights();
  void mark_height_sources();
  void check_plane_order();
  void default_heights();
  void mention_connected();
  void finish_style();
  void style_line(const Tokens& t);
  void first_style(const Tokens& t);
  void wanted_style(const Tokens& t);
  void variants(const Tokens& t);
  [[nodiscard]] bool noisy(Key key) const noexcept;
  void mention_first(Key key, const Tokens& t);
  void mention_rest(Key key, const Tokens& t);
  void mention_defaults(Key key, const Tokens& t);
  void device(const Tokens& t);
  void device_mentions(Dev dev, Tokens t);
  void subcircuit_mentions(Tokens t);
  void substrate(const Tokens& t);
  [[nodiscard]] tech::TypeMask parse(std::string_view text, bool report);
  void mention(const Tokens& t, std::size_t index);
  void malformed(const Keyword& kw);
  void report(std::string_view code, std::string text, Subset subset = Subset::types,
              Print print = Print::tech_error);

  const tech::Technology& tech_;
  diag::DiagnosticSink& sink_;
  std::optional<std::string_view> target_;
  Subset report_;
  const tech::RawLine* where_ = nullptr;
  Status status_ = Status::not_loaded;
  bool started_ = false;  // ExtCurStyle != NULL
  bool seen_plane_order_ = false;
  Order order_ = Order::needed;
  std::string name_;             // the style's name
  tech::TypeMask mentioned_;     // allExtractTypes
  tech::TypeMask types1_;        // the first switch's types1
  tech::TypeMask height_lines_;  // types a valid `height` line set
  StyleHeights heights_;         // the height subset until finish_style
  ExtStyle style_;
  RuleContext ctx_;  // what the handlers see
};

// keyTable of ExtTech.c, in its order, with each keyword's handler (design
// D7). A line runs its handler after the first switch has read its types
// (mention_first, mention_rest).
const std::array<Keyword, kKeywordCount>& StyleReader::keywords() {
  using T = const Tokens&;
  using R = StyleReader&;
  constexpr Scope sel = Scope::selected;
  static constexpr std::array<Keyword, kKeywordCount> kTable{{
      {"areacap", Key::areacap, 3, 3, "types capacitance", sel, [](R r, T t) { areacap_rule(r.ctx_, t); }},
      {"connect", Key::connect, 3, 3, "types1 types1", sel, [](R r, T t) { connect_rule(r.ctx_, t, true); }},
      {"contact", Key::contact, 3, 6, "type resistance", sel, [](R r, T t) { contact_rule(r.ctx_, t); }},
      {"cscale", Key::cscale, 2, 2, "capacitance-scalefactor", sel, [](R r, T t) { cscale_rule(r.ctx_, t); }},
      {"defaultareacap", Key::defaultareacap, 4, 6, "types plane capacitance", sel,
       [](R r, T t) { default_areacap_rule(r.ctx_, t); }},
      {"defaultoverlap", Key::defaultoverlap, 6, 6, "types plane otertypes otherplane capacitance", sel,
       [](R r, T t) { default_overlap_rule(r.ctx_, t); }},
      {"defaultperimeter", Key::defaultperimeter, 4, 6, "types plane capacitance", sel,
       [](R r, T t) { default_perimeter_rule(r.ctx_, t); }},
      {"defaultsideoverlap", Key::defaultsideoverlap, 6, 6, "types plane othertypes otherplane capacitance", sel,
       [](R r, T t) { default_sideoverlap_rule(r.ctx_, t); }},
      {"defaultsidewall", Key::defaultsidewall, 4, 5, "types plane capacitance [offset]", sel,
       [](R r, T t) { default_sidewall_rule(r.ctx_, t); }},
      {"device", Key::device, 4, 10, "dev-type dev-name types options...", sel, [](R r, T t) { r.device(t); }},
      {"devresist", Key::devresist, 4, 4, "type region ohms-per-square", sel,
       [](R r, T t) { devresist_rule(r.ctx_, t); }},
      {"disconnect", Key::disconnect, 3, 3, "types1 types1", sel, [](R r, T t) { connect_rule(r.ctx_, t, false); }},
      {"fet", Key::fet, 8, 9,
       "types terminal-types min-#-terminals name [subs-types] subs-node gscap gate-chan-cap", sel,
       [](R r, T t) { fet_rule(r.ctx_, t); }},
      {"fetresist", Key::fetresist, 4, 4, "type region ohms-per-square", sel,
       [](R r, T t) { devresist_rule(r.ctx_, t); }},
      // Deprecated: read and ignored.
      {"fringeshieldhalo", Key::fringeshieldhalo, 2, 2, "distance", sel, [](R, T) {}},
      {"height", Key::height, 4, 4, "type height-above-subtrate thickness", sel, [](R r, T t) { r.height(t); }},
      {"antenna", Key::antenna, 4, 6, "type [calc-type] [antenna-ratio-proportional] antenna-ratio-const", sel,
       [](R r, T t) { antenna_rule(r.ctx_, t); }},
      {"model", Key::model, 2, 3, "partial-cumulative [area-sidewall]", sel, [](R r, T t) { model_rule(r.ctx_, t); }},
      {"tiedown", Key::tiedown, 2, 2, "types", sel, [](R r, T t) { tiedown_rule(r.ctx_, t); }},
      {"lambda", Key::lambda, 2, 2, "units-per-lambda", sel,
       [](R r, T t) { r.heights_.units_per_lambda = static_cast<float>(std::atof(t[1].c_str())); }},
      {"overlap", Key::overlap, 4, 5, "toptypes bottomtypes capacitance [shieldtypes]", sel,
       [](R r, T t) { overlap_rule(r.ctx_, t); }},
      {"perimc", Key::perimc, 4, 4, "intypes outtypes capacitance", sel, [](R r, T t) { perimc_rule(r.ctx_, t); }},
      {"planeorder", Key::planeorder, 3, 3, "plane index", sel, [](R r, T t) { r.plane_order(t); }},
      {"noplaneordering", Key::noplaneordering, 1, 1, "(no arguments needed)", sel,
       [](R r, T) { r.no_plane_order(); }},
      {"resist", Key::resist, 3, 4, "types resistance", sel, [](R r, T t) { resist_rule(r.ctx_, t); }},
      {"rscale", Key::rscale, 2, 2, "resistance-scalefactor", sel, [](R r, T t) { rscale_rule(r.ctx_, t); }},
      {"sidehalo", Key::sidehalo, 2, 2, "distance", sel, [](R r, T t) { sidehalo_rule(r.ctx_, t); }},
      {"sideoverlap", Key::sideoverlap, 5, 6, "intypes outtypes ovtypes capacitance [shieldtypes]", sel,
       [](R r, T t) { sideoverlap_rule(r.ctx_, t); }},
      {"sidewall", Key::sidewall, 6, 7, "intypes outtypes neartypes fartypes capacitance [offset]", sel,
       [](R r, T t) { sidewall_rule(r.ctx_, t); }},
      {"step", Key::step, 2, 2, "size", sel, [](R r, T t) { (void)step_rule(r.ctx_, t); }},
      {"style", Key::style, 2, 4, "stylename", Scope::always, [](R r, T t) { r.style_line(t); }},
      {"substrate", Key::substrate, 3, 5, "types plane [subs-node]", sel, [](R r, T t) { r.substrate(t); }},
      {"units", Key::units, 2, 2, "lambda|microns", sel, [](R r, T t) { r.units(t); }},
      {"variants", Key::variants, 2, 2, "style,...", Scope::loading, [](R r, T t) { r.variants(t); }},
  }};
  return kTable;
}

std::string StyleReader::keyword_list() {
  std::string text;
  for (const Keyword& k : keywords()) {
    text += " " + std::string{k.name};
  }
  return text;
}

// A view reports its own subset; the full read reports everything, a
// TechError with Magic's file, line and section prefix.
void StyleReader::report(std::string_view code, std::string text, Subset subset, Print print) {
  if (subset != report_ && report_ != Subset::all) {
    return;
  }
  diag::Location where;
  if (where_ != nullptr && !where_->file.empty()) {
    where = diag::SourceLocation{where_->file, where_->line};
  }
  diag::Diagnostic d = diag::make(code, std::move(text), where);
  if (report_ == Subset::all && print == Print::tech_error) {
    d.related = diag::ObjectRef{diag::ObjectKind::section, "extract"};
  }
  sink_.emit(d);
}

void StyleReader::malformed(const Keyword& kw) {
  report(kMalformed, "Malformed line for keyword " + std::string{kw.name} + ".  Correct usage:\n\t" +
                         std::string{kw.name} + " " + std::string{kw.usage});
}

tech::TypeMask StyleReader::parse(std::string_view text, bool report_error) {
  auto parsed = tech_.parse_type_list(text);
  if (!parsed.ok()) {
    if (report_error && report_ == Subset::all) {
      report(parsed.error().code, parsed.error().text, Subset::all);
    } else if (report_error) {
      sink_.emit(parsed.error());
    }
    return {};
  }
  return parsed.value();
}

void StyleReader::mention(const Tokens& t, std::size_t index) {
  if (index < t.size()) {
    mentioned_ |= parse(t[index], false);
  }
}

bool StyleReader::applies(Scope scope) const noexcept {
  const bool in_style = started_ && (status_ == Status::pending || status_ == Status::suspended);
  switch (scope) {
    case Scope::always:
      return true;
    case Scope::loading:
      return in_style;
    case Scope::selected:
      return in_style && status_ == Status::pending;
  }
  return false;
}

void StyleReader::line(const tech::RawLine& raw) {
  where_ = &raw;
  const Tokens& t = raw.tokens;
  if (t.empty()) {
    return;
  }
  const Keyword* kw = find(t[0]);
  if (kw == nullptr) {
    // Reported for every line of the section, whatever its style, by the
    // full read only (the technology load).
    report(kIllegalKeyword, "Illegal keyword.  Legal keywords are:\n\t" + keyword_list(), Subset::all);
    return;
  }
  const bool bad_count = t.size() < kw->min_args || (t.size() > kw->max_args && kw->key != Key::device);
  if (bad_count) {
    report(kMalformed,
           "Malformed line for keyword " + std::string{kw->name} + ".  Correct usage:\n\t" + std::string{kw->name} +
               " " + std::string{kw->usage},
           Subset::all);
    return;
  }
  if (!applies(kw->scope)) {
    return;
  }
  if (kw->scope == Scope::selected) {
    mention_first(kw->key, t);
    mention_rest(kw->key, t);
  }
  kw->handler(*this, t);
}

// The first `style` line selects the style loaded with the technology.
void StyleReader::first_style(const Tokens& t) {
  if (t.size() == 2) {
    name_ = t[1];
  } else {
    const auto list = variant_list(t[3]);
    name_ = t[1] + (list.empty() ? std::string{} : list.front());
  }
  status_ = Status::pending;
}

// A `style` line while looking for a named style (ExtLoadStyle).
void StyleReader::wanted_style(const Tokens& t) {
  const std::string_view want = *target_;
  if (t.size() == 2) {
    if (t[1] == want) {
      status_ = Status::pending;
    }
    return;
  }
  if (!want.starts_with(t[1])) {
    return;
  }
  for (const std::string& v : variant_list(t[3])) {
    if (want.substr(t[1].size()) == v) {
      status_ = Status::pending;
      return;
    }
  }
}

void StyleReader::style_line(const Tokens& t) {
  if (t.size() != 2 && (t.size() != 4 || t[2].compare(0, 7, "variant") != 0)) {
    // Magic reports it for every style; the types subset for its own.
    if (status_ == Status::pending || report_ == Subset::all) {
      malformed(*find("style"));
    }
    return;
  }
  const bool first = !started_;
  started_ = true;
  if (status_ == Status::pending || status_ == Status::suspended) {
    status_ = Status::loaded;
  } else if (status_ == Status::not_loaded && !target_ && first) {
    first_style(t);
  } else if (status_ == Status::not_loaded && target_) {
    name_ = std::string{*target_};
    wanted_style(t);
  }
}

void StyleReader::variants(const Tokens& t) {
  for (const std::string& v : variants_line(t[1])) {
    if ((!v.empty() && v.front() == '*') || ends_with(name_, v)) {
      status_ = Status::pending;
      return;
    }
  }
  status_ = Status::suspended;
}

// Whether this pass reports a bad first type list of `key`
// (DBTechNoisyNameMask): each view reports the keywords of its subset.
bool StyleReader::noisy(Key key) const noexcept {
  if (report_ == Subset::all) {
    return true;  // ExtTechLine's DBTechNoisyNameMask
  }
  if (key == Key::height) {
    return report_ == Subset::heights;
  }
  return report_ == Subset::types && (key == Key::fet || key == Key::tiedown || key == Key::substrate);
}

// The first switch of ExtTechLine: the types of the first argument.
void StyleReader::mention_first(Key key, const Tokens& t) {
  switch (key) {
    case Key::areacap:
    case Key::connect:
    case Key::contact:
    case Key::devresist:
    case Key::disconnect:
    case Key::fet:
    case Key::fetresist:
    case Key::height:
    case Key::antenna:
    case Key::tiedown:
    case Key::overlap:
    case Key::perimc:
    case Key::resist:
    case Key::sidewall:
    case Key::sideoverlap:
    case Key::substrate:
      types1_ = parse(t[1], noisy(key));
      mentioned_ |= types1_;
      break;
    case Key::device:
      types1_ = parse(t[3], report_ == Subset::types);
      mentioned_ |= types1_;
      break;
    default:
      types1_ = {};
      break;
  }
}

// The other type arguments the keyword handlers read.
void StyleReader::mention_rest(Key key, const Tokens& t) {
  switch (key) {
    case Key::connect:
    case Key::disconnect:
    case Key::perimc:
      mention(t, 2);
      break;
    case Key::overlap:
      mention(t, 2);
      mention(t, 4);
      break;
    case Key::sideoverlap:
      mention(t, 2);
      mention(t, 3);
      if (t.size() == 6) {
        mention(t, 5);
      }
      break;
    case Key::sidewall:
      mention(t, 2);
      mention(t, 3);
      mention(t, 4);
      break;
    case Key::fet:
      mention(t, 2);
      if (!str_is_numeric(t[6])) {
        mention(t, 6);
      }
      break;
    default:
      mention_defaults(key, t);
      break;
  }
}

// ExtTechSimple*: the area, perimeter and overlap shorthands read their
// types only once a planeorder was seen; the sidewall one always does.
void StyleReader::mention_defaults(Key key, const Tokens& t) {
  const bool areas = key == Key::defaultareacap || key == Key::defaultperimeter;
  const bool overlaps = key == Key::defaultoverlap || key == Key::defaultsideoverlap;
  if (key == Key::defaultsidewall || ((areas || overlaps) && seen_plane_order_)) {
    mention(t, 1);
  }
  if (areas && seen_plane_order_ && t.size() > 5) {
    mention(t, t.size() - 3);
  }
  if (overlaps && seen_plane_order_) {
    mention(t, 3);
  }
}

// HEIGHT (ExtTech.c:2967-2989): numeric height and thickness for every
// real type of the first argument.
void StyleReader::height(const Tokens& t) {
  if (!str_is_numeric(t[2])) {
    report(kBadHeight, "Layer height " + t[2] + " must be numeric", Subset::heights);
    return;
  }
  if (!str_is_numeric(t[3])) {
    report(kBadHeight, "Layer thickness " + t[3] + " must be numeric", Subset::heights);
    return;
  }
  const auto h = static_cast<float>(std::strtod(t[2].c_str(), nullptr));
  const auto th = static_cast<float>(std::strtod(t[3].c_str(), nullptr));
  for (tech::TileType ty = tech::kFirstUserType; ty < tech_.type_count(); ++ty) {
    if (types1_.test(ty)) {
      heights_.heights[ty].height = h;
      heights_.heights[ty].thickness = th;
      height_lines_.set(ty);
    }
  }
}

// UNITS (ExtTech.c:3110-3117): doConvert.
void StyleReader::units(const Tokens& t) {
  if (t[1] == "microns" || t[1] == "um") {
    heights_.units_microns = true;
  } else if (t[1] != "lambda") {
    report(kBadUnits, "Units must be microns or lambda.  Using the default value (lambda).", Subset::heights);
  }
}

// PLANEORDER (ExtTech.c:3503-3520): still applied after `noplaneordering`.
void StyleReader::plane_order(const Tokens& t) {
  if (order_ == Order::not_needed) {
    report(kBadPlaneOrder, "\"planeorder\" specified after \"noplaneordering\".", Subset::heights);
  }
  order_ = Order::seen;
  seen_plane_order_ = true;
  const tech::Lookup<tech::PlaneId> plane = tech_.find_plane(t[1]);
  const std::int32_t pos = std::atoi(t[2].c_str());
  const auto tech_planes = static_cast<std::int32_t>(tech_.plane_count() - tech::kFirstUserPlane);
  if (!plane.ok()) {
    report(kBadPlaneOrder, "Unknown planeorder plane " + t[1], Subset::heights);
  } else if (pos < 0 || pos >= tech_planes) {
    report(kBadPlaneOrder, "Planeorder index must be [0.." + std::to_string(tech_planes - 1) + "]",
           Subset::heights);
  } else {
    heights_.plane_order[*plane.id] = pos;
  }
}

// NOPLANEORDER (ExtTech.c:3496-3502).
void StyleReader::no_plane_order() {
  if (order_ == Order::seen) {
    report(kBadPlaneOrder, "\"noplaneordering\" specified after \"planeorder\".", Subset::heights);
  } else {
    order_ = Order::not_needed;
  }
}

// extTechFinalStyle's plane-order check and height defaults
// (ExtTech.c:3801-3961). With `noplaneordering` Magic returns before both
// (ExtTech.c:3801-3802): a type without a `height` line keeps h = t = 0.
// The micron conversion of h and t is ExtStyle's (conversion.cpp).
void StyleReader::finish_heights() {
  heights_.style = name_;
  heights_.plane_ordering = order_ != Order::not_needed;
  if (order_ == Order::not_needed) {
    mark_height_sources();
    return;
  }
  check_plane_order();
  default_heights();
}

// The orders of the technology planes must be a permutation of 0..k;
// otherwise the warning and every plane's declaration order.
void StyleReader::check_plane_order() {
  const std::size_t first = tech::kFirstUserPlane;
  const std::size_t end = tech_.plane_count();
  std::vector<int> uses(end, 0);
  for (std::size_t p = first; p < end; ++p) {
    const std::int64_t pn = std::int64_t{heights_.plane_order[p]} + static_cast<std::int64_t>(first);
    if (pn >= static_cast<std::int64_t>(first) && pn < static_cast<std::int64_t>(end)) {
      ++uses[static_cast<std::size_t>(pn)];
    }
  }
  std::string conflicting;
  std::string missing;
  for (std::size_t p = first; p < end; ++p) {
    const std::string entry = " " + std::string{tech_.plane_name(static_cast<tech::PlaneId>(p))} + ",";
    conflicting += uses[p] > 1 ? entry : std::string{};
    missing += uses[p] < 1 ? entry : std::string{};
  }
  if (conflicting.empty() && missing.empty()) {
    return;
  }
  std::string text = "Warning: Extraction Style " + heights_.style + "\n";
  if (!conflicting.empty()) {
    text += "  Conflicting planeorder for plane(s):\n   " + conflicting + "\n";
  }
  if (!missing.empty()) {
    text += "  Missing planeorder for plane(s):\n   " + missing + "\n";
  }
  text += "  Magic will use the default planeorder for this style:\n   ";
  for (std::size_t p = first; p < end; ++p) {
    heights_.plane_order[p] = static_cast<std::int32_t>(p - first);
    text += " " + std::string{tech_.plane_name(static_cast<tech::PlaneId>(p))} + "=" + std::to_string(p - first) + ",";
  }
  report(kPlaneOrderFallback, std::move(text), Subset::heights, Print::plain);  // TxError
}

// A type's values come from its `height` line only when that line left
// neither h nor t at 0 (zinit would have replaced it).
void StyleReader::mark_height_sources() {
  for (tech::TileType ty = tech::kFirstUserType; ty < tech_.type_count(); ++ty) {
    TypeHeight& th = heights_.heights[ty];
    const bool defaulted = th.height == 0.0F || th.thickness == 0.0F;
    th.source = height_lines_.test(ty) && !defaulted ? HeightSource::height_line : HeightSource::defaulted;
  }
}

// zinit: t = 0.05 and h = 0.1 * order where the value is 0.
void StyleReader::default_heights() {
  mark_height_sources();
  for (tech::TileType ty = tech::kFirstUserType; ty < tech_.type_count(); ++ty) {
    TypeHeight& th = heights_.heights[ty];
    if (th.thickness == 0.0F) {
      th.thickness = 0.05F;
    }
    if (th.height == 0.0F) {
      const std::optional<tech::PlaneId> plane = tech_.home_plane(ty);
      const std::int32_t order = plane && *plane < heights_.plane_order.size() ? heights_.plane_order[*plane]
                                                                                : kNoPlaneOrder;
      th.height = static_cast<float>(0.1 * order);
    }
  }
}

void StyleReader::substrate(const Tokens& t) {
  if (t.back().starts_with("-")) {
    mention(Tokens{t.back().substr(1)}, 0);
  }
  substrate_rule(ctx_, t);
}

// The device line after its class (ExtTechLine DEVICE).
void StyleReader::device(const Tokens& t) {
  const DeviceClass* dc = lookup(kDevices, t[1]);
  if (dc == nullptr) {
    std::string text = "Illegal device.  Legal devices are:\n\t";
    for (const DeviceClass& d : kDevices) {
      text += " " + std::string{d.name};
    }
    report(kIllegalDevice, std::move(text));
    return;
  }
  if (t.size() - 1 < dc->min_args) {
    malformed(*find("device"));
    return;
  }
  device_mentions(dc->dev, t);
  device_rule(ctx_, t, dc->name);
}

// A `k=name` parameter argument: an `=` not preceded by `<` or `>`.
bool is_parameter_option(const std::string& arg) {
  const std::size_t eq = arg.find('=');
  return eq != std::string::npos && (eq == 0 || (arg[eq - 1] != '<' && arg[eq - 1] != '>'));
}

// A limit argument: `<`, `>`, `<=` or `>=` within the first four
// characters, followed by a number.
bool is_limit_option(const std::string& arg) {
  const std::size_t at = arg.find_first_of("<>");
  if (at == std::string::npos || at > 3) {
    return false;
  }
  const std::size_t skip = at + 1 < arg.size() && arg[at + 1] == '=' ? 1 : 0;
  return str_is_numeric(arg.substr(at + 1 + skip));
}

// Removes the trailing `k=name`, limit and `+types` arguments.
Tokens strip_device_options(Tokens t) {
  while (t.size() > 1 && is_parameter_option(t.back())) {
    t.pop_back();
  }
  while (t.size() > 1 && is_limit_option(t.back())) {
    t.pop_back();
  }
  if (t.size() > 1 && t.back().starts_with("+")) {
    t.pop_back();
  }
  return t;
}

void StyleReader::subcircuit_mentions(Tokens t) {
  if (t.size() >= 2 && parse(t.back(), false).empty()) {
    if (t[t.size() - 2] != "None") {
      mention(t, t.size() - 2);
    }
    t.resize(t.size() - 2);
  }
  std::size_t first = 4;
  std::size_t count = t.size() > 4 ? t.size() - 4 : 0;
  if (t.size() > 4 && str_is_int(t[4])) {
    first = 5;
    const long n = std::strtol(t[4].c_str(), nullptr, 10);
    const std::size_t available = t.size() - 5;
    count = n < 0 ? 0 : std::min(static_cast<std::size_t>(n), available);
  }
  for (std::size_t i = first; i < first + count; ++i) {
    mention(t, i);
  }
}

// The terminal and substrate types each class reads (they decide the
// active types only).
void StyleReader::device_mentions(Dev dev, Tokens t) {
  t = strip_device_options(std::move(t));
  const auto sub = [&](std::size_t i) {
    if (i < t.size() && t[i] != "None") {
      mention(t, i);
    }
  };
  switch (dev) {
    case Dev::bjt:
      mention(t, 4);
      mention(t, 5);
      break;
    case Dev::mosfet: {
      const bool asymmetric = t.size() > 7 && !str_is_numeric(t[7]);
      mention(t, 4);
      if (asymmetric) {
        mention(t, 5);
      }
      sub(asymmetric ? 6 : 5);
      break;
    }
    case Dev::diode:
    case Dev::pdiode:
    case Dev::ndiode:
      mention(t, 4);
      break;
    case Dev::cap:
    case Dev::caprev:
      mention(t, 4);
      for (int i = 0; i < 2 && t.size() > 5 && str_is_numeric(t.back()); ++i) {
        t.pop_back();
      }
      sub(5);
      break;
    case Dev::res:
      mention(t, 4);
      sub(5);
      break;
    case Dev::rsubckt:
    case Dev::csubckt:
    case Dev::dsubckt:
      mention(t, 4);
      if (t.size() > 5 && t[5].find('=') == std::string::npos) {
        sub(5);
      }
      break;
    default:
      subcircuit_mentions(std::move(t));
      break;
  }
}

// ExtTechFinal: a type connected to another is electrical.
void StyleReader::mention_connected() {
  const auto end = static_cast<tech::TileType>(tech_.user_type_end());
  for (tech::TileType t = tech::kFirstUserType; t < end; ++t) {
    tech::TypeMask others = tech_.connects_to(t);
    others.clear_type(t);
    if (!others.empty()) {
      mentioned_.set(t);
    }
  }
}

std::optional<StyleTables> StyleReader::finish() {
  if (!started_) {
    if (target_ && *target_ != "default") {
      return std::nullopt;
    }
    name_ = "default";
  } else if (target_ && status_ == Status::not_loaded) {
    return std::nullopt;
  }
  mention_connected();
  finish_heights();
  finish_style();
  return StyleTables{std::move(style_), mentioned_};
}

// The style: the plane and height tables, then the parse half of
// extTechFinalStyle (finalize.cpp). Heights stay in the style's units;
// micron_convert() converts.
void StyleReader::finish_style() {
  style_.name = name_;
  style_.units_microns = heights_.units_microns;
  style_.units_per_lambda = heights_.units_per_lambda;
  style_.plane_ordering = heights_.plane_ordering;
  style_.plane_order = heights_.plane_order;
  for (std::size_t t = 0; t < tech::kMaxTileTypes; ++t) {
    const TypeHeight& th = heights_.heights[t];
    style_.heights[t] = TypeHeightThickness{th.height, th.thickness};
    if (th.source == HeightSource::height_line) {
      style_.height_lines.set(static_cast<tech::TileType>(t));
    }
  }
  finalize_style(style_, tech_);
}

}  // namespace

std::vector<std::string> declared_styles(const tech::RawSection& section) {
  std::vector<std::string> names;
  const auto known = [&](std::string_view base) {
    return std::any_of(names.begin(), names.end(), [&](const std::string& n) { return n.starts_with(base); });
  };
  for (const tech::RawLine& line : section.lines) {
    const Tokens& t = line.tokens;
    const Keyword* kw = t.empty() ? nullptr : StyleReader::find(t[0]);
    if (kw == nullptr || kw->key != Key::style) {
      continue;
    }
    const bool plain = t.size() == 2;
    const bool with_variants = t.size() == 4 && t[2].compare(0, 7, "variant") == 0;
    if ((!plain && !with_variants) || known(t[1])) {
      continue;
    }
    if (plain) {
      names.push_back(t[1]);
      continue;
    }
    for (const std::string& v : variant_list(t[3])) {
      names.push_back(t[1] + v);
    }
  }
  return names;
}

std::optional<StyleTables> read_style_tables(const tech::RawSection& section, const tech::Technology& tech,
                                             diag::DiagnosticSink& sink, const ReadOptions& options) {
  if (options.style) {
    const auto names = declared_styles(section);
    const bool declared = std::find(names.begin(), names.end(), *options.style) != names.end();
    if (!declared && !(names.empty() && *options.style == "default")) {
      return std::nullopt;
    }
  }
  StyleReader reader{tech, sink, options};
  for (const tech::RawLine& line : section.lines) {
    reader.line(line);
  }
  return reader.finish();
}

void apply_non_electrical(ExtStyle& style, const tech::TypeMask& mentioned, std::size_t user_type_end) {
  tech::TypeMask unhandled;
  for (std::size_t t = tech::kFirstUserType; t < user_type_end; ++t) {
    if (!mentioned.test(static_cast<tech::TileType>(t))) {
      unhandled.set(static_cast<tech::TileType>(t));
    }
  }
  style.non_electrical = unhandled;
  style.active &= ~unhandled;
}

}  // namespace hefesto::extract::detail
