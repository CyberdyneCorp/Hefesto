// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A temporary directory removed at the end of a test, plus small file helpers.

#pragma once

#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <string_view>

namespace hefesto::test {

class TempDir {
 public:
  TempDir() {
    std::random_device rd;
    path_ = std::filesystem::temp_directory_path() / ("hefesto-app-test-" + std::to_string(rd()) + std::to_string(rd()));
    std::filesystem::create_directories(path_);
    path_ = std::filesystem::canonical(path_);
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;
  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }
  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

 private:
  std::filesystem::path path_;
};

inline void write_file(const std::filesystem::path& path, std::string_view text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out{path, std::ios::binary | std::ios::trunc};
  out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

inline std::string read_file(const std::filesystem::path& path) {
  std::ifstream in{path, std::ios::binary};
  std::stringstream s;
  s << in.rdbuf();
  return s.str();
}

}  // namespace hefesto::test
