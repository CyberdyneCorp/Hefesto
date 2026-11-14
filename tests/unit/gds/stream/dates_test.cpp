// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GDSII dates (tasks.md 9.1): BGNLIB/BGNSTR date fields in local time
// (Q-MASK-203) with the pinned-zone override (Q-MASK-132): UTC, a fixed
// non-UTC offset and zones with daylight saving time in both hemispheres;
// the POSIX TZ parser against the C library. Owner: GDSR.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <string>

#include "hefesto/gds/stream/dates.hpp"
#include "hefesto/gds/stream/encode.hpp"

namespace gds = hefesto::gds;

namespace {

using Fields = std::array<std::int16_t, 6>;

}  // namespace

TEST_CASE("dates encode in UTC as year-1900, month, day, hour, minute, second", "[cap:gds-write][req:Date records]") {
  CHECK(gds::encode_date(1700000000, "UTC0") == Fields{123, 11, 14, 22, 13, 20});  // 00 7B 00 0B 00 0E 00 16 00 0D 00 14
  CHECK(gds::encode_date(1600000000, "UTC0") == Fields{120, 9, 13, 12, 26, 40});   // 00 78 00 09 00 0D 00 0C 00 1A 00 28
  CHECK(gds::encode_date(0, "UTC0") == Fields{70, 1, 1, 0, 0, 0});                 // 00 46 00 01 00 01 00 00 ...
  CHECK(gds::encode_date(0, "UTC") == Fields{70, 1, 1, 0, 0, 0});
  CHECK(gds::encode_date(-1, "UTC0") == Fields{69, 12, 31, 23, 59, 59});
  CHECK(gds::encode_date(951782400, "UTC0") == Fields{100, 2, 29, 0, 0, 0});  // 2000-02-29
}

TEST_CASE("dates encode in a pinned non-UTC zone", "[cap:gds-write][req:Date records]") {
  // A fixed offset west of UTC: t = 0 falls on 1969-12-31.
  CHECK(gds::encode_date(0, "<-03>3") == Fields{69, 12, 31, 21, 0, 0});
  CHECK(gds::encode_date(1700000000, "<-03>3") == Fields{123, 11, 14, 19, 13, 20});
  // US Eastern: standard time in November, daylight time in July.
  const char* eastern = "EST5EDT,M3.2.0,M11.1.0";
  CHECK(gds::encode_date(0, eastern) == Fields{69, 12, 31, 19, 0, 0});
  CHECK(gds::encode_date(1700000000, eastern) == Fields{123, 11, 14, 17, 13, 20});
  CHECK(gds::encode_date(1690000000, eastern) == Fields{123, 7, 22, 0, 26, 40});
  // The transitions of 2023: 2023-03-12 07:00 UTC and 2023-11-05 06:00 UTC.
  CHECK(gds::encode_date(1678604399, eastern) == Fields{123, 3, 12, 1, 59, 59});
  CHECK(gds::encode_date(1678604400, eastern) == Fields{123, 3, 12, 3, 0, 0});
  CHECK(gds::encode_date(1699163999, eastern) == Fields{123, 11, 5, 1, 59, 59});
  CHECK(gds::encode_date(1699164000, eastern) == Fields{123, 11, 5, 1, 0, 0});
  // Without a rule a DST zone uses the US rules, as glibc does.
  CHECK(gds::encode_date(1690000000, "EST5EDT") == Fields{123, 7, 22, 0, 26, 40});
  // Southern hemisphere: daylight time spans the new year.
  const char* sydney = "AEST-10AEDT,M10.1.0,M4.1.0/3";
  CHECK(gds::encode_date(1700000000, sydney) == Fields{123, 11, 15, 9, 13, 20});
  CHECK(gds::encode_date(1690000000, sydney) == Fields{123, 7, 22, 14, 26, 40});
}

TEST_CASE("an unparsable zone is UTC, as glibc treats an invalid TZ", "[cap:gds-write][req:Date records]") {
  CHECK_FALSE(gds::TimeZone::parse("").has_value());
  CHECK_FALSE(gds::TimeZone::parse("X5").has_value());
  CHECK_FALSE(gds::TimeZone::parse("EST5EDT,M13.1.0,M11.1.0").has_value());
  CHECK_FALSE(gds::TimeZone::parse("EST99").has_value());
  CHECK_FALSE(gds::TimeZone::parse("<EST5").has_value());
  CHECK(gds::TimeZone::parse("EST5EDT4,J60/2,300/2").has_value());
  CHECK(gds::encode_date(1700000000, "America/Nowhere") == Fields{123, 11, 14, 22, 13, 20});
}

TEST_CASE("dates outside the representable range encode as zeros", "[cap:gds-write][req:Date records]") {
  CHECK(gds::encode_date(INT64_MAX, "UTC0") == Fields{});
  CHECK(gds::encode_date(INT64_MIN, "") == Fields{});
}

TEST_CASE("a pinned zone converts local times back as mktime does", "[cap:gds-read][req:Host mechanisms used by the reader]") {
  const gds::TimeZone eastern = gds::TimeZone::parse_or_utc("EST5EDT,M3.2.0,M11.1.0");
  CHECK(eastern.from_local(gds::LocalTime{123, 6, 22, 0, 26, 40}) == 1690000000);
  CHECK(eastern.from_local(gds::LocalTime{123, 10, 14, 17, 13, 20}) == 1700000000);
  // Out-of-range fields are normalized: month 12 of 2022 is January 2023.
  CHECK(eastern.from_local(gds::LocalTime{122, 12, 1, 0, 0, 0}) == eastern.from_local(gds::LocalTime{123, 0, 1, 0, 0, 0}));
  // The repeated hour of November 5 is read as daylight time, the skipped hour of March 12 as standard time.
  CHECK(eastern.from_local(gds::LocalTime{123, 10, 5, 1, 30, 0}) == 1699162200);
  CHECK(eastern.from_local(gds::LocalTime{123, 2, 12, 2, 30, 0}) == 1678606200);
  CHECK(eastern.offset_at(1690000000) == -4 * 3600);
  CHECK(eastern.offset_at(1700000000) == -5 * 3600);
}

#if !defined(_WIN32)
TEST_CASE("the POSIX zone conversion agrees with the C library", "[cap:gds-read][req:Host mechanisms used by the reader]") {
  const char* saved = std::getenv("TZ");
  const std::optional<std::string> old = saved != nullptr ? std::optional<std::string>{saved} : std::nullopt;
  for (const char* zone : {"UTC0", "<-03>3", "EST5EDT,M3.2.0,M11.1.0", "AEST-10AEDT,M10.1.0,M4.1.0/3",
                           "CET-1CEST,M3.5.0,M10.5.0/3", "<+0530>-5:30"}) {
    INFO(zone);
    setenv("TZ", zone, 1);
    tzset();
    std::uint64_t state = 7;
    for (int i = 0; i < 2000; ++i) {
      state = state * 6364136223846793005ULL + 1442695040888963407ULL;
      // From 1971 on: before it the C libraries differ (tzcode applies the rule only from
      // its first transition after 1970, glibc to every year, as the pinned zone does).
      const auto t = std::max<std::int64_t>(static_cast<std::int64_t>((state >> 33U) & 0x7fffffffU), 31536000);
      const auto host = gds::to_local_time(t, "");  // localtime_r under TZ
      REQUIRE(host.has_value());
      CHECK(gds::to_local_time(t, zone) == host);
    }
  }
  if (old) {
    setenv("TZ", old->c_str(), 1);
  } else {
    unsetenv("TZ");
  }
  tzset();
}
#endif
