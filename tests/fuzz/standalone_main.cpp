// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Replays fuzz inputs without libFuzzer: every file argument, or every file
// of every directory argument, is passed once to LLVMFuzzerTestOneInput.
// Builds without libFuzzer (Apple Clang, GCC, MSVC) use it to run the seed
// corpora as CTests, so every target compiles and its seeds run on every leg.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

namespace {

int run_file(const std::filesystem::path& path) {
  std::ifstream in{path, std::ios::binary};
  if (!in) {
    std::fprintf(stderr, "cannot read %s\n", path.string().c_str());
    return 1;
  }
  const std::vector<char> bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  std::vector<std::uint8_t> data(bytes.size());
  std::transform(bytes.begin(), bytes.end(), data.begin(), [](char c) { return static_cast<std::uint8_t>(c); });
  LLVMFuzzerTestOneInput(data.data(), data.size());
  return 0;
}

std::vector<std::filesystem::path> inputs(const std::filesystem::path& arg) {
  std::vector<std::filesystem::path> out;
  if (!std::filesystem::is_directory(arg)) {
    out.push_back(arg);
    return out;
  }
  for (const auto& entry : std::filesystem::directory_iterator(arg)) {
    if (entry.is_regular_file()) {
      out.push_back(entry.path());
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  int failures = 0;
  std::size_t count = 0;
  for (int i = 1; i < argc; ++i) {
    for (const auto& path : inputs(argv[i])) {  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      failures += run_file(path);
      ++count;
    }
  }
  std::printf("replayed %zu input(s)\n", count);
  return failures == 0 && count > 0 ? 0 : 1;
}
