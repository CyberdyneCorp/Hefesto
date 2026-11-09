// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The .mag reader: header, technology check, scaling on read, body
// dispatch, paint records and post-read processing (cell-file-format,
// cell-timestamps "Timestamps on cell read"). Ported from Magic 8.3.694
// database/DBio.c (dbCellReadDef) and utils/getrect.c (GetRect); Magic is
// (c) the Regents of the University of California, distributed under its
// permissive license (THIRD_PARTY_NOTICES.md).

#include <charconv>
#include <limits>
#include <optional>
#include <set>

#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/tech/commands.hpp"
#include "hefesto/tech/loader.hpp"
#include "mag_reader.hpp"

namespace hefesto::cell {

namespace detail {

namespace {

enum class HeaderResult { ok, bad_file, tech_failed };

struct Header {
  HeaderResult result = HeaderResult::ok;
  std::int32_t stamp = 0;
  std::int64_t n = 1;
  std::int64_t d = 1;
};

std::string text_of(std::string_view line) {
  return std::string{chomp(line)};
}

/// "Technology check on read"; true when the read may continue.
bool check_technology(ReadState& st, const std::string& file_tech) {
  if (file_tech == st.tech().name()) {
    return true;
  }
  st.emit(codes::kTechMismatch, "Cell " + st.def.name() + " has technology \"" + file_tech +
                                    "\", but current technology is \"" + st.tech().name() + "\"");
  if (st.options.ignore_tech) {
    st.emit(codes::kReadNote, "Will attempt to read cell anyway.");
    return true;
  }
  if (st.ctx.load_technology && !tech::layout_has_paint(st.ctx.library, st.tech().plane_count())) {
    if (const tech::Technology* loaded = st.ctx.load_technology(file_tech)) {
      st.ctx.tech = loaded;
    }
  }
  if (file_tech == st.tech().name()) {
    return true;
  }
  st.emit(codes::kTechMismatch, "Use command \"tech load\" if you want to switch technologies, or use");
  st.emit(codes::kTechMismatch, "\"cellname delete " + st.def.name() + "\" and \"load " + st.def.name() +
                                    " -force\" to force the cell to load as technology " + st.tech().name());
  return false;
}

void read_magscale(ReadState& st, Header& h) {
  Scan s{st.line};
  std::int32_t n = 0;
  std::int32_t d = 0;
  if (s.lit("magscale")) {
    s.ws();
    if (s.integer(n) && s.integer(d)) {
      h.n = n;
      h.d = d;
    } else {
      st.emit(codes::kHeaderLine, "Expected two arguments to magscale;  ignoring");
      h.n = h.d = 1;
    }
  } else if (st.line.starts_with("maxlabscale")) {
    st.emit(codes::kHeaderLine, "Deprecated keyword \"maxlabscale\" in input file.");
  } else {
    st.emit(codes::kHeaderLine, "Expected magscale but got: " + text_of(st.line));
  }
}

void read_stamp_line(ReadState& st, Header& h) {
  Scan s{st.line};
  std::int32_t stamp = 0;
  if (s.lit("timestamp") && (s.ws(), s.integer(stamp))) {
    h.stamp = stamp;
  } else {
    st.emit(codes::kHeaderLine, "Expected timestamp but got: " + text_of(st.line));
  }
}

/// Lines after `tech`: optional magscale and timestamp, then the first body line.
HeaderResult read_optional_header(ReadState& st, Header& h) {
  if (!st.in.fgets(st.line)) {
    return HeaderResult::bad_file;
  }
  if (st.line.front() == 'm') {
    read_magscale(st, h);
    if (!st.in.fgets(st.line)) {
      return HeaderResult::bad_file;
    }
  }
  if (st.line.front() == 't') {
    read_stamp_line(st, h);
    if (!st.in.fgets(st.line)) {
      return HeaderResult::bad_file;
    }
  }
  return HeaderResult::ok;
}

Header read_header(ReadState& st) {
  Header h;
  if (!st.in.fgets(st.line)) {
    h.result = HeaderResult::bad_file;
    return h;
  }
  if (!st.line.starts_with("magic")) {
    st.emit(codes::kBadFirstLine, "First line in file must be \"magic\"; instead saw: " + text_of(st.line));
    h.result = HeaderResult::bad_file;
    return h;
  }
  if (!st.in.fgets(st.line)) {
    h.result = HeaderResult::bad_file;
    return h;
  }
  if (st.line.front() == '<') {
    return h;
  }
  Scan s{st.line};
  std::string file_tech;
  if (!s.lit("tech") || (s.ws(), !s.str(file_tech, 49))) {
    st.emit(codes::kBadTechLine, "Malformed \"tech\" line: " + text_of(st.line));
    h.result = HeaderResult::bad_file;
    return h;
  }
  if (!check_technology(st, file_tech)) {
    h.result = HeaderResult::tech_failed;
    return h;
  }
  h.result = read_optional_header(st, h);
  return h;
}

/// "Coordinate scaling on read" steps 1-5.
void reconcile_scale(ReadState& st, const Header& h) {
  const tech::GridScale ratio = tech::reduce(tech::GridScale{h.n * st.ctx.grid.internal, h.d * st.ctx.grid.lambda});
  st.n = ratio.internal;
  st.d = ratio.lambda;
  const bool limited = !tech::check_grid_limit(st.tech(), st.n, st.d).ok();
  if (!limited && st.d > 1 && st.ctx.rescale_database && st.ctx.rescale_database(st.d, 1)) {
    st.ctx.grid.internal *= st.d;
    st.d = 1;
  }
  if (st.d > 1) {  // Q-CELL-2: warned whenever a divisor remains
    st.emit(codes::kGeometryLost, "Warning:  Geometry may be lost because internal grid cannot be reduced.");
  }
}

// --- paint records ---------------------------------------------------------

/// One GetRect integer: optional '-', digits (none = 0); `c` is the first
/// character and receives the terminating one.
std::int64_t get_rect_int(MagInput& in, int& c) {
  const bool negative = c == '-';
  if (negative) {
    c = in.getc();
  }
  std::int64_t n = 0;
  constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
  for (; is_digit(c); c = in.getc()) {
    const std::int64_t digit = c - '0';
    // Saturate instead of overflowing; the range check rejects it later.
    n = n > (kMax - digit) / 10 ? kMax : n * 10 + digit;
  }
  return negative ? -n : n;
}

void skip_to_eol(MagInput& in, int c) {
  while (c != kEof && c != '\n') {
    c = in.getc();
  }
}

/// GetRect: 0 on a format error, else the corner bits (1 | s:2 | e:4).
int get_rect(ReadState& st, int skip, db::Rect& r) {
  MagInput& in = st.in;
  for (int i = 0; i < skip; ++i) {
    (void)in.getc();
  }
  std::int64_t v[4] = {};
  int c = in.getc();
  for (int i = 0; i < 4; ++i) {
    v[i] = get_rect_int(in, c);
    if (i == 3) {
      break;
    }
    if (!is_space(c)) {
      skip_to_eol(in, c);
      return 0;
    }
    c = in.skip_space_then_getc();
  }
  r = db::Rect{st.scale(v[0]), st.scale(v[1]), st.scale(v[2]), st.scale(v[3])};
  int dir = 0x1;
  while (c != kEof && c != '\n') {
    c = in.getc();
    if (c == 's') {
      dir |= 0x2;
    } else if (c == 'e') {
      dir |= 0x4;
    }
  }
  return dir;
}

std::optional<db::TriangleCorner> corner_of_bits(int dir) {
  switch ((dir >> 1) & 0x3) {
    case 0:
      return db::TriangleCorner::nw;
    case 1:
      return db::TriangleCorner::sw;
    case 3:
      return db::TriangleCorner::se;
    default:
      return db::TriangleCorner::ne;
  }
}

std::optional<db::TriangleCorner> corner_of_name(std::string_view t) {
  if (t == "nw") {
    return db::TriangleCorner::nw;
  }
  if (t == "sw") {
    return db::TriangleCorner::sw;
  }
  if (t == "se") {
    return db::TriangleCorner::se;
  }
  if (t == "ne") {
    return db::TriangleCorner::ne;
  }
  return std::nullopt;
}

struct PaintRecord {
  db::Rect rect;
  std::optional<db::TriangleCorner> corner;
};

/// Q-CELL-3 with magic-compat off: whole-token parsing of one line.
std::optional<PaintRecord> parse_record_tokens(ReadState& st, int first) {
  std::string rest;
  (void)st.in.rest_of_line(rest);
  const std::string line = static_cast<char>(first) + rest;
  std::vector<std::string_view> tok;
  for (std::size_t p = 0; p < line.size();) {
    while (p < line.size() && is_space(static_cast<unsigned char>(line[p]))) {
      ++p;
    }
    const std::size_t q = line.find_first_of(" \t\r\n\f\v", p);
    if (p < line.size()) {
      tok.push_back(std::string_view{line}.substr(p, q == std::string::npos ? std::string::npos : q - p));
    }
    p = q == std::string::npos ? line.size() : q;
  }
  const bool tri = first == 't';
  if (tok.size() != (tri ? 6U : 5U) || tok[0] != (tri ? "tri" : "rect")) {
    return std::nullopt;
  }
  std::int64_t v[4] = {};
  for (int i = 0; i < 4; ++i) {
    const auto t = tok[static_cast<std::size_t>(i) + 1];
    const auto res = std::from_chars(t.data(), t.data() + t.size(), v[i]);
    if (res.ec != std::errc{} || res.ptr != t.data() + t.size()) {
      return std::nullopt;
    }
  }
  PaintRecord rec{db::Rect{st.scale(v[0]), st.scale(v[1]), st.scale(v[2]), st.scale(v[3])}, std::nullopt};
  if (tri) {
    rec.corner = corner_of_name(tok[5]);
    if (!rec.corner) {
      return std::nullopt;
    }
  }
  return rec;
}

std::optional<PaintRecord> read_record(ReadState& st, int c) {
  if (!st.ctx.magic_compat) {
    return parse_record_tokens(st, c);
  }
  PaintRecord rec;
  const int dir = get_rect(st, c == 't' ? 3 : 4, rec.rect);
  if (dir == 0) {
    return std::nullopt;
  }
  if (c == 't') {
    rec.corner = corner_of_bits(dir);
  }
  return rec;
}

void paint_record(ReadState& st, const PaintRecord& rec, const tech::TypeMask& mask) {
  if (rec.rect.x0 >= rec.rect.x1 || rec.rect.y0 >= rec.rect.y1) {
    return;  // degenerate after scaling: silently ignored
  }
  std::optional<db::TriangleSpec> tri;
  if (rec.corner) {
    const db::TriangleSide side = db::triangle_side(*rec.corner);
    tri = db::TriangleSpec{side.diagonal, side.right_side};
  }
  const std::size_t end = st.tech().user_type_end();
  for (std::size_t t = 1; t < end; ++t) {
    const auto type = static_cast<tech::TileType>(t);
    if (!mask.test(type)) {
      continue;
    }
    if (auto done = db::paint_type(st.def, st.tech(), rec.rect, type, tri); !done.ok()) {
      st.ctx.sink.emit(done.error());
    }
  }
}

constexpr int kRecordError = -2;

/// Consecutive rect/tri records starting with character `c`: the first
/// character after them, or kRecordError on a format error.
int read_records(ReadState& st, const tech::TypeMask& mask, int c) {
  while (c == 'r' || c == 't') {
    const auto rec = read_record(st, c);
    if (!rec) {
      return kRecordError;
    }
    paint_record(st, *rec, mask);
    c = st.in.getc();
  }
  return c;
}

/// The paint records of one section; false on a format error.
bool read_paint_section(ReadState& st, const tech::TypeMask& mask) {
  while (true) {
    const int c = read_records(st, mask, st.in.getc());
    if (c == '#') {
      (void)st.in.rest_of_line(st.line);  // only the comment's own line
      continue;
    }
    if (c == '\n' || c == '\r') {
      continue;  // Q-CELL-1: blank lines are skipped
    }
    if (c == kEof || c == kRecordError) {
      return false;
    }
    std::string rest;
    if (!st.in.rest_of_line(rest)) {
      return false;
    }
    st.line = static_cast<char>(c) + rest;
    return true;
  }
}

tech::TypeMask residue_mask(const tech::Technology& tech, tech::TileType t) {
  if (const tech::ContactInfo* ci = tech.contact(t)) {
    return ci->residues;
  }
  return tech::TypeMask::of(t);
}

tech::TypeMask section_mask(ReadState& st, const std::string& token) {
  const tech::Technology& tech = st.tech();
  const auto found = tech.find_type(token);
  if (found.ok()) {
    const tech::TileType t = *found.id;
    const auto home = tech.home_plane(t);
    if (!home || *home == 0) {
      return {};
    }
    return t < tech.user_type_end() ? tech::TypeMask::of(t) : residue_mask(tech, t);
  }
  auto parsed = tech.parse_type_list(token);
  if (!parsed.ok()) {
    st.ctx.sink.emit(parsed.error());
    return {};
  }
  return parsed.value();
}

enum class Section { paint, labels, elements, properties, end };

Section classify(ReadState& st, const std::string& token) {
  if (st.tech().find_type(token).ok()) {
    return Section::paint;
  }
  if (token == "labels") {
    return Section::labels;
  }
  if (token == "elements") {
    return Section::elements;
  }
  if (token == "properties") {
    return Section::properties;
  }
  return token == "end" ? Section::end : Section::paint;
}

bool section_token(const std::string& line, std::string& token) {
  Scan s{line};
  return s.lit("<<") && s.str(token, std::string::npos);
}

/// The body loop; true when `<< end >>` was reached.
bool read_body(ReadState& st) {
  std::string token;
  while (true) {
    if (!section_token(st.line, token)) {
      if (!read_use(st)) {
        return false;
      }
      continue;
    }
    bool ok = true;
    switch (classify(st, token)) {
      case Section::end:
        return true;
      case Section::labels:
        ok = read_labels(st);
        break;
      case Section::elements:
        ok = read_elements(st);
        break;
      case Section::properties:
        ok = read_properties(st);
        break;
      case Section::paint:
        ok = read_paint_section(st, section_mask(st, token));
        break;
    }
    if (!ok) {
      return false;
    }
  }
}

void assign_unique_ids(ReadState& st) {
  db::CellDef& def = st.def;
  std::set<std::string, std::less<>> seen;
  std::vector<const db::CellUse*> unnamed;
  for (const auto& u : def.uses()) {
    if (u->id.empty()) {
      unnamed.push_back(u.get());
    } else if (!seen.insert(u->id).second) {
      st.emit(codes::kUseProblem, "Duplicate instance-id for cell " + u->def->name() + " (" + u->id +
                                      ") will be renamed");
      unnamed.push_back(u.get());
    }
  }
  (void)db::make_use_ids_unique(def, st.ctx.magic_compat);
  for (const auto& u : def.uses()) {
    if (std::find(unnamed.begin(), unnamed.end(), u.get()) != unnamed.end()) {
      st.emit(codes::kReadNote, "Setting instance-id of cell " + u->def->name() + " to " + u->id);
    }
  }
}

/// "Timestamps on cell read".
void process_read_stamp(ReadState& st, std::int32_t stamp) {
  db::CellDef& def = st.def;
  if (def.timestamp() != stamp || stamp == 0) {
    const bool in_parent = std::any_of(def.instances().begin(), def.instances().end(),
                                       [](const db::CellUse* u) { return u->parent != nullptr; });
    if (in_parent) {
      st.ctx.stamps.record(def, def.bbox());
      flag_mismatched_parents(def);
    }
  }
  def.set_timestamp(stamp);
  if (stamp == 0) {
    st.emit(codes::kZeroTimestamp, "\"" + def.name() + "\" has a zero timestamp; it should be written out");
    st.emit(codes::kZeroTimestamp, "    to establish a correct timestamp.");
    def.flags().child_timestamps_changed = true;
    def.flags().needs_new_timestamp = true;
  }
}

void finish(ReadState& st, std::int32_t stamp) {
  db::CellFlags& f = st.def.flags();
  f.modified = false;
  f.child_bboxes_changed = false;
  f.needs_new_timestamp = false;
  assign_unique_ids(st);
  process_read_stamp(st, stamp);
}

}  // namespace

}  // namespace detail

diag::Status read_mag(CellIoContext& ctx, db::CellDef& def, std::string_view text, const ReadOptions& options) {
  detail::ReadState st{ctx, def, text, options};
  const detail::Header h = detail::read_header(st);
  if (h.result == detail::HeaderResult::tech_failed) {
    return diag::make(codes::kTechMismatch, "cell " + def.name() + " was not read: technology mismatch");
  }
  def.flags().available = true;
  bool ok = h.result == detail::HeaderResult::ok;
  if (ok) {
    detail::reconcile_scale(st, h);
    ok = detail::read_body(st);
  }
  detail::finish(st, h.stamp);
  if (!ok) {
    return diag::make(codes::kFormatError, "File " + def.name() + " contained format error");
  }
  return diag::Done{};
}

}  // namespace hefesto::cell
