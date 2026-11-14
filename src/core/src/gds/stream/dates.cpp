// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GDSII dates and POSIX TZ zones (see hefesto/gds/stream/dates.hpp). The
// host-zone path ports Magic 8.3.694 calma/CalmaRdio.c calmaReadStampRecord
// (mktime) and calma/CalmaWrite.c calmaOutDate (localtime); Magic is (c)
// the Regents of the University of California, distributed under its
// permissive license (THIRD_PARTY_NOTICES.md). Civil-date arithmetic
// follows the public-domain days_from_civil/civil_from_days algorithms.
// Owner: GDSR.

#include "hefesto/gds/stream/dates.hpp"

#include <cctype>
#include <ctime>
#include <limits>

namespace hefesto::gds {

namespace {

constexpr std::int64_t kDay = 86400;
// About 35 million years: beyond it the civil arithmetic could overflow.
constexpr std::int64_t kMaxSeconds = std::int64_t{1} << 50;

std::int64_t floor_div(std::int64_t a, std::int64_t b) {
  const std::int64_t q = a / b;
  return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}

/// Days since 1970-01-01 of the proleptic Gregorian date y-m-d (m 1-12).
std::int64_t days_from_civil(std::int64_t y, std::int64_t m, std::int64_t d) {
  y -= m <= 2 ? 1 : 0;
  const std::int64_t era = floor_div(y, 400);
  const std::int64_t yoe = y - era * 400;
  const std::int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const std::int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

struct Civil {
  std::int64_t year;
  std::int64_t month;  // 1-12
  std::int64_t day;
};

Civil civil_from_days(std::int64_t z) {
  z += 719468;
  const std::int64_t era = floor_div(z, 146097);
  const std::int64_t doe = z - era * 146097;
  const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const std::int64_t mp = (5 * doy + 2) / 153;
  const std::int64_t d = doy - (153 * mp + 2) / 5 + 1;
  const std::int64_t m = mp < 10 ? mp + 3 : mp - 9;
  return Civil{yoe + era * 400 + (m <= 2 ? 1 : 0), m, d};
}

bool is_leap(std::int64_t y) {
  return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

/// Weekday (0 = Sunday) of a day count since the epoch (a Thursday).
std::int64_t weekday(std::int64_t days) {
  return days + 4 - floor_div(days + 4, 7) * 7;
}

/// Seconds since the epoch of local midnight (no offset) of a transition
/// date in `year`.
std::int64_t transition_day(const TimeZone::Transition& t, std::int64_t year) {
  using Kind = TimeZone::Transition::Kind;
  const std::int64_t jan1 = days_from_civil(year, 1, 1);
  if (t.kind == Kind::zero_based) {
    return jan1 + t.day;
  }
  if (t.kind == Kind::julian_no_leap) {
    const bool after_feb = t.day >= 60 && is_leap(year);
    return jan1 + t.day - 1 + (after_feb ? 1 : 0);
  }
  const std::int64_t first = days_from_civil(year, t.month, 1);
  std::int64_t day = first + (t.day - weekday(first) + 7) % 7;
  day += 7 * (t.week - 1);
  const std::int64_t next_month = t.month == 12 ? days_from_civil(year + 1, 1, 1) : days_from_civil(year, t.month + 1, 1);
  while (day >= next_month) {
    day -= 7;
  }
  return day;
}

// ---- POSIX TZ parsing -------------------------------------------------------

class TzParser {
 public:
  explicit TzParser(std::string_view text) : text_(text) {}

  [[nodiscard]] bool at_end() const { return pos_ >= text_.size(); }
  [[nodiscard]] char peek() const { return at_end() ? '\0' : text_[pos_]; }
  bool accept(char c) {
    if (peek() != c) {
      return false;
    }
    ++pos_;
    return true;
  }

  /// A zone name: <...> or three or more letters.
  bool name() {
    if (accept('<')) {
      const std::size_t close = text_.find('>', pos_);
      if (close == std::string_view::npos || close - pos_ < 3) {
        return false;
      }
      pos_ = close + 1;
      return true;
    }
    const std::size_t start = pos_;
    while (!at_end() && std::isalpha(static_cast<unsigned char>(peek())) != 0) {
      ++pos_;
    }
    return pos_ - start >= 3;
  }

  /// An unsigned decimal number of at most `digits` digits.
  std::optional<std::int64_t> number(int digits) {
    std::int64_t value = 0;
    int count = 0;
    while (count < digits && std::isdigit(static_cast<unsigned char>(peek())) != 0) {
      value = value * 10 + (text_[pos_++] - '0');
      ++count;
    }
    return count == 0 ? std::nullopt : std::optional{value};
  }

  /// [+|-]hh[:mm[:ss]] in seconds, positive as written.
  std::optional<std::int64_t> clock(std::int64_t max_hours) {
    const bool negative = accept('-');
    if (!negative) {
      accept('+');
    }
    const auto hours = number(3);
    if (!hours || *hours > max_hours) {
      return std::nullopt;
    }
    std::int64_t seconds = *hours * 3600;
    for (const std::int64_t unit : {std::int64_t{60}, std::int64_t{1}}) {
      if (!accept(':')) {
        break;
      }
      const auto part = number(2);
      if (!part || *part > 59) {
        return std::nullopt;
      }
      seconds += *part * unit;
    }
    return negative ? -seconds : seconds;
  }

  std::optional<TimeZone::Transition> transition() {
    TimeZone::Transition t;
    if (accept('M')) {
      if (!month_week_day(t)) {
        return std::nullopt;
      }
    } else if (!day_number(t)) {
      return std::nullopt;
    }
    if (accept('/')) {
      const auto time = clock(167);
      if (!time) {
        return std::nullopt;
      }
      t.time = *time;
    }
    return t;
  }

 private:
  bool month_week_day(TimeZone::Transition& t) {
    const auto m = number(2);
    if (!m || *m < 1 || *m > 12 || !accept('.')) {
      return false;
    }
    const auto w = number(1);
    if (!w || *w < 1 || *w > 5 || !accept('.')) {
      return false;
    }
    const auto d = number(1);
    if (!d || *d > 6) {
      return false;
    }
    t.kind = TimeZone::Transition::Kind::month_week_day;
    t.month = static_cast<int>(*m);
    t.week = static_cast<int>(*w);
    t.day = static_cast<int>(*d);
    return true;
  }

  bool day_number(TimeZone::Transition& t) {
    const bool julian = accept('J');
    const auto n = number(3);
    if (!n || (julian && (*n < 1 || *n > 365)) || (!julian && *n > 365)) {
      return false;
    }
    t.kind = julian ? TimeZone::Transition::Kind::julian_no_leap : TimeZone::Transition::Kind::zero_based;
    t.day = static_cast<int>(*n);
    return true;
  }

  std::string_view text_;
  std::size_t pos_ = 0;
};

// The rule glibc applies to a zone with a DST name and no rule (US rules).
constexpr TimeZone::Transition kDefaultStart{TimeZone::Transition::Kind::month_week_day, 0, 2, 3, 7200};
constexpr TimeZone::Transition kDefaultEnd{TimeZone::Transition::Kind::month_week_day, 0, 1, 11, 7200};

/// The daylight-saving part of a POSIX TZ string: name [offset] [,start,end].
struct DstPart {
  std::int64_t east = 0;
  TimeZone::Transition start = kDefaultStart;
  TimeZone::Transition end = kDefaultEnd;
};

std::optional<DstPart> parse_dst(TzParser& p, std::int64_t std_east) {
  if (!p.name()) {
    return std::nullopt;
  }
  DstPart dst{std_east + 3600};
  if (!p.at_end() && p.peek() != ',') {
    const auto offset = p.clock(24);
    if (!offset) {
      return std::nullopt;
    }
    dst.east = -*offset;
  }
  if (p.accept(',')) {
    const auto start = p.transition();
    const auto end = start && p.accept(',') ? p.transition() : std::nullopt;
    if (!end) {
      return std::nullopt;
    }
    dst.start = *start;
    dst.end = *end;
  }
  return p.at_end() ? std::optional{dst} : std::nullopt;
}

}  // namespace

std::optional<TimeZone> TimeZone::parse(std::string_view text) {
  TzParser p{text};
  if (!p.name()) {
    return std::nullopt;
  }
  TimeZone zone;
  if (p.at_end()) {
    return zone;  // a bare name ("UTC", "GMT") has no offset
  }
  const auto std_offset = p.clock(24);
  if (!std_offset) {
    return std::nullopt;
  }
  zone.std_east_ = -*std_offset;
  zone.dst_east_ = zone.std_east_;
  if (p.at_end()) {
    return zone;
  }
  const auto dst = parse_dst(p, zone.std_east_);
  if (!dst) {
    return std::nullopt;
  }
  zone.has_dst_ = true;
  zone.dst_east_ = dst->east;
  zone.start_ = dst->start;
  zone.end_ = dst->end;
  return zone;
}

TimeZone TimeZone::parse_or_utc(std::string_view text) {
  return parse(text).value_or(TimeZone{});
}

bool TimeZone::in_dst(std::int64_t utc) const {
  if (!has_dst_) {
    return false;
  }
  const std::int64_t year = civil_from_days(floor_div(utc + std_east_, kDay)).year;
  // The start is given in standard local time, the end in daylight time.
  const std::int64_t start = transition_day(start_, year) * kDay + start_.time - std_east_;
  const std::int64_t end = transition_day(end_, year) * kDay + end_.time - dst_east_;
  if (start < end) {
    return utc >= start && utc < end;
  }
  return !(utc >= end && utc < start);  // southern hemisphere: DST spans the new year
}

std::int64_t TimeZone::offset_at(std::int64_t utc) const {
  return in_dst(utc) ? dst_east_ : std_east_;
}

LocalTime TimeZone::to_local(std::int64_t utc) const {
  const std::int64_t local = utc + offset_at(utc);
  const std::int64_t days = floor_div(local, kDay);
  const std::int64_t secs = local - days * kDay;
  const Civil c = civil_from_days(days);
  return LocalTime{c.year - 1900, c.month - 1, c.day, secs / 3600, (secs % 3600) / 60, secs % 60};
}

std::int64_t TimeZone::from_local(const LocalTime& local) const {
  const std::int64_t year = 1900 + local.year + floor_div(local.month, 12);
  const std::int64_t month = local.month - floor_div(local.month, 12) * 12;
  const std::int64_t days = days_from_civil(year, month + 1, 1) + local.day - 1;
  const std::int64_t wall = days * kDay + local.hour * 3600 + local.minute * 60 + local.second;
  const std::int64_t as_std = wall - std_east_;
  if (!has_dst_) {
    return as_std;
  }
  const std::int64_t as_dst = wall - dst_east_;
  if (in_dst(as_dst)) {
    return as_dst;  // daylight time, or the first of two ambiguous instants
  }
  return as_std;
}

std::optional<LocalTime> to_local_time(std::int64_t utc, std::string_view zone) {
  if (utc < -kMaxSeconds || utc > kMaxSeconds) {
    return std::nullopt;
  }
  if (!zone.empty()) {
    return TimeZone::parse_or_utc(zone).to_local(utc);
  }
  if (utc < std::numeric_limits<std::time_t>::min() || utc > std::numeric_limits<std::time_t>::max()) {
    return std::nullopt;
  }
  const std::time_t t = utc;  // in range (checked above); same type on LP64 Linux
  std::tm tm{};
#if defined(_WIN32)
  if (localtime_s(&tm, &t) != 0) {
    return std::nullopt;
  }
#else
  if (localtime_r(&t, &tm) == nullptr) {
    return std::nullopt;
  }
#endif
  return LocalTime{tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec};
}

std::int64_t from_local_time(const LocalTime& local, std::string_view zone) {
  if (!zone.empty()) {
    return TimeZone::parse_or_utc(zone).from_local(local);
  }
  std::tm tm{};
  tm.tm_year = static_cast<int>(local.year);
  tm.tm_mon = static_cast<int>(local.month);
  tm.tm_mday = static_cast<int>(local.day);
  tm.tm_hour = static_cast<int>(local.hour);
  tm.tm_min = static_cast<int>(local.minute);
  tm.tm_sec = static_cast<int>(local.second);
  tm.tm_isdst = -1;
  return std::mktime(&tm);  // Magic keeps mktime's -1 on error too
}

std::int64_t decode_date(std::span<const std::int16_t, 6> fields, std::string_view zone, bool magic_compat) {
  std::int64_t year = fields[0];
  if (!magic_compat && year >= 1900) {
    year -= 1900;  // a full year (Q-MASK-134 OFF)
  }
  const LocalTime local{year, fields[1] - 1, fields[2], fields[3], fields[4], fields[5]};
  const std::int64_t t = from_local_time(local, zone);
  if (magic_compat) {
    // (int)mktime(...): the low 32 bits, two's complement.
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(t) & 0xffffffffU));
  }
  return t;
}

}  // namespace hefesto::gds
