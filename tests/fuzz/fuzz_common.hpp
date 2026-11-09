// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Small helpers shared by the libFuzzer targets.

#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace hefesto::fuzz {

inline std::string_view as_text(const std::uint8_t* data, std::size_t size) noexcept {
  return {reinterpret_cast<const char*>(data), size};  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

/// The first byte selects a mode; the rest is the input proper.
struct Split {
  std::uint8_t mode = 0;
  std::string_view rest;
};

inline Split split_mode(std::string_view text) noexcept {
  if (text.empty()) {
    return {};
  }
  return {static_cast<std::uint8_t>(text.front()), text.substr(1)};
}

/// A file of the source tree (HEFESTO_SOURCE_DIR is set by tests/fuzz/CMakeLists.txt).
inline std::string source_file(std::string_view relative) {
  std::ifstream in{std::string{HEFESTO_SOURCE_DIR} + "/" + std::string{relative}, std::ios::binary};
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

}  // namespace hefesto::fuzz
