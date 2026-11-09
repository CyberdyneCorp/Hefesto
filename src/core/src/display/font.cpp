// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Outline fonts (capability font-labels). Ported from Magic 8.3.694
// database/DBlabel.c (DBLoadFont, dbGetToken, CalcBezierPoints,
// DBFontLabelSetBBox, DBNameToFont), Copyright (C) 1985, 1990 Regents of
// the University of California, used under Magic's permissive license (see
// THIRD_PARTY_NOTICES.md). Rendering follows dbwind/DBWdisplay.c and
// graphics/grTCairo3.c (informative).

#include "hefesto/display/font.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>

#include "display/c_scan.hpp"
#include "hefesto/display/diag_codes.hpp"

namespace hefesto::display {

namespace {

constexpr std::size_t kTokenLine = 511;  // dbGetToken: fgets(line, 511, ff)
constexpr std::size_t kNumBuf = 16;
constexpr std::size_t kNotdefSlot = 95;
constexpr int kBezierSegments = 5;  // INTSEGS

constexpr std::array<std::string_view, kGlyphSlots> kAsciiNames = {
    "space", "exclam", "quotedbl", "numbersign", "dollar", "percent", "ampersand", "quoteright", "parenleft",
    "parenright", "asterisk", "plus", "comma", "hyphen", "period", "slash", "zero", "one", "two", "three", "four",
    "five", "six", "seven", "eight", "nine", "colon", "semicolon", "less", "equal", "greater", "question", "at", "A",
    "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X",
    "Y", "Z", "bracketleft", "backslash", "bracketright", "asciicircum", "underscore", "quoteleft", "a", "b", "c", "d",
    "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
    "braceleft", "bar", "braceright", "asciitilde", ".notdef",
};

/// dbGetToken over the whole file: comment ('%') and blank lines skipped.
std::vector<std::string> tokenize(std::string_view text) {
  std::vector<std::string> tokens;
  for (auto line : cscan::fgets_chunks(text, kTokenLine)) {
    std::size_t p = 0;
    while (p < line.size() && cscan::is_space(line[p]) && line[p] != '\n') {
      ++p;
    }
    if (p >= line.size() || line[p] == '%' || line[p] == '\n') {
      continue;
    }
    while (p < line.size()) {
      const std::size_t start = p;
      while (p < line.size() && !cscan::is_space(line[p])) {
        ++p;
      }
      tokens.emplace_back(line.substr(start, p - start));
      while (p < line.size() && cscan::is_space(line[p])) {
        ++p;
      }
    }
  }
  return tokens;
}

std::optional<int> numeric(std::string_view token) {
  const auto v = cscan::leading_number(token);
  if (!v) {
    return std::nullopt;
  }
  const double rounded = std::round(*v);
  // Out-of-range values would be undefined when converted (found by fuzz_font).
  if (!(rounded >= std::numeric_limits<int>::min() && rounded <= std::numeric_limits<int>::max())) {
    return std::nullopt;
  }
  return static_cast<int>(rounded);
}

std::optional<std::size_t> slot_of(std::string_view name) {
  for (std::size_t i = 0; i < kAsciiNames.size(); ++i) {
    if (kAsciiNames[i] == name) {
      return i;
    }
  }
  return std::nullopt;
}

/// The contour of one closed path: consecutive duplicates removed, and the
/// last point removed when it repeats the first.
std::vector<db::Point> close_contour(const std::vector<db::Point>& path) {
  std::vector<db::Point> out;
  for (const db::Point& p : path) {
    if (out.empty() || !(out.back() == p)) {
      out.push_back(p);
    }
  }
  if (out.size() > 1 && out.back() == out.front()) {
    out.pop_back();
  }
  return out;
}

class FontParser {
 public:
  explicit FontParser(double scale) : scale_(scale) {}

  void run(const std::vector<std::string>& tokens);
  [[nodiscard]] bool has_font() const noexcept { return font_.has_value(); }
  [[nodiscard]] OutlineFont take() { return std::move(*font_); }

 private:
  [[nodiscard]] std::string next() { return pos_ < tokens_->size() ? (*tokens_)[pos_++] : std::string{}; }
  [[nodiscard]] int ring(std::size_t back) const { return numbers_[(index_ + kNumBuf - back) % kNumBuf]; }
  void push(int v) {
    numbers_[index_] = v;
    index_ = (index_ + 1) % kNumBuf;
  }
  /// Returns false to stop (definefont).
  bool outside(const std::string& token);
  void font_bbox();
  void dictionary(const std::string& token);
  void glyph_header(const std::string& token);
  void glyph_path(const std::string& token);
  void close_path();
  void append_points(const std::string& token);

  double scale_;
  const std::vector<std::string>* tokens_ = nullptr;
  std::size_t pos_ = 0;
  int state_ = 0;  // Magic's chardef: 0 header, 1 dictionary, 2 glyph header, 3 glyph path
  std::string psname_;
  db::Rect extents_;
  std::optional<OutlineFont> font_;
  std::size_t slot_ = 0;
  std::array<int, kNumBuf> numbers_{};
  std::size_t index_ = 0;
  std::vector<db::Point> path_;  // persists across glyphs until a closepath, as in Magic
  bool contours_reset_ = false;
};

void FontParser::run(const std::vector<std::string>& tokens) {
  tokens_ = &tokens;
  while (pos_ < tokens.size()) {
    const std::string token = next();
    if (!token.empty() && token.front() == '/') {
      psname_ = token.substr(1);
    }
    switch (state_) {
      case 0:
        if (!outside(token)) {
          return;
        }
        break;
      case 1:
        dictionary(token);
        break;
      case 2:
        glyph_header(token);
        break;
      default:
        glyph_path(token);
        break;
    }
  }
}

bool FontParser::outside(const std::string& token) {
  if (token == "/FontBBox") {
    font_bbox();
  } else if (token == "CharProcs") {
    if (next() == "begin") {
      state_ = 1;
      font_.emplace();
      font_->extents = extents_;
      font_->extents.y1 = static_cast<db::Coord>(static_cast<float>(extents_.y1) * static_cast<float>(scale_));
    }
  } else if (token == "definefont") {
    if (font_ && !psname_.empty()) {
      const auto dot = psname_.rfind('.');
      font_->name = dot == std::string::npos ? psname_ : psname_.substr(0, dot);
    }
    return false;
  }
  return true;
}

void FontParser::font_bbox() {
  std::string first = next();
  std::string_view t = first;
  if (!t.empty() && t.front() == '{') {
    t.remove_prefix(1);
  }
  while (!t.empty() && cscan::is_space(t.front())) {
    t.remove_prefix(1);
  }
  if (const auto v = numeric(t)) {
    extents_.x0 = *v;
  }
  for (db::Coord* field : {&extents_.y0, &extents_.x1, &extents_.y1}) {
    if (const auto v = numeric(next())) {
      *field = *v;
    }
  }
}

void FontParser::dictionary(const std::string& token) {
  if (token == "currentdict") {
    if (next() == "end") {
      state_ = 0;
    }
  } else if (token == "{") {
    if (const auto slot = slot_of(psname_)) {
      slot_ = *slot;
      state_ = 2;
    }
    index_ = 0;
  }
}

void FontParser::glyph_header(const std::string& token) {
  if (token == "}") {
    state_ = 1;
  }
  if (const auto v = numeric(token)) {
    push(*v);
  } else if (token == "setcachedevice") {
    state_ = 3;
    GlyphOutline& g = font_->glyphs[slot_];
    g.advance = db::Point{ring(6), ring(5)};
    g.bbox = db::Rect{ring(4), ring(3), ring(2), ring(1)};
    contours_reset_ = true;  // a redefinition replaces the glyph's contours
  }
}

void FontParser::glyph_path(const std::string& token) {
  if (const auto v = numeric(token)) {
    push(*v);
  } else if (token == "}") {
    state_ = 1;
  } else if (token == "closepath") {
    close_path();
  } else {
    append_points(token);
  }
}

void FontParser::close_path() {
  GlyphOutline& g = font_->glyphs[slot_];
  if (contours_reset_) {
    g.contours.clear();
    contours_reset_ = false;
  }
  auto contour = close_contour(path_);
  if (!contour.empty()) {
    g.contours.push_back(std::move(contour));
  }
  path_.clear();
}

void FontParser::append_points(const std::string& token) {
  if (token == "curveto") {
    const db::Point c1{ring(6), ring(5)};
    const db::Point c2{ring(4), ring(3)};
    const db::Point end{ring(2), ring(1)};
    if (path_.empty()) {  // no current point: Magic reads a stale pointer here
      path_.insert(path_.end(), {c1, c2, end});
      return;
    }
    const auto points = flatten_bezier(path_.back(), c1, c2, end);
    path_.insert(path_.end(), points.begin(), points.end());
    return;
  }
  path_.push_back(db::Point{ring(2), ring(1)});  // moveto, lineto and anything else
}

}  // namespace

const GlyphOutline& OutlineFont::glyph(char c) const noexcept {
  const auto code = static_cast<unsigned char>(c);
  return code >= 32 && code <= 126 ? glyphs[code - 32U] : glyphs[kNotdefSlot];
}

diag::Outcome<OutlineFont> parse_outline_font(std::string_view text, std::string_view file_name, double scale) {
  FontParser parser(scale);
  parser.run(tokenize(text));
  if (!parser.has_font()) {
    return diag::make(codes::kFontFormat,
                      "Font file \"" + std::string(file_name) + "\" has no glyph dictionary (CharProcs begin)");
  }
  return parser.take();
}

std::vector<db::Point> flatten_bezier(db::Point p0, db::Point p1, db::Point p2, db::Point p3) {
  // Single precision, as DBFontInitCurves / CalcBezierPoints.
  const float cx = static_cast<float>(3.0 * static_cast<double>(static_cast<float>(p1.x - p0.x)));
  const float bx = static_cast<float>(3.0 * static_cast<double>(static_cast<float>(p2.x - p1.x)) - static_cast<double>(cx));
  const float ax = static_cast<float>(p3.x - p0.x) - cx - bx;
  const float cy = static_cast<float>(3.0 * static_cast<double>(static_cast<float>(p1.y - p0.y)));
  const float by = static_cast<float>(3.0 * static_cast<double>(static_cast<float>(p2.y - p1.y)) - static_cast<double>(cy));
  const float ay = static_cast<float>(p3.y - p0.y) - cy - by;
  std::vector<db::Point> out;
  db::Point last = p0;
  for (int i = 0; i < kBezierSegments; ++i) {
    const float t = static_cast<float>(i + 1) / static_cast<float>(kBezierSegments + 1);
    const float t2 = t * t;
    const float t3 = t2 * t;
    const db::Point p{static_cast<db::Coord>(ax * t3 + bx * t2 + cx * t + static_cast<float>(p0.x)),
                      static_cast<db::Coord>(ay * t3 + by * t2 + cy * t + static_cast<float>(p0.y))};
    if (!(p == last)) {
      out.push_back(p);
      last = p;
    }
  }
  out.push_back(p3);
  return out;
}

int FontRegistry::find(std::string_view name) const {
  auto same = [name](std::string_view other) {
    return name.size() == other.size() &&
           std::equal(name.begin(), name.end(), other.begin(), [](char a, char b) {
             return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
           });
  };
  for (std::size_t i = 0; i < fonts.size(); ++i) {
    if (fonts[i] && same(fonts[i]->name)) {
      return static_cast<int>(i);
    }
  }
  return same("default") ? -1 : -2;
}

const OutlineFont* FontRegistry::font_of(const db::Label& label) const noexcept {
  if (label.font < 0 || static_cast<std::size_t>(label.font) >= fonts.size()) {
    return nullptr;
  }
  return fonts[static_cast<std::size_t>(label.font)].get();
}

// --- geometry -------------------------------------------------------------------

namespace {

constexpr double kDegreesToRadians = 0.0174532925;  // as written in DBlabel.c

struct TextExtent {
  db::Rect box;  // (0, Y1, W, Y2) in font units
};

TextExtent text_extent(const std::string& text, const OutlineFont& font) {
  db::Rect box{0, 0, 0, 0};  // GeoNullRect
  for (std::size_t i = 0; i < text.size(); ++i) {
    const GlyphOutline& g = font.glyph(text[i]);
    box.x1 += i + 1 == text.size() ? g.bbox.x1 : g.advance.x;
    box.y1 = std::max(box.y1, g.bbox.y1);
    box.y0 = std::min(box.y0, g.bbox.y0);
  }
  box.y1 = std::max(box.y1, font.extents.y1);
  return TextExtent{box};
}

db::Coord scale_value(db::Coord v, std::int32_t size, db::Coord y2) {
  const std::int64_t num = std::int64_t{v} * size;
  return static_cast<db::Coord>(num / (y2 == 0 ? 1 : y2));
}

void justify(db::Rect& r, db::Position just) {
  switch (just) {
    case 5: case 6: case 4:  // S, SW, SE
      r.y0 -= r.y1;
      r.y1 = 0;
      break;
    case 7: case 3: case 0:  // W, E, CENTER
      r.y1 >>= 1;
      r.y0 -= r.y1;
      break;
    default:
      break;
  }
  switch (just) {
    case 7: case 8: case 6:  // W, NW, SW
      r.x0 -= r.x1;
      r.x1 = 0;
      break;
    case 1: case 5: case 0:  // N, S, CENTER
      r.x1 >>= 1;
      r.x0 -= r.x1;
      break;
    default:
      break;
  }
}

db::Point rotate(db::Point p, double c, double s) {
  const double x = p.x * c - p.y * s;
  const double y = p.x * s + p.y * c;
  return db::Point{static_cast<db::Coord>(std::round(x)), static_cast<db::Coord>(std::round(y))};
}

db::Coord to_internal(db::Coord v, bool upper, bool magic_compat) {
  const bool expand = magic_compat ? (v & 0x7F) != 0 : (v & 0x7) != 0;
  v >>= 3;
  if (expand) {
    v += upper ? 1 : -1;
  }
  return v;
}

}  // namespace

FontLabelGeometry font_label_geometry(const db::Label& label, const OutlineFont& font, bool magic_compat) {
  FontLabelGeometry out;
  const TextExtent extent = text_extent(label.text, font);
  const db::Coord y2 = extent.box.y1;
  db::Rect r{scale_value(extent.box.x0, label.size, y2), scale_value(extent.box.y0, label.size, y2),
             scale_value(extent.box.x1, label.size, y2), scale_value(extent.box.y1, label.size, y2)};
  out.baseline_drop = r.y0;
  justify(r, label.justification);
  r.x0 += label.offset.x;
  r.x1 += label.offset.x;
  r.y0 += label.offset.y;
  r.y1 += label.offset.y;
  out.text_box = r;
  std::int32_t rot = label.rotation;
  if (rot < 0) {
    rot += 360;
  }
  if (rot >= 360) {
    rot -= 360;
  }
  out.rotation = rot;
  const double rad = rot * kDegreesToRadians;
  const double c = std::cos(rad);
  const double s = std::sin(rad);
  const std::array<db::Point, 4> unrotated = {db::Point{r.x0, r.y0}, db::Point{r.x1, r.y0}, db::Point{r.x1, r.y1},
                                              db::Point{r.x0, r.y1}};
  db::Rect bb{};
  for (std::size_t i = 0; i < 4; ++i) {
    out.corners[i] = rotate(unrotated[i], c, s);
    const db::Point& p = out.corners[i];
    bb = i == 0 ? db::Rect{p.x, p.y, p.x, p.y}
                : db::Rect{std::min(bb.x0, p.x), std::min(bb.y0, p.y), std::max(bb.x1, p.x), std::max(bb.y1, p.y)};
  }
  const db::Coord cx = (label.rect.x0 + label.rect.x1) * 4;
  const db::Coord cy = (label.rect.y0 + label.rect.y1) * 4;
  out.bbox = db::Rect{to_internal(bb.x0 + cx, false, magic_compat), to_internal(bb.y0 + cy, false, magic_compat),
                      to_internal(bb.x1 + cx, true, magic_compat), to_internal(bb.y1 + cy, true, magic_compat)};
  return out;
}

std::array<db::Point, 4> font_label_corners(const db::Label& label, const OutlineFont& font) {
  return font_label_geometry(label, font).corners;
}

std::vector<std::vector<PointF>> font_label_polygons(const db::Label& label, const OutlineFont& font) {
  const FontLabelGeometry geo = font_label_geometry(label, font);
  const db::Coord y2 = text_extent(label.text, font).box.y1;
  const double scale = static_cast<double>(label.size) / (y2 == 0 ? 1 : y2);  // font units -> 1/8 units
  const db::Rect& box = geo.text_box;
  const bool flip = geo.rotation >= 90 && geo.rotation < 270;  // keep the text upright
  const double rad = geo.rotation * kDegreesToRadians;
  const double c = std::cos(rad);
  const double s = std::sin(rad);
  const double cx = (label.rect.x0 + label.rect.x1) * 4.0;
  const double cy = (label.rect.y0 + label.rect.y1) * 4.0;
  std::vector<std::vector<PointF>> out;
  double pen = box.x0;
  const double base = box.y0 - geo.baseline_drop;
  for (const char ch : label.text) {
    const GlyphOutline& g = font.glyph(ch);
    for (const auto& contour : g.contours) {
      std::vector<PointF> poly;
      poly.reserve(contour.size());
      for (const db::Point& p : contour) {
        double x = pen + p.x * scale;
        double y = base + p.y * scale;
        if (flip) {
          x = box.x0 + box.x1 - x;
          y = box.y0 + box.y1 - y;
        }
        poly.push_back(PointF{(x * c - y * s + cx) / 8.0, (x * s + y * c + cy) / 8.0});
      }
      out.push_back(std::move(poly));
    }
    pen += g.advance.x * scale;
  }
  return out;
}

}  // namespace hefesto::display
