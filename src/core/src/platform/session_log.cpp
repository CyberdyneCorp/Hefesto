// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/platform/session_log.hpp"

#include <cstdlib>
#include <memory>
#include <system_error>

namespace hefesto::platform {

namespace {

bool is_set(const char* value) noexcept {
  return value != nullptr && *value != '\0';
}

}  // namespace

Path default_storage_dir() {
  const char* home = std::getenv("HOME");
#if defined(__APPLE__)
  if (is_set(home)) {
    return Path{home} / "Library" / "Application Support" / "Hefesto";
  }
#elif defined(__linux__)
  if (const char* state = std::getenv("XDG_STATE_HOME"); is_set(state)) {
    return Path{state} / "hefesto";
  }
  if (is_set(home)) {
    return Path{home} / ".local" / "state" / "hefesto";
  }
#endif
  (void)home;
  std::error_code ec;
  const Path temp = std::filesystem::temp_directory_path(ec);
  return (ec ? Path{"."} : temp) / "hefesto";
}

SessionLog install_session_log(PortableServices& services, const Path& directory, std::string_view base_name,
                               std::string_view warning_code) {
  if (auto made = services.files().create_directories(directory); !made) {
    return {nullptr, diag::make(warning_code, "session log disabled: " + made.error().text)};
  }
  auto logger = std::make_unique<RotatingFileLogger>(services.files(), services.clock(), directory,
                                                     std::string{base_name}, kSessionLogRotation);
  Logger* installed = logger.get();
  services.set_logger(std::move(logger));
  return {installed, std::nullopt};
}

}  // namespace hefesto::platform
