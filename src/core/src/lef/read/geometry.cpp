// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LefReadRect (lefRead.c:868-936), LefReadLefPoint (lefRead.c:818-843) and
// LefReadPolygon (lefRead.c:965-1031). Where Magic dereferences a missing
// token at the end of the input, the statement is a parse error (X-2
// tier 1).
//
// Owner: LEFR (design D2).

#include "hefesto/lef/read/geometry.hpp"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

#include "hefesto/lef/read/units.hpp"

namespace hefesto::lef {

namespace {

constexpr std::string_view kErrorCode = "LEF-READ-001";

constexpr bool fits(std::int64_t v) noexcept {
  return v >= std::numeric_limits<db::Coord>::min() && v <= std::numeric_limits<db::Coord>::max();
}

struct Cursor {
  Lexer& lexer;
  std::optional<Token> token;

  void advance() { token = lexer.take(); }
  [[nodiscard]] bool starts(char c) const { return token && token->text.starts_with(c); }
  [[nodiscard]] std::optional<float> number() const { return token ? parse_number(token->text) : std::nullopt; }
};

using Pair = std::pair<float, float>;

// x at the current token, y at the next; the cursor stays on y.
std::optional<Pair> read_pair(Cursor& cursor) {
  const auto x = cursor.number();
  if (!x) {
    return std::nullopt;
  }
  cursor.advance();
  const auto y = cursor.number();
  if (!y) {
    return std::nullopt;
  }
  return Pair{*x, *y};
}

// The two corners in LefReadRect's token order: the first point is
// followed by one token read ahead (its `)` when parenthesized), the second
// reads past its y only to check a `)`.
std::optional<std::pair<Pair, Pair>> read_corners(Cursor& cursor) {
  bool paren = cursor.starts('(');
  if (paren) {
    cursor.advance();
  }
  const auto first = read_pair(cursor);
  if (!first) {
    return std::nullopt;
  }
  cursor.advance();
  if (paren) {
    if (!cursor.starts(')')) {
      return std::nullopt;
    }
    cursor.advance();
  }
  paren = cursor.starts('(');
  if (paren) {
    cursor.advance();
  }
  const auto second = read_pair(cursor);
  if (!second) {
    return std::nullopt;
  }
  if (paren) {
    cursor.advance();
    if (!cursor.starts(')')) {
      return std::nullopt;
    }
  }
  return std::pair{*first, *second};
}

}  // namespace

std::optional<db::Rect> read_rect(Lexer& lexer, const MessageTarget& messages, bool layer_known, float scale) {
  return read_rect(lexer, messages, layer_known, scale, lexer.take());
}

std::optional<db::Rect> read_rect(Lexer& lexer, const MessageTarget& messages, bool layer_known, float scale,
                                  std::optional<Token> first) {
  Cursor cursor{lexer, std::move(first)};
  const auto corners = read_corners(cursor);
  if (!corners) {
    messages.report(Dialect::lef, diag::Severity::error, kErrorCode, "Bad port geometry: RECT requires 4 values.");
    return std::nullopt;
  }
  if (!layer_known) {
    messages.report(Dialect::lef, diag::Severity::error, kErrorCode, "No layer defined for RECT.");
    return db::Rect{0, 0, 0, 0};
  }
  const auto x0 = to_internal(corners->first.first, scale);
  const auto y0 = to_internal(corners->first.second, scale);
  const auto x1 = to_internal(corners->second.first, scale);
  const auto y1 = to_internal(corners->second.second, scale);
  if (!x0 || !y0 || !x1 || !y1) {
    return std::nullopt;
  }
  return db::Rect{std::min(*x0, *x1), std::min(*y0, *y1), std::max(*x0, *x1), std::max(*y0, *y1)};
}

PointRead read_point(Lexer& lexer, float& x, float& y) {
  Cursor cursor{lexer, std::nullopt};
  cursor.advance();
  if (!cursor.token || cursor.starts(';')) {
    return PointRead::none;
  }
  const bool paren = cursor.starts('(');
  if (paren) {
    cursor.advance();
  }
  const auto pair = read_pair(cursor);
  if (!pair) {
    return PointRead::invalid;
  }
  if (paren) {
    cursor.advance();
    if (!cursor.starts(')')) {
      return PointRead::invalid;
    }
  }
  x = pair->first;
  y = pair->second;
  return PointRead::point;
}

namespace {

std::optional<db::Point> shifted_point(float x, float y, float scale, db::Point offset) {
  const auto px = to_internal(x, scale);
  const auto py = to_internal(y, scale);
  if (!px || !py) {
    return std::nullopt;
  }
  const std::int64_t sx = std::int64_t{*px} + offset.x;
  const std::int64_t sy = std::int64_t{*py} + offset.y;
  if (!fits(sx) || !fits(sy)) {
    return std::nullopt;
  }
  return db::Point{static_cast<db::Coord>(sx), static_cast<db::Coord>(sy)};
}

}  // namespace

std::vector<db::Point> read_points(Lexer& lexer, const MessageTarget& messages, float scale, db::Point offset,
                                   std::string_view what) {
  const std::string noun{what};
  const auto error = [&](const std::string& text) {
    messages.report(Dialect::lef, diag::Severity::error, kErrorCode, text);
  };
  std::vector<db::Point> points;
  while (auto token = lexer.take()) {
    if (token->text.starts_with(';')) {
      break;
    }
    const auto x = parse_number(token->text);
    if (!x) {
      error("Bad X value in " + noun + ".");
      (void)skip_statement(lexer);
      break;
    }
    token = lexer.take();
    if (!token || token->text.starts_with(';')) {
      error("Missing Y value in " + noun + " point!");
      break;
    }
    const auto y = parse_number(token->text);
    if (!y) {
      error("Bad Y value in " + noun + ".");
      (void)skip_statement(lexer);
      break;
    }
    if (const auto point = shifted_point(*x, *y, scale, offset)) {
      points.push_back(*point);
    }
  }
  return points;
}

}  // namespace hefesto::lef
