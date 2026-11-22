// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The DEF reader's shared helpers: tokens (LefNextToken with push-back,
// LefEndStatement, LefSkipSection, LefParseEndStatement), LefError with
// DEF's prefix, unit conversion, LefReadRect, LefReadLayer and the LefInfo
// lookups (capability def-read "Tokenization and keyword matching", "Unit
// scaling", "Diagnostics and error limits").
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/lefRead.c (LefReadRect,
// LefReadLayers) and lef/defRead.c, Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md). Where Magic reads
// a NULL token at the end of the input, the helpers see "" and the loops
// stop (X-2 tier 1).
//
// Owner: DEFR (design D2).

#include <algorithm>
#include <charconv>
#include <limits>
#include <memory>
#include <string>

#include "def/read/reader.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/drc/rules/query.hpp"
#include "hefesto/lef/read/units.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/undo/undo_log.hpp"

namespace hefesto::def::detail {

namespace {

constexpr std::string_view kEndCode = "LEF-LEX-003";

// The view of the session's LEF tables that lef::resolve_layer reads:
// LefInfo, where a via definition shadows a tech `lef` layer of its name.
class TableLayers final : public lef::LayerAccess {
 public:
  explicit TableLayers(const Reader& reader) : reader_(reader) {}

  [[nodiscard]] std::optional<lef::LayerEntry> find(std::string_view name) const override {
    const LefEntry entry = reader_.lookup(name);
    if (entry.via != nullptr) {
      lef::LayerEntry out;
      out.layer_class = entry.via->layer_class;
      out.type = entry.via->type.value_or(tech::kSpace);
      out.obstruction = entry.via->obstruction_type.value_or(tech::kSpace);
      out.contact_area = entry.via->area;
      return out;
    }
    if (entry.layer != nullptr) {
      return from_record(*entry.layer);
    }
    return std::nullopt;
  }
  // DEF read never changes the layer table through this view.
  void add_route(std::string_view /*name*/, tech::TileType /*type*/) override {}
  void update_route(std::string_view /*name*/, const lef::RouteUpdate& /*update*/) override {}
  void reserve(std::string_view /*name*/) override {}
  [[nodiscard]] bool keyed_without_entry(std::string_view name) const override {
    return reader_.keyed_without_record(name);
  }

 private:
  static lef::LayerEntry from_record(const lef::LayerRecord& record) {
    lef::LayerEntry out;
    out.layer_class = record.layer_class;
    out.type = record.type;
    out.obstruction = record.obstruction_type;
    for (std::size_t t = tech::kFirstUserType; t < tech::kMaxTileTypes; ++t) {
      const auto type = static_cast<tech::TileType>(t);
      if (type != record.obstruction_type && record.obstruction_types.test(type)) {
        out.via_obstruction = type;
        break;
      }
    }
    out.contact_area = record.contact_area;
    return out;
  }

  const Reader& reader_;
};

// The cursor of LefReadRect: the current token, and whether a `;` was
// consumed on the way.
struct RectCursor {
  Reader& reader;
  bool& semicolon;
  std::string token;

  void advance() {
    token = reader.take();
    if (token.starts_with(';')) {
      semicolon = true;
    }
  }
  [[nodiscard]] bool starts(char c) const { return token.starts_with(c); }
  [[nodiscard]] std::optional<float> number() const { return lef::parse_number(token); }
};

using Pair = std::pair<float, float>;

std::optional<Pair> read_pair(RectCursor& c) {
  const auto x = c.number();
  if (!x) {
    return std::nullopt;
  }
  c.advance();
  const auto y = c.number();
  if (!y) {
    return std::nullopt;
  }
  return Pair{*x, *y};
}

// One corner: `( x y )` or `x y`. The first corner reads its `)` and the
// token after it; the second reads only its `)`.
std::optional<Pair> read_corner(RectCursor& c, bool first) {
  const bool paren = c.starts('(');
  if (paren) {
    c.advance();
  }
  const auto pair = read_pair(c);
  if (!pair) {
    return std::nullopt;
  }
  if (first) {
    c.advance();
  }
  if (paren) {
    if (!first) {
      c.advance();
    }
    if (!c.starts(')')) {
      return std::nullopt;
    }
    if (first) {
      c.advance();
    }
  }
  return pair;
}

}  // namespace

// --- tokens ----------------------------------------------------------------------------

std::optional<std::string> TokenSource::next() {
  if (!pending_.empty()) {
    std::string token = std::move(pending_.front());
    pending_.pop_front();
    return token;
  }
  if (ended_) {
    return std::nullopt;
  }
  auto token = lexer_.take();
  if (!token) {
    ended_ = true;
    return std::nullopt;
  }
  return std::move(token->text);
}

std::string TokenSource::take() {
  return next().value_or(std::string{});
}

void TokenSource::push_front(std::vector<std::string> tokens) {
  for (auto it = tokens.rbegin(); it != tokens.rend(); ++it) {
    pending_.push_front(std::move(*it));
  }
}

void Reader::skip_statement() {
  while (const auto token = next()) {
    if (token->starts_with(';')) {
      return;
    }
  }
}

bool Reader::end_matches(std::string_view name) {
  const auto token = next();
  if (!token) {
    lef_messages_.report(lef::Dialect::lef, diag::Severity::error, kEndCode,
                         "Bad file read while looking for END statement");
    return false;
  }
  // `END END` answers -1, which Magic's callers take as a match.
  return lef::keyword_is(*token, name) || lef::keyword_is(*token, "END");
}

void Reader::skip_section(std::string_view name) {
  while (const auto token = next()) {
    if (lef::keyword_is(*token, "END")) {
      if (end_matches(name)) {
        return;
      }
    } else if (lef::keyword_is(*token, "ENDEXT") && name == "BEGINEXT") {
      return;
    }
  }
  lef_messages_.report(lef::Dialect::lef, diag::Severity::error, kEndCode,
                       "Section " + std::string{name} + " has no END record!");
}

// --- messages --------------------------------------------------------------------------

void Reader::error(std::string_view text) const {
  lef_messages_.report(lef::Dialect::def, diag::Severity::error, kError, text);
}

void Reader::warning(std::string_view text) const {
  lef_messages_.report(lef::Dialect::def, diag::Severity::warning, kWarning, text);
}

void Reader::message(std::string_view text) const {
  lef_messages_.report(lef::Dialect::def, diag::Severity::info, kMessage, text);
}

void Reader::lef_error(std::string_view text) const {
  lef_messages_.report(lef::Dialect::lef, diag::Severity::error, kError, text);
}

void Reader::fix_error(std::string_view text) const {
  lef_messages_.report(lef::Dialect::def, diag::Severity::error, kFixError, text);
}

void Reader::fix_warning(std::string_view text) const {
  lef_messages_.report(lef::Dialect::def, diag::Severity::warning, kFixWarning, text);
}

void Reader::report(std::string text) const {
  out_.emit(diag::make(kReport, std::move(text)));
}

void Reader::progress() const {
  if (context_.options.progress != nullptr) {
    context_.options.progress->update(lexer_.estimate());
  }
}

// LefEstimate returns only for a count of 0 (`if (!total) return;`), so a
// negative declared count ends the display suspension too (Q-LEFDEF-141).
void Reader::estimate(std::int32_t total) {
  estimated_ = estimated_ || total != 0;
  progress();
}

bool Reader::cancelled() const noexcept {
  return context_.options.progress != nullptr && context_.options.progress->cancelled();
}

// The via code's messages, counted and prefixed as LefError prints them.
class Reader::ViaSink final : public diag::DiagnosticSink {
 public:
  explicit ViaSink(const Reader& reader) : reader_(reader) {}
  void emit(const diag::Diagnostic& d) override {
    const lef::Dialect dialect = lef::via_message_dialect(d.code).value_or(lef::Dialect::def);
    reader_.lef_messages().report(dialect, d.severity, d.code, d.text);
  }

 private:
  const Reader& reader_;
};

Reader::Reader(lef::Lexer& lexer, lef::MessageCounter& counter, diag::DiagnosticSink& out,
               const ReadContext& context)
    : lexer_(lexer),
      counter_(counter),
      out_(out),
      context_(context),
      tokens_(lexer),
      lef_messages_{counter, out, lexer},
      via_sink_(std::make_unique<ViaSink>(*this)) {}

Reader::~Reader() = default;

lef::ViaContext Reader::via_context() noexcept {
  return lef::ViaContext{context_.tech, context_.style, context_.compat, *via_sink_};
}

// --- units and geometry ----------------------------------------------------------------

std::optional<db::Coord> Reader::scaled(float value) const noexcept {
  return lef::to_internal(value, oscale_);
}

std::optional<db::Coord> Reader::doubled(float value) const noexcept {
  return lef::to_internal(2.0F * value, oscale_);
}

std::optional<db::Rect> Reader::read_rect(bool layer_known, float scale, bool& semicolon) {
  semicolon = false;
  RectCursor cursor{*this, semicolon, {}};
  cursor.advance();
  const auto first = read_corner(cursor, true);
  const auto second = first ? read_corner(cursor, false) : std::nullopt;
  if (!second) {
    lef_error("Bad port geometry: RECT requires 4 values.");
    return std::nullopt;
  }
  if (!layer_known) {
    lef_error("No layer defined for RECT.");
    return db::Rect{0, 0, 0, 0};
  }
  const auto x0 = lef::to_internal(first->first, scale);
  const auto y0 = lef::to_internal(first->second, scale);
  const auto x1 = lef::to_internal(second->first, scale);
  const auto y1 = lef::to_internal(second->second, scale);
  if (!x0 || !y0 || !x1 || !y1) {
    return std::nullopt;  // a corner beyond the int range: Magic's cast is undefined
  }
  return db::canonical(db::Rect{*x0, *y0, *x1, *y1});
}

std::optional<std::vector<db::Point>> Reader::read_points(float scale) {
  std::vector<db::Point> points;
  while (auto token = next()) {
    if (*token != "(") {
      push_front(std::move(*token));
      break;
    }
    const auto x = lef::parse_number(take());
    const auto y = lef::parse_number(take());
    const std::string close = take();
    const auto px = x ? lef::to_internal(*x, scale) : std::nullopt;
    const auto py = y ? lef::to_internal(*y, scale) : std::nullopt;
    if (!px || !py || close != ")") {
      if (close.starts_with(';')) {
        push_front(close);
      }
      return std::nullopt;
    }
    points.push_back(db::Point{*px, *py});
  }
  return points;
}

std::optional<tech::TileType> Reader::read_layer(bool obstruction) {
  const auto token = next();
  if (!token) {
    return std::nullopt;
  }
  const TableLayers layers{*this};
  lef::LayerState state;
  const lef::LayerContext layer_context{layers, context_.tech, context_.compat, lef_messages_};
  // LefReadLayer: a key with no record resolves to nothing with
  // magic-compat ON (Q-LEFDEF-151).
  lef::resolve_layer(*token, obstruction ? lef::LayerLookup::obstruction : lef::LayerLookup::normal, state,
                     layer_context, lef::LayerSource::read_layer);
  return state.type;
}

LefEntry Reader::lookup(std::string_view name) const {
  const std::string key{name};
  if (const auto* via = context_.vias.find(key)) {
    return LefEntry{nullptr, via->value ? &*via->value : nullptr};
  }
  return LefEntry{context_.layers.find(name), nullptr};
}

bool Reader::keyed_without_record(std::string_view name) const {
  return lookup(name).empty() && (named_without_record(name) || context_.layers.has_key(name));
}

bool Reader::named_without_record(std::string_view name) const {
  const auto* via = context_.vias.find(std::string{name});
  return via != nullptr && !via->value;
}

std::optional<tech::TileType> Reader::lower_type(std::string_view name) const {
  std::string lower{name.substr(0, 255)};
  for (char& c : lower) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return context_.tech.find_type(lower).id;
}

std::int32_t Reader::drc_width(tech::TileType type) const {
  return context_.rules != nullptr ? drc::default_width(*context_.rules, context_.tech, type) : 0;
}

db::Coord Reader::default_width(const LefEntry& entry, tech::TileType type) const {
  const std::int64_t fallback = 3 * context_.lambda_internal / std::max<std::int64_t>(context_.lambda_lambda, 1);
  const auto fit = [](std::int64_t v) {
    return static_cast<db::Coord>(std::clamp<std::int64_t>(v, 0, db::kPlaneMax));
  };
  std::int64_t width = fallback;
  if (entry.layer != nullptr) {
    width = entry.layer->width.value_or(0);
  } else if (entry.via != nullptr) {
    width = entry.via->area.x0;  // info.route.width shares its storage with info.via.area.r_xbot
  }
  if (width > 0) {
    return fit(width);
  }
  const std::int32_t drc = drc_width(type);
  return fit(drc > 0 ? drc : fallback);
}

void Reader::paint(db::CellDef& cell, tech::TileType type, const db::Rect& rect) const {
  if (type == tech::kSpace || type >= context_.tech.type_count()) {
    return;
  }
  if (const auto clipped = clip_to_plane(rect)) {
    (void)db::paint_type(cell, context_.tech, *clipped, type);
  }
}

// --- free helpers ----------------------------------------------------------------------

db::Transform orientation_transform(std::size_t index) noexcept {
  switch (index) {
    case 1:
      return db::Transform{-1, 0, 0, 0, -1, 0};  // S
    case 2:
      return db::Transform{0, 1, 0, -1, 0, 0};  // E
    case 3:
      return db::Transform{0, -1, 0, 1, 0, 0};  // W
    case 4:
      return db::Transform{-1, 0, 0, 0, 1, 0};  // FN
    case 5:
      return db::Transform{1, 0, 0, 0, -1, 0};  // FS
    case 6:
      return db::Transform{0, -1, 0, -1, 0, 0};  // FE
    case 7:
      return db::Transform{0, 1, 0, 1, 0, 0};  // FW
    default:
      return db::Transform{};  // N
  }
}

std::optional<std::int32_t> parse_int(std::string_view token) noexcept {
  std::size_t start = 0;
  while (start < token.size() && (token[start] == ' ' || token[start] == '\t')) {
    ++start;
  }
  if (start < token.size() && token[start] == '+') {
    ++start;  // from_chars takes no '+'
  }
  std::int32_t value = 0;
  const auto [end, ec] = std::from_chars(token.data() + start, token.data() + token.size(), value);
  if (ec != std::errc{} || end == token.data() + start) {
    return std::nullopt;
  }
  return value;
}

std::optional<std::int32_t> parse_whole_int(std::string_view token) noexcept {
  std::int32_t value = 0;
  const auto [end, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
  if (ec != std::errc{} || end != token.data() + token.size() || token.empty()) {
    return std::nullopt;
  }
  return value;
}

std::optional<db::Rect> clip_to_plane(const db::Rect& r) noexcept {
  const db::Rect plane = db::whole_plane();
  const db::Rect clipped{std::max(r.x0, plane.x0), std::max(r.y0, plane.y0), std::min(r.x1, plane.x1),
                         std::min(r.y1, plane.y1)};
  if (clipped.x0 >= clipped.x1 || clipped.y0 >= clipped.y1) {
    return std::nullopt;
  }
  return clipped;
}

}  // namespace hefesto::def::detail
