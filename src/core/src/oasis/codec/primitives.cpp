// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The OASIS primitive decoders and encoders (capability oasis-format
// "Unsigned and signed integers", "Real numbers", "Strings and character
// sets", "Delta encodings", "Intervals"; design D11):
// - integers accept up to 64 value bits and any number of redundant zero
//   groups (Q-OASIS-1); a set bit beyond bit 63 is OASIS-CODEC-005;
// - reals of types 0-7 decode exactly (n and d widened to binary64 before
//   dividing); the writer picks type 0/1, then 2/3, else 7;
// - strings are read one byte at a time, never pre-allocated from the
//   declared length;
// - a delta component outside the signed 32-bit range drops the element.
//
// Owner: OASFMT (design D2).

#include "hefesto/oasis/codec/primitives.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <utility>

#include "internal.hpp"

namespace hefesto::oasis {

namespace detail {

namespace {

constexpr double kTwo53 = 9007199254740992.0;

/// The 3-delta and octangular g-delta directions: E N W S NE NW SW SE.
constexpr std::array<Point64, 8> kOctangular = {{{1, 0}, {0, 1}, {-1, 0}, {0, -1}, {1, 1}, {-1, 1}, {-1, -1}, {1, -1}}};

Point64 scaled(Point64 unit, std::uint64_t magnitude) {
  // magnitude < 2^62 (the value was shifted right at least twice).
  const auto m = static_cast<std::int64_t>(magnitude);
  return {unit.x * m, unit.y * m};
}

std::uint64_t read_fixed(Input& in, int bytes) {
  std::uint64_t value = 0;
  for (int i = 0; i < bytes; ++i) {
    value |= std::uint64_t{in.get()} << (8U * static_cast<unsigned>(i));
  }
  return value;
}

double ratio(double numerator, std::uint64_t divisor) {
  if (divisor == 0) {
    fail("OASIS-CODEC-006", "division by zero in real");
  }
  return numerator / static_cast<double>(divisor);
}

/// The direction code 0-7 of an octangular vector and its magnitude.
std::pair<std::uint64_t, std::uint64_t> octangular(Point64 d) {
  const std::uint64_t m = d.x != 0 ? static_cast<std::uint64_t>(std::llabs(d.x)) : static_cast<std::uint64_t>(std::llabs(d.y));
  const auto sx = (d.x > 0) - (d.x < 0);
  const auto sy = (d.y > 0) - (d.y < 0);
  for (std::uint64_t dir = 0; dir < kOctangular.size(); ++dir) {
    if (kOctangular[dir].x == sx && kOctangular[dir].y == sy) {
      return {dir, m};
    }
  }
  return {0, 0};  // the zero vector: east, magnitude 0
}

}  // namespace

void fail(std::string_view code, std::string text) {
  throw CodecError{diag::make(code, std::move(text))};
}

std::uint8_t CursorInput::get() {
  if (cursor_->offset >= cursor_->bytes.size()) {
    fail("OASIS-CODEC-002", "unexpected end of file");
  }
  return std::to_integer<std::uint8_t>(cursor_->bytes[cursor_->offset++]);
}

void Drop::set(std::string_view code, std::string text) {
  if (!diagnostic) {
    diagnostic = diag::make(code, std::move(text));
  }
}

std::int64_t add(std::int64_t a, std::int64_t b, Drop& drop) {
  std::int64_t sum = 0;
  if (__builtin_add_overflow(a, b, &sum)) {
    drop.set("OASIS-CODEC-008", "coordinate overflow");
    return 0;
  }
  return sum;
}

Point64 add(Point64 a, Point64 b, Drop& drop) {
  return {add(a.x, b.x, drop), add(a.y, b.y, drop)};
}

void check_delta(Point64 delta, Drop& drop) {
  constexpr auto lo = std::numeric_limits<std::int32_t>::min();
  constexpr auto hi = std::numeric_limits<std::int32_t>::max();
  if (delta.x < lo || delta.x > hi || delta.y < lo || delta.y > hi) {
    drop.set("OASIS-CODEC-008", "coordinate overflow");
  }
}

std::int64_t to_length(std::uint64_t value, Drop& drop) {
  if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    drop.set("OASIS-CODEC-008", "coordinate overflow");
    return 0;
  }
  return static_cast<std::int64_t>(value);
}

std::uint64_t read_unsigned(Input& in) {
  std::uint64_t value = 0;
  unsigned shift = 0;
  for (;;) {
    const std::uint8_t byte = in.get();
    const std::uint64_t group = byte & 0x7fU;
    // Groups at shifts 0..56 fit whole; at 63 only one bit fits; past it only zero groups.
    const bool fits = shift <= 56 || (shift == 63 ? group <= 1 : group == 0);
    if (!fits) {
      fail("OASIS-CODEC-005", "integer overflow");
    }
    if (shift < 64) {
      value |= group << shift;
      shift += 7;
    }
    if ((byte & 0x80U) == 0) {
      return value;
    }
  }
}

std::uint32_t read_u32(Input& in, std::string_view field) {
  const std::uint64_t value = read_unsigned(in);
  if (value > 0xffffffffU) {
    fail("OASIS-CODEC-005", "integer overflow in " + std::string{field});
  }
  return static_cast<std::uint32_t>(value);
}

std::int64_t read_signed(Input& in) {
  const std::uint64_t raw = read_unsigned(in);
  const auto magnitude = static_cast<std::int64_t>(raw >> 1U);
  return (raw & 1U) != 0 ? -magnitude : magnitude;
}

double read_real(Input& in) {
  return read_real_data(in, read_u32(in, "real type"));
}

double read_real_data(Input& in, std::uint64_t type) {
  switch (type) {
    case 0:
      return static_cast<double>(read_unsigned(in));
    case 1:
      return -static_cast<double>(read_unsigned(in));
    case 2:
      return ratio(1.0, read_unsigned(in));
    case 3:
      return -ratio(1.0, read_unsigned(in));
    case 4:
    case 5: {
      const auto numerator = static_cast<double>(read_unsigned(in));
      const double value = ratio(numerator, read_unsigned(in));
      return type == 4 ? value : -value;
    }
    case 6:
      return static_cast<double>(std::bit_cast<float>(static_cast<std::uint32_t>(read_fixed(in, 4))));
    case 7:
      return std::bit_cast<double>(read_fixed(in, 8));
    default:
      fail("OASIS-CODEC-006", "invalid real type " + std::to_string(type));
  }
}

std::string read_string(Input& in) {
  const std::uint64_t length = read_unsigned(in);
  std::string text;
  for (std::uint64_t i = 0; i < length; ++i) {
    text.push_back(static_cast<char>(in.get()));
  }
  return text;
}

void skip_string(Input& in) {
  const std::uint64_t length = read_unsigned(in);
  for (std::uint64_t i = 0; i < length; ++i) {
    (void)in.get();
  }
}

Point64 read_delta(Input& in, DeltaForm form) {
  switch (form) {
    case DeltaForm::one_horizontal:
      return {read_signed(in), 0};
    case DeltaForm::one_vertical:
      return {0, read_signed(in)};
    case DeltaForm::two: {
      const std::uint64_t u = read_unsigned(in);
      return scaled(kOctangular[u & 3U], u >> 2U);
    }
    case DeltaForm::three: {
      const std::uint64_t u = read_unsigned(in);
      return scaled(kOctangular[u & 7U], u >> 3U);
    }
    case DeltaForm::g:
      break;
  }
  const std::uint64_t u = read_unsigned(in);
  if ((u & 1U) == 0) {
    return scaled(kOctangular[(u >> 1U) & 7U], u >> 4U);
  }
  const auto magnitude = static_cast<std::int64_t>(u >> 2U);
  return {(u & 2U) != 0 ? -magnitude : magnitude, read_signed(in)};
}

Interval read_interval(Input& in) {
  const std::uint32_t type = read_u32(in, "interval type");
  Interval interval{static_cast<std::uint8_t>(type), 0, 0xffffffffU};
  switch (type) {
    case 0:
      break;
    case 1:
      interval.high = read_u32(in, "interval bound");
      break;
    case 2:
      interval.low = read_u32(in, "interval bound");
      break;
    case 3:
      interval.low = read_u32(in, "interval bound");
      interval.high = interval.low;
      break;
    case 4:
      interval.low = read_u32(in, "interval bound");
      interval.high = read_u32(in, "interval bound");
      break;
    default:
      fail("OASIS-CODEC-011", "invalid interval type " + std::to_string(type));
  }
  return interval;
}

std::size_t unsigned_length(std::uint64_t value) noexcept {
  std::size_t length = 1;
  while (value >= 0x80U) {
    value >>= 7U;
    ++length;
  }
  return length;
}

std::optional<diag::Diagnostic> check_string(StringKind kind, std::string_view text) {
  if (legal_string(kind, text)) {
    return std::nullopt;
  }
  const std::string_view name = kind == StringKind::a_string ? "a-string" : "n-string";
  return diag::make("OASIS-CODEC-007", "illegal character in " + std::string{name} + " \"" + std::string{text} + "\"");
}

}  // namespace detail

namespace {

/// Runs a throwing primitive decoder over `cursor`; on error the cursor is
/// left where the error was found.
template <typename F>
auto run(ByteCursor& cursor, F decode) -> diag::Outcome<decltype(decode(std::declval<detail::Input&>()))> {
  detail::CursorInput in{cursor};
  try {
    return decode(in);
  } catch (const detail::CodecError& error) {
    return error.diagnostic;
  }
}

/// Runs a decoder that may drop the element; a drop becomes the error.
template <typename F>
auto run_dropping(ByteCursor& cursor, F decode)
    -> diag::Outcome<decltype(decode(std::declval<detail::Input&>(), std::declval<detail::Drop&>()))> {
  detail::Drop drop;
  auto result = run(cursor, [&](detail::Input& in) { return decode(in, drop); });
  if (result.ok() && drop.dropped()) {
    return *drop.diagnostic;
  }
  return result;
}

}  // namespace

bool legal_string(StringKind kind, std::string_view text) noexcept {
  if (kind == StringKind::b_string) {
    return true;
  }
  const unsigned char low = kind == StringKind::n_string ? 0x21 : 0x20;
  if (kind == StringKind::n_string && text.empty()) {
    return false;
  }
  for (const char c : text) {
    const auto byte = static_cast<unsigned char>(c);
    if (byte < low || byte > 0x7e) {
      return false;
    }
  }
  return true;
}

diag::Outcome<std::uint64_t> read_unsigned(ByteCursor& cursor) {
  return run(cursor, [](detail::Input& in) { return detail::read_unsigned(in); });
}

diag::Outcome<std::int64_t> read_signed(ByteCursor& cursor) {
  return run(cursor, [](detail::Input& in) { return detail::read_signed(in); });
}

diag::Outcome<double> read_real(ByteCursor& cursor) {
  return run(cursor, [](detail::Input& in) { return detail::read_real(in); });
}

diag::Outcome<std::string> read_string(ByteCursor& cursor) {
  return run(cursor, [](detail::Input& in) { return detail::read_string(in); });
}

diag::Outcome<Point64> read_delta(ByteCursor& cursor, DeltaForm form) {
  return run_dropping(cursor, [form](detail::Input& in, detail::Drop& drop) {
    const Point64 delta = detail::read_delta(in, form);
    detail::check_delta(delta, drop);
    return delta;
  });
}

diag::Outcome<PointList> read_point_list(ByteCursor& cursor) {
  return run_dropping(cursor, [](detail::Input& in, detail::Drop& drop) { return detail::read_point_list(in, drop); });
}

diag::Outcome<Repetition> read_repetition(ByteCursor& cursor) {
  return run_dropping(cursor, [](detail::Input& in, detail::Drop& drop) {
    return detail::read_repetition(in, std::nullopt, drop);
  });
}

diag::Outcome<Interval> read_interval(ByteCursor& cursor) {
  return run(cursor, [](detail::Input& in) { return detail::read_interval(in); });
}

void write_unsigned(std::vector<std::byte>& out, std::uint64_t value) {
  while (value >= 0x80U) {
    out.push_back(static_cast<std::byte>((value & 0x7fU) | 0x80U));
    value >>= 7U;
  }
  out.push_back(static_cast<std::byte>(value));
}

void write_signed(std::vector<std::byte>& out, std::int64_t value) {
  const std::uint64_t magnitude = value < 0 ? 0 - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);
  write_unsigned(out, (magnitude << 1U) | (value < 0 ? 1U : 0U));
}

void write_real(std::vector<std::byte>& out, double value) {
  const double magnitude = std::fabs(value);
  if (std::isfinite(value) && std::trunc(value) == value && magnitude < detail::kTwo53) {
    write_unsigned(out, value < 0 ? 1 : 0);
    write_unsigned(out, static_cast<std::uint64_t>(magnitude));
    return;
  }
  const double inverse = 1.0 / value;
  if (std::isfinite(inverse) && std::trunc(inverse) == inverse && std::fabs(inverse) < detail::kTwo53 &&
      1.0 / inverse == value) {
    write_unsigned(out, value < 0 ? 3 : 2);
    write_unsigned(out, static_cast<std::uint64_t>(std::fabs(inverse)));
    return;
  }
  write_unsigned(out, 7);
  const auto bits = std::bit_cast<std::uint64_t>(value);
  for (unsigned i = 0; i < 8; ++i) {
    out.push_back(static_cast<std::byte>((bits >> (8U * i)) & 0xffU));
  }
}

void write_string(std::vector<std::byte>& out, std::string_view text) {
  write_unsigned(out, text.size());
  for (const char c : text) {
    out.push_back(static_cast<std::byte>(c));
  }
}

void write_delta(std::vector<std::byte>& out, Point64 delta, DeltaForm form) {
  switch (form) {
    case DeltaForm::one_horizontal:
      write_signed(out, delta.x);
      return;
    case DeltaForm::one_vertical:
      write_signed(out, delta.y);
      return;
    case DeltaForm::two: {
      const auto [dir, m] = detail::octangular(delta);
      write_unsigned(out, (m << 2U) | dir);
      return;
    }
    case DeltaForm::three: {
      const auto [dir, m] = detail::octangular(delta);
      write_unsigned(out, (m << 3U) | dir);
      return;
    }
    case DeltaForm::g:
      break;
  }
  if (delta.x == 0 || delta.y == 0 || std::llabs(delta.x) == std::llabs(delta.y)) {
    const auto [dir, m] = detail::octangular(delta);
    write_unsigned(out, (m << 4U) | (dir << 1U));
    return;
  }
  const auto mx = static_cast<std::uint64_t>(std::llabs(delta.x));
  write_unsigned(out, (mx << 2U) | (delta.x < 0 ? 2U : 0U) | 1U);
  write_signed(out, delta.y);
}

}  // namespace hefesto::oasis
