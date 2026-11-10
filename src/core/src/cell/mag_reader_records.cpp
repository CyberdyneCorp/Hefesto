// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Label, port, element and property sections of the .mag reader
// (cell-file-format "Label records", "Port records", "Element records",
// "Properties section records"; hefesto-mag-extensions). Ported from Magic
// 8.3.694 database/DBio.c (dbReadLabels, dbReadElements,
// dbReadProperties) and utils/lookup.c (Lookup); Magic is (c) the Regents
// of the University of California, distributed under its permissive
// license (THIRD_PARTY_NOTICES.md).

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <optional>

#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/hints/hints.hpp"
#include "mag_elements.hpp"
#include "mag_reader.hpp"

namespace hefesto::cell::detail {

namespace {

std::string text_of(std::string_view line) {
  return std::string{chomp(line)};
}

// --- labels ----------------------------------------------------------------

struct LabelFields {
  std::string layer;
  db::Rect rect;
  std::int32_t pos = 0;
  std::string font;  // empty: default font
  std::int32_t size = 0;
  std::int32_t rotate = 0;
  db::Point offset;
  std::string text;
  bool sticky = false;
};

/// The token after the layer of an rlabel/flabel line starts with 's'.
std::optional<bool> sticky_token(std::string_view line, std::string_view keyword) {
  Scan s{line};
  std::string layer;
  std::string flag;
  if (!s.lit(keyword) || !s.str(layer, 49) || !s.str(flag, 1)) {
    return std::nullopt;
  }
  return flag.front() == 's';
}

bool scan_rect_pos(Scan& s, LabelFields& f) {
  return s.integer(f.rect.x0) && s.integer(f.rect.y0) && s.integer(f.rect.x1) && s.integer(f.rect.y1) &&
         s.integer(f.pos);
}

bool scan_text(Scan& s, LabelFields& f) {
  s.ws();
  return s.rest(f.text);  // Q-CELL-5: the full text is kept
}

bool scan_sticky_char(Scan& s, LabelFields& f) {
  char c = 0;
  s.ws();
  f.sticky = true;
  return s.chr(c);
}

bool parse_rlabel(std::string_view line, LabelFields& f, bool sticky) {
  Scan s{line};
  if (!s.lit("rlabel") || !s.str(f.layer, 49) || (sticky && !scan_sticky_char(s, f))) {
    return false;
  }
  return scan_rect_pos(s, f) && scan_text(s, f);
}

bool parse_flabel(std::string_view line, LabelFields& f, bool sticky) {
  Scan s{line};
  if (!s.lit("flabel") || !s.str(f.layer, 49) || (sticky && !scan_sticky_char(s, f))) {
    return false;
  }
  return scan_rect_pos(s, f) && s.str(f.font, 255) && s.integer(f.size) && s.integer(f.rotate) &&
         s.integer(f.offset.x) && s.integer(f.offset.y) && scan_text(s, f);
}

bool parse_old_label(std::string_view line, LabelFields& f) {
  Scan s{line};
  if (!s.lit("label") || !s.str(f.layer, 49) || !s.integer(f.rect.x0) || !s.integer(f.rect.y0) ||
      !s.integer(f.pos) || !scan_text(s, f)) {
    return false;
  }
  f.rect.x1 = f.rect.x0;
  f.rect.y1 = f.rect.y0;
  return true;
}

/// The record of a label line; nullopt (after the skip message) when bad.
std::optional<LabelFields> parse_label_line(ReadState& st) {
  LabelFields f;
  const char kind = st.line.front();
  if (kind == 'l') {
    if (parse_old_label(st.line, f)) {
      return f;
    }
    st.emit(codes::kRecordSkipped, "Skipping bad \"label\" line: " + text_of(st.line));
    return std::nullopt;
  }
  const std::string_view keyword = kind == 'r' ? "rlabel" : "flabel";
  const auto sticky = sticky_token(st.line, keyword);
  if (!sticky) {
    st.emit(codes::kRecordSkipped, "Skipping bad \"flabel\" line: " + text_of(st.line));
    return std::nullopt;
  }
  const bool ok = kind == 'r' ? parse_rlabel(st.line, f, *sticky) : parse_flabel(st.line, f, *sticky);
  if (!ok) {
    st.emit(codes::kRecordSkipped, "Skipping bad \"" + std::string{keyword} + "\" line: " + text_of(st.line));
    return std::nullopt;
  }
  return f;
}

tech::TileType label_type(ReadState& st, const LabelFields& f) {
  const tech::Technology& tech = st.tech();
  std::optional<tech::TileType> type;
  if (const auto found = tech.find_type(f.layer); found.ok()) {
    type = found.id;
  } else {
    for (const auto& [name, mask] : tech.aliases()) {
      if (name == f.layer) {
        for (std::size_t t = 0; t < tech::kMaxTileTypes && !type; ++t) {
          if (mask.test(static_cast<tech::TileType>(t))) {
            type = static_cast<tech::TileType>(t);
          }
        }
      }
    }
  }
  if (!type) {
    st.emit(codes::kRecordValue,
            "Warning: label \"" + f.text + "\" attached to unknown type \"" + f.layer + "\"");
    return tech::kSpace;
  }
  return *type >= tech.user_type_end() ? db::label_layer_for(tech, *type) : *type;
}

std::int32_t font_index(const ReadState& st, const std::string& name) {
  if (name.empty() || st.ctx.fonts == nullptr) {
    return -1;
  }
  const int index = st.ctx.fonts->find(name);
  return index < -1 ? -1 : index;
}

void add_label(ReadState& st, const LabelFields& f) {
  db::Label lab;
  lab.text = f.text;
  lab.rect = db::Rect{st.scale(f.rect.x0), st.scale(f.rect.y0), st.scale(f.rect.x1), st.scale(f.rect.y1)};
  lab.justification = static_cast<db::Position>(f.pos);
  lab.sticky = f.sticky;
  lab.font = font_index(st, f.font);
  if (lab.font >= 0) {
    lab.size = st.scale(f.size);
    lab.rotation = f.rotate;
    lab.offset = db::Point{st.scale(f.offset.x), st.scale(f.offset.y)};
    const display::OutlineFont* font = st.ctx.fonts->font_of(lab);
    if (font != nullptr) {  // DBPutFontLabel -> DBFontLabelSetBBox, which stores the normalised rotation
      const display::FontLabelGeometry geo = display::font_label_geometry(lab, *font, st.ctx.magic_compat);
      lab.text_bbox = geo.bbox;
      lab.rotation = geo.rotation;
    }
  }
  lab.type = label_type(st, f);
  st.def.labels().push_back(std::move(lab));
}

// --- ports -----------------------------------------------------------------

std::optional<db::PortUse> port_use(std::string_view t) {
  if (t == "reset") {
    return db::PortUse::reset;  // Q-CELL-6: full words
  }
  if (t == "scan") {
    return db::PortUse::scan;
  }
  if (t == "tieoff") {
    return db::PortUse::tieoff;
  }
  switch (t.empty() ? '\0' : t.front()) {
    case 's':
      return db::PortUse::signal;
    case 'a':
      return db::PortUse::analog;
    case 'p':
      return db::PortUse::power;
    case 'g':
      return db::PortUse::ground;
    case 'c':
      return db::PortUse::clock;
    case 'd':
      return db::PortUse::default_use;
    default:
      return std::nullopt;
  }
}

std::optional<db::PortClass> port_class(std::string_view t) {
  switch (t.empty() ? '\0' : t.front()) {
    case 'i':
      return db::PortClass::input;
    case 'o':
      return db::PortClass::output;
    case 't':
      return db::PortClass::tristate;
    case 'b':
      return db::PortClass::bidirectional;
    case 'f':
      return db::PortClass::feedthrough;
    case 'd':
      return db::PortClass::default_class;
    default:
      return std::nullopt;
  }
}

std::optional<db::PortShape> port_shape(std::string_view t) {
  switch (t.empty() ? '\0' : t.front()) {
    case 'a':
      return db::PortShape::abutment;
    case 'r':
      return db::PortShape::ring;
    case 'f':
      return db::PortShape::feedthrough;
    case 'd':
      return db::PortShape::default_shape;
    default:
      return std::nullopt;
  }
}

std::uint8_t port_dirs(std::string_view dirs) {
  std::uint8_t bits = 0;
  for (const char c : dirs) {
    if (c == 'n') {
      bits |= db::kPortNorth;
    } else if (c == 's') {
      bits |= db::kPortSouth;
    } else if (c == 'e') {
      bits |= db::kPortEast;
    } else if (c == 'w') {
      bits |= db::kPortWest;
    }
  }
  return bits;
}

struct PortFields {
  std::int32_t index = 0;
  std::string dirs;
  std::string tok[3];
  int ntok = 0;
};

PortFields scan_port(std::string_view line) {
  PortFields p;
  Scan s{line};
  if (!s.lit("port") || !s.integer(p.index)) {
    return p;
  }
  p.ntok = 1;
  if (!s.str(p.dirs, 4)) {
    return p;
  }
  p.ntok = 2;
  for (auto& t : p.tok) {
    if (!s.str(t, 49)) {
      break;
    }
    ++p.ntok;
  }
  return p;
}

void apply_use_and_class(ReadState& st, db::Label& lab, const PortFields& p) {
  if (p.ntok == 3) {  // Q-CELL-6: Magic's three-token line; the token is the class
    if (const auto c = port_class(p.tok[0])) {
      lab.port_class = *c;
    } else {
      st.emit(codes::kRecordValue, "Ignoring unknown \"port\" use: " + p.tok[0]);
    }
    return;
  }
  if (const auto u = port_use(p.tok[0])) {
    lab.port_use = *u;
  } else {
    st.emit(codes::kRecordValue, "Ignoring unknown \"port\" use: " + p.tok[0]);
  }
  if (const auto c = port_class(p.tok[1])) {
    lab.port_class = *c;
  } else {
    st.emit(codes::kRecordValue, "Ignoring unknown \"port\" use: " + p.tok[0]);
  }
  if (p.ntok == 5) {
    if (const auto sh = port_shape(p.tok[2])) {
      lab.port_shape = *sh;
    } else {
      st.emit(codes::kRecordValue, "Ignoring unknown \"port\" shape: " + p.tok[2]);
    }
  }
}

void read_port(ReadState& st) {
  const PortFields p = scan_port(st.line);
  auto& labels = st.def.labels();
  if (labels.empty() || labels.back().port_directions != 0 || p.ntok < 2) {
    st.emit(codes::kRecordSkipped, "Skipping bad \"port\" line: " + text_of(st.line));
    return;
  }
  db::Label& lab = labels.back();
  lab.port_index = static_cast<std::uint32_t>(p.index);
  lab.port_directions = port_dirs(p.dirs);
  if (p.ntok >= 3) {
    apply_use_and_class(st, lab, p);
  }
}

// --- elements --------------------------------------------------------------

void read_element(ReadState& st) {
  std::vector<std::string> flag_errors;
  auto element = parse_element(st.line, &flag_errors);
  if (!element) {
    const char kind = st.line.front();
    const std::string_view keyword = kind == 'r' ? "rectangle" : (kind == 'l' ? "line" : "text");
    st.emit(codes::kRecordSkipped,
            "Skipping bad \"" + std::string{keyword} + "\" element line: " + text_of(st.line));
    return;
  }
  element->area = db::Rect{st.scale(element->area.x0), st.scale(element->area.y0), st.scale(element->area.x1),
                           st.scale(element->area.y1)};
  put_element(st.def, std::move(*element));
  for (const std::string& message : flag_errors) {
    st.emit(codes::kRecordValue, message);  // DBWElementParseFlags (Magic ref: dbwind/DBWelement.c:1016)
  }
}

// --- properties ------------------------------------------------------------

/// utils/lookup.c Lookup(): case-insensitive unique prefix, an exact match
/// wins; -1 none, -2 ambiguous.
/// Lookup over Magic's type table; `plane` (index 4) takes part only with
/// magic-compat OFF (Q-MISC-12).
constexpr int kPlaneType = 4;

int lookup_type(std::string_view token, bool accept_plane) {
  static constexpr std::string_view kTypes[] = {"string", "integer", "dimension", "double", "plane"};
  int match = -1;
  const int count = accept_plane ? 5 : 4;
  for (int i = 0; i < count; ++i) {
    const std::string_view name = kTypes[i];
    if (token.size() > name.size()) {
      continue;
    }
    bool prefix = true;
    for (std::size_t k = 0; k < token.size() && prefix; ++k) {
      prefix = std::tolower(static_cast<unsigned char>(token[k])) == name[k];
    }
    if (!prefix) {
      continue;
    }
    if (token.size() == name.size()) {
      return i;
    }
    match = match == -1 ? i : -2;
  }
  return match;
}

std::vector<std::string_view> value_tokens(std::string_view v) {
  std::vector<std::string_view> out;
  std::size_t p = 0;
  while (p < v.size()) {
    while (p < v.size() && is_space(static_cast<unsigned char>(v[p]))) {
      ++p;
    }
    std::size_t q = p;
    while (q < v.size() && !is_space(static_cast<unsigned char>(v[q]))) {
      ++q;
    }
    if (q > p) {
      out.push_back(v.substr(p, q - p));
    }
    p = q;
  }
  return out;
}

/// The %d value of a token that strtol(base 0) accepts; nullopt otherwise.
std::optional<std::int64_t> numeric_token(std::string_view t) {
  const std::string s{t};
  char* end = nullptr;
  (void)std::strtoll(s.c_str(), &end, 0);
  if (end == s.c_str()) {
    return std::nullopt;
  }
  Scan scan{s};
  std::int64_t v = 0;
  return scan.integer64(v) ? std::optional<std::int64_t>{v} : std::optional<std::int64_t>{0};
}

void put(ReadState& st, const std::string& key, std::optional<db::PropertyValue> value) {
  db::CellFlags& f = st.def.flags();
  const bool no_edit = f.no_edit;
  f.no_edit = false;  // properties are stored even on read-only cells
  db::set_property(st.def, key, std::move(value));
  f.no_edit = no_edit;
}

std::optional<std::vector<std::int64_t>> parse_numbers(std::string_view value) {
  std::vector<std::int64_t> out;
  for (const auto t : value_tokens(value)) {
    const auto v = numeric_token(t);
    if (!v) {
      return std::nullopt;  // Q-CELL-8: stored as a string and parsing stops
    }
    out.push_back(*v);
  }
  return out;
}

void store_numeric(ReadState& st, int type, const std::string& key, const std::string& value) {
  const auto numbers = parse_numbers(value);
  if (!numbers) {
    put(st, key, value);
    return;
  }
  if (type == 3) {
    put(st, key, db::DoubleList{*numbers});
    return;
  }
  std::vector<std::int32_t> ints;
  for (const std::int64_t v : *numbers) {
    ints.push_back(type == 2 ? st.scale(static_cast<std::int32_t>(v)) : static_cast<std::int32_t>(v));
  }
  if (type == 2) {
    put(st, key, db::DimensionList{ints});
  } else {
    put(st, key, db::IntegerList{ints});
  }
}

void store_bbox(ReadState& st, const std::string& key, const std::string& value) {
  Scan s{value};
  std::int32_t v[4] = {};
  if (!(s.integer(v[0]) && s.integer(v[1]) && s.integer(v[2]) && s.integer(v[3]))) {
    st.emit(codes::kRecordValue, "Cannot read bounding box values in " + key + " property");
    put(st, key, value);
    return;
  }
  put(st, key, db::DimensionList{{st.scale(v[0]), st.scale(v[1]), st.scale(v[2]), st.scale(v[3])}});
}

// mask-hints "Mask hints in .mag files": the value parsed by
// hints::parse_hint_value with the read scale. A non-integer token drops
// the property (DBPropPut with NULL, which also removes an earlier value).
void store_mask_hints(ReadState& st, const std::string& key, const std::string& value) {
  auto parsed = hints::parse_hint_value(value, st.n, st.d);
  if (!parsed.ok()) {
    st.emit(codes::kRecordValue, parsed.error().text);
    put(st, key, std::nullopt);
    return;
  }
  if (parsed.value().truncated) {
    st.emit(codes::kRecordValue, "Mask-hint property number of values is not divisible by four.  Truncated.");
  }
  put(st, key, db::PropertyValue{std::move(parsed.value().region)});
}

void store_offset(ReadState& st, const std::string& key, const std::string& value) {
  Scan s{value};
  std::int64_t v = 0;
  if (!s.integer64(v)) {
    st.emit(codes::kRecordValue, "Cannot read file offset value in " + key + " property");
    put(st, key, value);
    return;
  }
  put(st, key, db::DoubleList{{v}});
}

/// hefesto-mag-extensions "Extension values are versioned".
void check_extension(ReadState& st, const std::string& key, const std::string& value) {
  if (!key.starts_with(kExtensionPrefix) || value.size() < 3 || value.front() != 'v') {
    return;
  }
  const std::size_t colon = value.find(':');
  // Saturated: any version above kExtensionVersion is "newer", however long.
  constexpr int kSaturated = 1000000;
  int version = 0;
  for (std::size_t i = 1; i < colon && i < value.size(); ++i) {
    if (!is_digit(static_cast<unsigned char>(value[i]))) {
      return;
    }
    version = std::min(kSaturated, version * 10 + (value[i] - '0'));
  }
  if (colon != std::string::npos && version > kExtensionVersion) {
    st.emit(codes::kNewerExtension, "Cell " + st.def.name() + ": property " + key + " has schema version " +
                                        value.substr(0, colon) + "; it is kept unchanged");
  }
}

void store_string(ReadState& st, const std::string& key, const std::string& value) {
  if (key == "FIXED_BBOX" || key == "OBS_BBOX") {
    store_bbox(st, key, value);
  } else if (key.starts_with("GDS_START") || key.starts_with("GDS_BEGIN") || key.starts_with("GDS_END")) {
    store_offset(st, key, value);
  } else {
    check_extension(st, key, value);
    put(st, key, value);
  }
}

struct PropertyLine {
  int ntok = 0;
  std::string type;
  std::string key;
  std::string value;
};

PropertyLine scan_property(std::string_view line) {
  PropertyLine p;
  Scan s{line};
  if (!s.str(p.type, 31)) {
    return p;
  }
  p.ntok = 1;
  if (!s.str(p.key, 127)) {
    return p;
  }
  p.ntok = 2;
  s.ws();
  if (s.rest(p.value)) {
    p.ntok = 3;
  }
  return p;
}

/// A value line: `MASKHINTS_` keys are hints whatever the type keyword
/// (Q-MISC-10); `plane` (magic-compat OFF only, Q-MISC-12) is accepted for
/// hints and reported for other keys.
void store_value(ReadState& st, int type, const std::string& key, const std::string& value) {
  if (hints::hint_name(key)) {
    store_mask_hints(st, key, value);
  } else if (type == kPlaneType) {
    st.emit(codes::kRecordValue, "Plane type specified for property \"" + key + "\" but property is not a mask hint!");
  } else if (type == 0) {
    store_string(st, key, value);
  } else {
    store_numeric(st, type, key, value);
  }
}

/// One property line; false when the line ends the section.
bool read_property(ReadState& st) {
  const PropertyLine p = scan_property(st.line);
  const int type = p.ntok > 0 ? lookup_type(p.type, !st.ctx.magic_compat) : -1;
  if (p.ntok > 0 && type < 0) {
    return false;
  }
  if (p.ntok == 2 && !st.ctx.magic_compat) {
    // Q-CELL-7: an empty value reads back as empty (an empty hint region
    // for a MASKHINTS_ key, which is how it was written).
    put(st, p.key, hints::hint_name(p.key) ? db::PropertyValue{hints::HintRegion{}} : db::PropertyValue{std::string{}});
    return true;
  }
  if (p.ntok != 3) {
    st.emit(codes::kRecordSkipped, "Skipping bad property line: " + text_of(st.line));
    return true;
  }
  store_value(st, type, p.key, p.value);
  return true;
}

}  // namespace

bool read_labels(ReadState& st) {
  if (!st.in.fgets(st.line)) {
    return false;
  }
  while (true) {
    const char c = st.line.front();
    if (c != 'r' && c != 'l' && c != 'p' && c != 'f') {
      return true;
    }
    if (c == 'p') {
      read_port(st);
    } else if (const auto f = parse_label_line(st)) {
      add_label(st, *f);
    }
    if (!st.in.fgets(st.line)) {
      return false;
    }
  }
}

bool read_elements(ReadState& st) {
  if (!st.in.fgets(st.line)) {
    return false;
  }
  while (true) {
    const char c = st.line.front();
    if (c != 'r' && c != 'l' && c != 't') {
      return true;
    }
    read_element(st);
    if (!st.in.fgets(st.line)) {
      return false;
    }
  }
}

bool read_properties(ReadState& st) {
  if (!st.in.fgets(st.line)) {
    return false;
  }
  while (read_property(st)) {
    if (!st.in.fgets(st.line)) {
      return false;
    }
  }
  return true;
}

}  // namespace hefesto::cell::detail
