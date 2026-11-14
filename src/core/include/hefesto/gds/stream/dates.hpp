// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GDSII date fields and the time zone they are converted in (capabilities
// gds-read "Structure timestamps", "Host mechanisms used by the reader",
// gds-write "Date records"; openspec/changes/m5-mask-gds-cif/design.md
// D10, D12; Q-MASK-132, 134, 203).
//
// Magic converts with localtime() and mktime() in the host's zone. Hefesto
// does the same when no zone is pinned. A pinned zone (the Q-MASK-132
// override, used by golden captures and tests) is a POSIX TZ string such as
// "UTC0", "EST5EDT,M3.2.0,M11.1.0" or "<-03>3", converted here without the
// process-wide TZ variable, so pinning is thread-safe and identical on
// every host. A zone string that is not a valid POSIX TZ string is UTC, as
// glibc treats an invalid TZ.
//
// Owner: GDSR (design D2).

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace hefesto::gds {

/// Broken-down local time as struct tm holds it: `year` is years since
/// 1900, `month` 0-11; out-of-range fields are normalized by from_local.
struct LocalTime {
  std::int64_t year = 70;
  std::int64_t month = 0;
  std::int64_t day = 1;
  std::int64_t hour = 0;
  std::int64_t minute = 0;
  std::int64_t second = 0;

  friend bool operator==(const LocalTime&, const LocalTime&) = default;
};

/// A POSIX TZ rule: standard offset and an optional daylight-saving rule.
class TimeZone {
 public:
  /// UTC.
  TimeZone() = default;

  /// The zone of a POSIX TZ string; nullopt when it does not parse.
  [[nodiscard]] static std::optional<TimeZone> parse(std::string_view text);
  /// parse(text), or UTC when it does not parse (glibc's behavior).
  [[nodiscard]] static TimeZone parse_or_utc(std::string_view text);

  /// Seconds east of UTC in effect at `utc` (seconds since the epoch).
  [[nodiscard]] std::int64_t offset_at(std::int64_t utc) const;
  /// localtime(): `utc` as local broken-down time.
  [[nodiscard]] LocalTime to_local(std::int64_t utc) const;
  /// mktime() with tm_isdst = -1: the instant of `local`. A time in the
  /// spring-forward gap is read as standard time, an ambiguous time in the
  /// fall-back hour as daylight time.
  [[nodiscard]] std::int64_t from_local(const LocalTime& local) const;

  /// One transition date of a daylight-saving rule (POSIX "Jn", "n" or
  /// "Mm.w.d") and its local time of day in seconds.
  struct Transition {
    enum class Kind : std::uint8_t { julian_no_leap, zero_based, month_week_day };
    Kind kind = Kind::month_week_day;
    int day = 0;     // Jn: 1-365; n: 0-365; Mm.w.d: d (0 = Sunday)
    int week = 0;    // Mm.w.d: w (1-5, 5 = last)
    int month = 0;   // Mm.w.d: m (1-12)
    std::int64_t time = 7200;
  };

 private:
  [[nodiscard]] bool in_dst(std::int64_t utc) const;

  std::int64_t std_east_ = 0;
  std::int64_t dst_east_ = 0;
  bool has_dst_ = false;
  Transition start_{};
  Transition end_{};
};

/// localtime(): `utc` in the host zone when `zone` is empty, else in the
/// POSIX zone `zone`; nullopt when the host cannot represent it or |utc|
/// exceeds 2^50 seconds.
[[nodiscard]] std::optional<LocalTime> to_local_time(std::int64_t utc, std::string_view zone);
/// mktime() with tm_isdst = -1 in the host zone or `zone` (-1 when the host
/// cannot represent it, as mktime answers).
[[nodiscard]] std::int64_t from_local_time(const LocalTime& local, std::string_view zone);

/// The creation time of a BGNSTR date (year, month, day, hour, minute,
/// second as written; month 1-based), converted in the host zone or
/// `zone`. With magic-compat ON the year is years since 1900 and the result
/// keeps its low 32 bits (Magic's int); with it OFF a year of 1900 or more
/// is a full year and the result is not truncated (Q-MASK-134).
[[nodiscard]] std::int64_t decode_date(std::span<const std::int16_t, 6> fields, std::string_view zone,
                                       bool magic_compat);

}  // namespace hefesto::gds
