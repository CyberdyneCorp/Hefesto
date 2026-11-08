// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/platform/logger.hpp"

#include <algorithm>
#include <cstdio>
#include <utility>

#include "hefesto/diag/format.hpp"

namespace hefesto::platform {

namespace {

// Days since 1970-01-01 to a civil date (H. Hinnant's algorithm), so the
// timestamp needs neither the C time-zone database nor gmtime's static state.
struct CivilDate {
  long long year;
  unsigned month;
  unsigned day;
};

CivilDate civil_from_days(long long z) {
  z += 719468;
  const long long era = (z >= 0 ? z : z - 146096) / 146097;
  const auto doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  const unsigned day = doy - (153 * mp + 2) / 5 + 1;
  const unsigned month = mp < 10 ? mp + 3 : mp - 9;
  const long long year = static_cast<long long>(yoe) + era * 400 + (month <= 2 ? 1 : 0);
  return {year, month, day};
}

}  // namespace

std::string format_utc_timestamp(std::chrono::system_clock::time_point time) {
  using namespace std::chrono;
  const auto ms = floor<milliseconds>(time).time_since_epoch().count();
  const long long ms_per_day = 86'400'000;
  const long long days = (ms >= 0 ? ms : ms - ms_per_day + 1) / ms_per_day;
  const long long in_day = ms - days * ms_per_day;
  const CivilDate date = civil_from_days(days);
  char buffer[192];  // room for any long long year, so GCC proves no truncation
  std::snprintf(buffer, sizeof buffer, "%04lld-%02u-%02uT%02lld:%02lld:%02lld.%03lldZ", date.year, date.month,
                date.day, in_day / 3'600'000, in_day / 60'000 % 60, in_day / 1000 % 60, in_day % 1000);
  return buffer;
}

void Logger::log(const diag::Diagnostic& diagnostic) {
  write(diagnostic.severity, diag::format_text(diagnostic));
}

void MemoryLogger::write(diag::Severity level, std::string_view message) {
  const std::scoped_lock lock{mutex_};
  lines_.push_back(std::string{diag::severity_name(level)} + " " + std::string{message});
}

std::vector<std::string> MemoryLogger::lines() const {
  const std::scoped_lock lock{mutex_};
  return lines_;
}

RotatingFileLogger::RotatingFileLogger(FileSystem& fs, Clock& clock, Path directory, std::string base_name,
                                       RotationPolicy policy)
    : fs_(fs), clock_(clock), directory_(std::move(directory)), base_name_(std::move(base_name)), policy_(policy) {
  policy_.max_files = std::max<std::uint32_t>(policy_.max_files, 1);
}

Path RotatingFileLogger::numbered_path(std::uint32_t index) const {
  if (index == 0) {
    return directory_ / (base_name_ + ".log");
  }
  return directory_ / (base_name_ + "." + std::to_string(index) + ".log");
}

Path RotatingFileLogger::current_path() const {
  return numbered_path(0);
}

std::optional<diag::Diagnostic> RotatingFileLogger::last_error() const {
  const std::scoped_lock lock{mutex_};
  return last_error_;
}

void RotatingFileLogger::record_failure_locked(const diag::Status& status) {
  if (!status.ok() && !last_error_) {
    last_error_ = status.error();
  }
}

void RotatingFileLogger::rotate_locked() {
  const std::uint32_t oldest = policy_.max_files - 1;
  if (oldest == 0) {
    record_failure_locked(fs_.remove(numbered_path(0)));
    return;
  }
  record_failure_locked(fs_.remove(numbered_path(oldest)));
  for (std::uint32_t i = oldest; i > 0; --i) {
    if (fs_.exists(numbered_path(i - 1))) {
      record_failure_locked(fs_.rename(numbered_path(i - 1), numbered_path(i)));
    }
  }
}

void RotatingFileLogger::write(diag::Severity level, std::string_view message) {
  std::string line = format_utc_timestamp(clock_.wall_now());
  line += ' ';
  line += diag::severity_name(level);
  line += ' ';
  line += message;
  line += '\n';

  const std::scoped_lock lock{mutex_};
  const auto current = fs_.size(current_path());
  if (current.ok() && current.value() > 0 && current.value() + line.size() > policy_.max_bytes) {
    rotate_locked();
  }
  record_failure_locked(fs_.append(current_path(), line));
}

}  // namespace hefesto::platform
