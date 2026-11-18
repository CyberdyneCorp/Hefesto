// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The extraction-style subset of the `extract` section reader (see
// style_reader.hpp).
// Owner: EXTSUB (openspec/changes/m6-lef-def-oasis/design.md D10).

#include "style_reader.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>

namespace hefesto::extract::detail {

namespace {

constexpr std::string_view kMalformed = "EXTRACT-STYLE-001";
constexpr std::string_view kIllegalDevice = "EXTRACT-STYLE-002";

enum class Key : std::uint8_t {
  areacap, connect, contact, cscale, defaultareacap, defaultoverlap, defaultperimeter, defaultsideoverlap,
  defaultsidewall, device, devresist, disconnect, fet, fetresist, fringeshieldhalo, height, antenna, model,
  tiedown, lambda, overlap, perimc, planeorder, noplaneordering, resist, rscale, sidehalo, sideoverlap, sidewall,
  step, style, substrate, units, variants,
};

struct Keyword {
  std::string_view name;
  Key key;
  std::size_t min_args;
  std::size_t max_args;
  std::string_view usage;
};

// keyTable of ExtTech.c, in its order.
constexpr std::array<Keyword, 34> kKeywords{{
    {"areacap", Key::areacap, 3, 3, "types capacitance"},
    {"connect", Key::connect, 3, 3, "types1 types1"},
    {"contact", Key::contact, 3, 6, "type resistance"},
    {"cscale", Key::cscale, 2, 2, "capacitance-scalefactor"},
    {"defaultareacap", Key::defaultareacap, 4, 6, "types plane capacitance"},
    {"defaultoverlap", Key::defaultoverlap, 6, 6, "types plane otertypes otherplane capacitance"},
    {"defaultperimeter", Key::defaultperimeter, 4, 6, "types plane capacitance"},
    {"defaultsideoverlap", Key::defaultsideoverlap, 6, 6, "types plane othertypes otherplane capacitance"},
    {"defaultsidewall", Key::defaultsidewall, 4, 5, "types plane capacitance [offset]"},
    {"device", Key::device, 4, 10, "dev-type dev-name types options..."},
    {"devresist", Key::devresist, 4, 4, "type region ohms-per-square"},
    {"disconnect", Key::disconnect, 3, 3, "types1 types1"},
    {"fet", Key::fet, 8, 9,
     "types terminal-types min-#-terminals name [subs-types] subs-node gscap gate-chan-cap"},
    {"fetresist", Key::fetresist, 4, 4, "type region ohms-per-square"},
    {"fringeshieldhalo", Key::fringeshieldhalo, 2, 2, "distance"},
    {"height", Key::height, 4, 4, "type height-above-subtrate thickness"},
    {"antenna", Key::antenna, 4, 6, "type [calc-type] [antenna-ratio-proportional] antenna-ratio-const"},
    {"model", Key::model, 2, 3, "partial-cumulative [area-sidewall]"},
    {"tiedown", Key::tiedown, 2, 2, "types"},
    {"lambda", Key::lambda, 2, 2, "units-per-lambda"},
    {"overlap", Key::overlap, 4, 5, "toptypes bottomtypes capacitance [shieldtypes]"},
    {"perimc", Key::perimc, 4, 4, "intypes outtypes capacitance"},
    {"planeorder", Key::planeorder, 3, 3, "plane index"},
    {"noplaneordering", Key::noplaneordering, 1, 1, "(no arguments needed)"},
    {"resist", Key::resist, 3, 4, "types resistance"},
    {"rscale", Key::rscale, 2, 2, "resistance-scalefactor"},
    {"sidehalo", Key::sidehalo, 2, 2, "distance"},
    {"sideoverlap", Key::sideoverlap, 5, 6, "intypes outtypes ovtypes capacitance [shieldtypes]"},
    {"sidewall", Key::sidewall, 6, 7, "intypes outtypes neartypes fartypes capacitance [offset]"},
    {"step", Key::step, 2, 2, "size"},
    {"style", Key::style, 2, 4, "stylename"},
    {"substrate", Key::substrate, 3, 5, "types plane [subs-node]"},
    {"units", Key::units, 2, 2, "lambda|microns"},
    {"variants", Key::variants, 2, 2, "style,..."},
}};

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

// LookupStruct over a table of lower-case names.
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

bool str_is_numeric(const std::string& s) {
  char* end = nullptr;
  (void)std::strtod(s.c_str(), &end);
  return end != s.c_str();
}

bool str_is_int(std::string_view s) noexcept {
  if (!s.empty() && (s.front() == '-' || s.front() == '+')) {
    s.remove_prefix(1);
  }
  for (const char c : s) {
    if (c < '0' || c > '9') {
      return false;
    }
  }
  return true;
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

using Tokens = std::vector<std::string>;

// ExtTechLine for one style.
class StyleReader {
 public:
  StyleReader(const tech::Technology& tech, diag::DiagnosticSink& sink, std::optional<std::string_view> target)
      : tech_(tech), sink_(sink), target_(target) {
    types_.active = all_but_space_and_drc();
  }

  void line(const tech::RawLine& line);
  [[nodiscard]] std::optional<StyleTypes> finish();

 private:
  [[nodiscard]] static tech::TypeMask all_but_space_and_drc();
  void style_line(const Tokens& t);
  void first_style(const Tokens& t);
  void wanted_style(const Tokens& t);
  void variants(const Tokens& t);
  void apply(const Keyword& kw, const Tokens& t);
  void mention_first(Key key, const Tokens& t);
  void mention_rest(Key key, const Tokens& t);
  void mention_defaults(Key key, const Tokens& t);
  void device(const Tokens& t);
  void device_mentions(Dev dev, Tokens t);
  void subcircuit_mentions(Tokens t);
  void resist(const Tokens& t);
  void substrate(const Tokens& t);
  [[nodiscard]] tech::TypeMask parse(std::string_view text, bool report);
  void mention(const Tokens& t, std::size_t index);
  void malformed(const Keyword& kw);
  void report(std::string_view code, std::string text);

  const tech::Technology& tech_;
  diag::DiagnosticSink& sink_;
  std::optional<std::string_view> target_;
  const tech::RawLine* where_ = nullptr;
  Status status_ = Status::not_loaded;
  bool started_ = false;  // ExtCurStyle != NULL
  bool seen_plane_order_ = false;
  tech::TypeMask mentioned_;  // allExtractTypes
  tech::TypeMask types1_;     // the first switch's types1
  StyleTypes types_;
};

tech::TypeMask StyleReader::all_but_space_and_drc() {
  tech::TypeMask mask;
  for (tech::TileType t = tech::kMagnet; t < tech::kMaxTileTypes; ++t) {
    mask.set(t);
  }
  return mask;
}

void StyleReader::report(std::string_view code, std::string text) {
  diag::Location where;
  if (where_ != nullptr && !where_->file.empty()) {
    where = diag::SourceLocation{where_->file, where_->line};
  }
  sink_.emit(diag::make(code, std::move(text), where));
}

void StyleReader::malformed(const Keyword& kw) {
  report(kMalformed, "Malformed line for keyword " + std::string{kw.name} + ".  Correct usage:\n\t" +
                         std::string{kw.name} + " " + std::string{kw.usage});
}

tech::TypeMask StyleReader::parse(std::string_view text, bool report_error) {
  auto parsed = tech_.parse_type_list(text);
  if (!parsed.ok()) {
    if (report_error) {
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

void StyleReader::line(const tech::RawLine& raw) {
  where_ = &raw;
  const Tokens& t = raw.tokens;
  if (t.empty()) {
    return;
  }
  const Keyword* kw = lookup(kKeywords, t[0]);
  if (kw == nullptr) {
    return;  // "Illegal keyword": printed when the technology loads
  }
  const bool bad_count = t.size() < kw->min_args || (t.size() > kw->max_args && kw->key != Key::device);
  if (bad_count) {
    return;  // "Malformed line": printed when the technology loads
  }
  if (kw->key == Key::style) {
    style_line(t);
    return;
  }
  if (!started_ || (status_ != Status::pending && status_ != Status::suspended)) {
    return;
  }
  if (kw->key == Key::variants) {
    variants(t);
  }
  if (status_ == Status::pending) {
    apply(*kw, t);
  }
}

// The first `style` line selects the style loaded with the technology.
void StyleReader::first_style(const Tokens& t) {
  if (t.size() == 2) {
    types_.style = t[1];
  } else {
    const auto list = variant_list(t[3]);
    types_.style = t[1] + (list.empty() ? std::string{} : list.front());
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
    if (status_ == Status::pending) {
      malformed(*lookup(kKeywords, "style"));
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
    types_.style = std::string{*target_};
    wanted_style(t);
  }
}

void StyleReader::variants(const Tokens& t) {
  for (const std::string& v : variants_line(t[1])) {
    if ((!v.empty() && v.front() == '*') || ends_with(types_.style, v)) {
      status_ = Status::pending;
      return;
    }
  }
  status_ = Status::suspended;
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
      types1_ = parse(t[1], key == Key::fet || key == Key::tiedown || key == Key::substrate);
      mentioned_ |= types1_;
      break;
    case Key::device:
      types1_ = parse(t[3], true);
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

void StyleReader::apply(const Keyword& kw, const Tokens& t) {
  mention_first(kw.key, t);
  mention_rest(kw.key, t);
  switch (kw.key) {
    case Key::device:
      device(t);
      break;
    case Key::fet:
      types_.gate |= types1_;
      break;
    case Key::tiedown:
      types_.diff |= types1_;
      break;
    case Key::substrate:
      substrate(t);
      break;
    case Key::resist:
      resist(t);
      break;
    case Key::planeorder:
      seen_plane_order_ = true;
      break;
    default:
      break;
  }
}

void StyleReader::resist(const Tokens& t) {
  if (str_is_int(t[2]) || t[2] != "None") {
    return;
  }
  for (tech::TileType ty = tech::kFirstUserType; ty < tech::kMaxTileTypes; ++ty) {
    if (types1_.test(ty)) {
      types_.active.clear_type(ty);
    }
  }
}

void StyleReader::substrate(const Tokens& t) {
  if (t.back().starts_with("-")) {
    mention(Tokens{t.back().substr(1)}, 0);
  }
  types_.substrate = types1_;
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
    malformed(*lookup(kKeywords, "device"));
    return;
  }
  device_mentions(dc->dev, t);
  if (dc->dev == Dev::mosfet || dc->dev == Dev::msubckt) {
    types_.gate |= types1_;
  }
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

std::optional<StyleTypes> StyleReader::finish() {
  if (!started_) {
    if (target_ && *target_ != "default") {
      return std::nullopt;
    }
    types_.style = "default";
  } else if (target_ && status_ == Status::not_loaded) {
    return std::nullopt;
  }
  // ExtTechFinal: a type connected to another is electrical; the user
  // types mentioned nowhere are not.
  const auto end = static_cast<tech::TileType>(tech_.user_type_end());
  for (tech::TileType t = tech::kFirstUserType; t < end; ++t) {
    tech::TypeMask others = tech_.connects_to(t);
    others.clear_type(t);
    if (!others.empty()) {
      mentioned_.set(t);
    }
  }
  for (tech::TileType t = tech::kFirstUserType; t < end; ++t) {
    if (!mentioned_.test(t)) {
      types_.active.clear_type(t);
    }
  }
  // The device records exist for real types only (TT_TECHDEPBASE ..
  // DBNumTypes).
  tech::TypeMask real;
  for (tech::TileType t = tech::kFirstUserType; t < tech_.type_count(); ++t) {
    real.set(t);
  }
  types_.gate &= real;
  return types_;
}

}  // namespace

std::vector<std::string> declared_styles(const tech::RawSection& section) {
  std::vector<std::string> names;
  const auto known = [&](std::string_view base) {
    return std::any_of(names.begin(), names.end(), [&](const std::string& n) { return n.starts_with(base); });
  };
  for (const tech::RawLine& line : section.lines) {
    const Tokens& t = line.tokens;
    if (t.empty() || lookup(kKeywords, t[0]) == nullptr || lookup(kKeywords, t[0])->key != Key::style) {
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

std::optional<StyleTypes> read_style(const tech::RawSection& section, const tech::Technology& tech,
                                     diag::DiagnosticSink& sink, const std::optional<std::string_view>& style) {
  if (style) {
    const auto names = declared_styles(section);
    const bool declared = std::find(names.begin(), names.end(), *style) != names.end();
    if (!declared && !(names.empty() && *style == "default")) {
      return std::nullopt;
    }
  }
  StyleReader reader{tech, sink, style};
  for (const tech::RawLine& line : section.lines) {
    reader.line(line);
  }
  return reader.finish();
}

}  // namespace hefesto::extract::detail
