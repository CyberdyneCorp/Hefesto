// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The CIF parser: the main loop and command table (CIFReadFile,
// cif/CIFrdutils.c:1609-1704), user extensions (CIFParseUser,
// cif/CIFrdcl.c:1682-1730), point scaling with mid-read subdivision
// (CIFParsePoint, CIFParsePath, cif/CIFrdutils.c:586-720) and transforms
// (CIFParseTransform, CIFDirectionToTrans, cif/CIFrdutils.c:1475-1606).
// Owner: CIF.

#include <cstdlib>
#include <numeric>

#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/geometry.hpp"
#include "parser_impl.hpp"

namespace hefesto::cif {

std::optional<std::size_t> ParseStyle::find(std::string_view layer_name) const {
  for (std::size_t i = 0; i < layers.size(); ++i) {
    if (layers[i].in_style && layers[i].name == layer_name) {
      return i;
    }
  }
  return std::nullopt;
}

void parse(std::string_view text, const ParseStyle& style, const ParseOptions& options, CifReadTarget& target,
           ReadMessages& messages, ReadSession& session) {
  detail::Parser parser(text, style, options, target, messages, session);
  parser.run();
}

namespace detail {

namespace {

constexpr std::int64_t kDefaultRescaleLimit = 100;  // CIFMAXRESCALE

bool is_space(int ch) noexcept {
  return ch == ' ' || (ch >= '\t' && ch <= '\r');
}

}  // namespace

Parser::Parser(std::string_view text, const ParseStyle& style, const ParseOptions& options, CifReadTarget& target,
               ReadMessages& messages, ReadSession& session)
    : lex_(text),
      style_(style),
      options_(options),
      target_(target),
      messages_(messages),
      session_(session),
      multiplier_(style.multiplier) {
  session_.symbols =
      SymbolTable([this](std::string_view name) { return target_.cell_exists(name); }, options.max_symbols);
  session_.saw_end = false;
}

void Parser::run() {
  bool ended = false;
  while (!ended && lex_.peek() != Lexer::kEof) {
    if (cancelled()) {
      ended = true;  // Magic's SigInterruptPending: stop without the End message
      break;
    }
    lex_.skip_blanks();
    range_error_ = false;
    ended = command();
    if (!ended && !lex_.skip_semicolon()) {
      error("`;' expected.");
    }
  }
  if (!ended) {
    error("no \"End\" statement.");
  }
  cleanup();
  session_.errors = messages_.errors();
  session_.warnings = messages_.warnings();
}

bool Parser::cancelled() const noexcept {
  return options_.cancel != nullptr && options_.cancel->load(std::memory_order_relaxed);
}

bool Parser::command() {
  switch (lex_.peek()) {
    case Lexer::kEof:
    case ';':
      return false;
    case 'B': box(); return false;
    case 'C': call(); return false;
    case 'D': return definition_command();
    case 'E': end_command(); return true;
    case 'L': layer(); return false;
    case 'P': polygon(); return false;
    case 'R': flash(); return false;
    case 'W': wire(); return false;
    case '(':
      if (!lex_.skip_comment()) {
        error("(comment) extends to end of file.");
      }
      return false;
    default:
      if (lex_.peek() >= '0' && lex_.peek() <= '9') {
        user_extension();
      } else {
        unknown_command();
      }
      return false;
  }
}

bool Parser::definition_command() {
  lex_.take();  // the 'D'
  lex_.skip_blanks();
  switch (lex_.peek()) {
    case 'D': delete_definition(); break;
    case 'F': finish_definition(); break;
    case 'S': start_definition(); break;
    default: unknown_command(); break;
  }
  return false;
}

void Parser::unknown_command() {
  const int ch = lex_.peek();
  std::string text = "unknown command `";
  if (ch != Lexer::kEof) {
    text.push_back(static_cast<char>(ch));
  }
  error(text + "'; ignored.");
  lex_.skip_to_semicolon();
}

void Parser::end_command() {
  lex_.take();
  lex_.skip_blanks();
  session_.saw_end = true;
  if (lex_.peek() != Lexer::kEof) {
    error("End command isn't at end of file.");
  }
}

void Parser::user_extension() {
  if (lex_.take() == '9') {
    const int ch = lex_.peek();
    if (ch == '1' || ch == '4' || ch == '5') {
      lex_.take();
    }
    switch (ch) {
      case '1': user91(); return;
      case '4': user94(); return;
      case '5': user95(); return;
      default:
        if (is_space(ch)) {
          user9();
          return;
        }
    }
  }
  error("unimplemented user extension; ignored.");
  lex_.skip_to_semicolon();
}

void Parser::check(const diag::Status& status) {
  if (!status.ok()) {
    messages_.diagnostic(status.error());
  }
}

void Parser::outside_plane(std::string_view element) {
  messages_.diagnostic(diag::make_native(
      db::codes::kGeometryOutsidePlane,
      "Ignoring CIF " + std::string{element} + " at line " + std::to_string(lex_.line()) +
          ": its coordinates lie outside the usable plane (" + std::to_string(db::kPlaneMin) + ".." +
          std::to_string(db::kPlaneMax) + ")"));
}

// --- integers and points ----------------------------------------------------

std::optional<std::int64_t> Parser::unsigned_integer() {
  const auto value = lex_.signed_integer();
  if (value && *value < 0) {
    error("negative integer not permitted.");
  }
  return value;
}

std::int64_t Parser::mul(std::int64_t a, std::int64_t b) {
  // Every value stays within +-kIntegerLimit (2^61), so the sum of two
  // values never overflows and only products need a check; an overflow
  // saturates and marks the command's geometry as out of range.
  constexpr std::int64_t kLimit = Lexer::kIntegerLimit;
  if (a != 0 && std::llabs(b) > kLimit / std::llabs(a)) {
    range_error_ = true;
    return (a < 0) != (b < 0) ? -kLimit : kLimit;
  }
  return a * b;
}

std::int64_t Parser::saturate(std::int64_t v) {
  constexpr std::int64_t kLimit = Lexer::kIntegerLimit;
  if (v > kLimit || v < -kLimit) {
    range_error_ = true;
    return v < 0 ? -kLimit : kLimit;
  }
  return v;
}

std::int64_t Parser::rescale_limit() const noexcept {
  return options_.rescale_limit > 0 ? options_.rescale_limit : kDefaultRescaleLimit;
}

bool Parser::rescale(std::int64_t factor) {
  // CIFInputRescale(factor, 1): the read units are subdivided; the parser
  // keeps N and M, the engine scales S, distances and planes.
  scale_n_ = mul(scale_n_, factor);
  multiplier_ = mul(multiplier_, factor);
  check(target_.rescale(factor));
  warning("CIF style " + style_.name + ": units rescaled by factor of " + std::to_string(factor) + " / 1");
  return true;
}

std::int64_t Parser::scale_value(std::int64_t v, std::int64_t iscale, ReadPoint* earlier) {
  v = mul(mul(v, scale_n_), iscale);
  if (range_error_) {
    return v / scale_d_;  // the command is rejected; never rescale for a value that overflowed
  }
  if (v % scale_d_ != 0) {
    const std::int64_t factor = scale_d_ / std::gcd(scale_d_, std::llabs(v));
    if (mul(scale_n_, factor) > rescale_limit()) {
      warning("CIF units at maximum scale; value is rounded");
      v += v < 0 ? -((scale_d_ - 1) >> 1) : (scale_d_ >> 1);  // round to nearest, ties up
    } else {
      static_cast<void>(rescale(factor));
      v = mul(v, factor);
      if (earlier != nullptr) {
        earlier->x = mul(earlier->x, factor);
      }
    }
  }
  return v / scale_d_;
}

std::optional<ReadPoint> Parser::point(std::int64_t iscale) {
  ReadPoint p;
  const auto x = lex_.signed_integer();
  if (!x) {
    return std::nullopt;
  }
  p.x = scale_value(*x, iscale, nullptr);
  const auto y = lex_.signed_integer();
  if (!y) {
    return std::nullopt;
  }
  p.y = scale_value(*y, iscale, &p);
  return p;
}

std::optional<std::vector<ReadPoint>> Parser::path(std::int64_t iscale) {
  std::vector<ReadPoint> points;
  while (true) {
    lex_.skip_separators();
    if (lex_.peek() == ';') {
      break;
    }
    const std::int64_t saved = scale_n_;
    const auto p = point(iscale);
    if (!p) {
      return std::nullopt;
    }
    if (saved != scale_n_) {
      // A subdivision multiplies every point parsed so far.
      const std::int64_t factor = scale_n_ / saved;
      for (ReadPoint& q : points) {
        q = ReadPoint{mul(q.x, factor), mul(q.y, factor)};
      }
    }
    points.push_back(*p);
  }
  if (points.empty()) {
    return std::nullopt;
  }
  return points;
}

// --- transforms -------------------------------------------------------------

Transform64 Parser::direction(std::int64_t x, std::int64_t y) {
  if (x != 0 && y == 0) {
    return x > 0 ? Transform64{} : Transform64{-1, 0, 0, 0, -1, 0};  // identity, Geo180
  }
  if (y != 0 && x == 0) {
    return y > 0 ? Transform64{0, -1, 0, 1, 0, 0} : Transform64{0, 1, 0, -1, 0, 0};  // Geo270, Geo90
  }
  error("non-manhattan direction vector (" + std::to_string(x) + ", " + std::to_string(y) + "); ignored.");
  return Transform64{};
}

namespace {

/// `t` followed by `m` (GeoTransTrans(t, m)).
Transform64 then(const Transform64& t, const Transform64& m) {
  return Transform64{m.a * t.a + m.b * t.d, m.a * t.b + m.b * t.e, m.a * t.c + m.b * t.f,
                     m.d * t.a + m.e * t.d, m.d * t.b + m.e * t.e, m.d * t.c + m.e * t.f};
}

}  // namespace

bool Parser::transform_step(int ch, Transform64& t) {
  lex_.take();
  if (ch == 'T') {
    const auto p = point(1);
    if (!p) {
      error("translation, but no point.");
      return false;
    }
    // GeoTranslateTrans. Both terms are within +-2^61, so the sum fits;
    // it is saturated back to the limit so later steps cannot overflow.
    t.c = saturate(t.c + p->x);
    t.f = saturate(t.f + p->y);
    return true;
  }
  if (ch == 'M') {
    lex_.skip_blanks();
    const int axis = lex_.peek();
    if (axis != 'X' && axis != 'Y') {
      error("mirror, but not in X or Y.");
      return false;
    }
    lex_.take();
    t = then(t, axis == 'X' ? Transform64{-1, 0, 0, 0, 1, 0} : Transform64{1, 0, 0, 0, -1, 0});
    return true;
  }
  const auto x = lex_.signed_integer();
  const auto y = x ? lex_.signed_integer() : std::nullopt;
  if (!x || !y) {
    error("rotation, but no direction.");
    return false;
  }
  t = then(t, direction(*x, *y));
  return true;
}

bool Parser::transform(Transform64& t) {
  t = Transform64{};
  lex_.skip_blanks();
  for (int ch = lex_.peek(); ch != ';'; ch = lex_.peek()) {
    if (ch != 'T' && ch != 'M' && ch != 'R') {
      error("transformation expected.");
      lex_.skip_to_semicolon();
      return false;
    }
    if (!transform_step(ch, t)) {
      lex_.skip_to_semicolon();
      return false;
    }
    lex_.skip_blanks();
  }
  return true;
}

}  // namespace detail

}  // namespace hefesto::cif
