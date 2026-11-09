// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the safe tar unpacker (capability
// hefesto-pdk-manager "Safe archive unpacking"; hefesto-testing-strategy
// "Parser fuzzing"). Every input unpacks into a fresh directory below a
// per-process scratch root with a 1 MiB bound; nothing may be written
// outside that directory.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <sstream>
#include <string>

#include "fuzz_common.hpp"
#include "hefesto/pdk/tar_reader.hpp"

namespace {

// One root per process, so parallel fuzzing jobs never share it; removed at exit.
class Scratch {
 public:
  Scratch() {
    std::random_device rd;
    root_ = std::filesystem::temp_directory_path() / ("hefesto-fuzz-tar-" + std::to_string(rd()));
    std::filesystem::create_directories(root_);
    root_ = std::filesystem::canonical(root_);
  }
  Scratch(const Scratch&) = delete;
  Scratch& operator=(const Scratch&) = delete;
  ~Scratch() {
    std::error_code ec;
    std::filesystem::remove_all(root_, ec);
  }
  [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }

 private:
  std::filesystem::path root_;
};

const std::filesystem::path& scratch() {
  static const Scratch kScratch;
  return kScratch.root();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::filesystem::path out = scratch() / "out";
  std::error_code ec;
  std::filesystem::remove_all(out, ec);
  std::filesystem::create_directories(out);
  std::istringstream in{std::string{hefesto::fuzz::as_text(data, size)}};
  (void)hefesto::pdk::unpack_tar(in, out, std::uint64_t{1} << 20);
  // Containment: the scratch root holds nothing but `out`.
  for (const auto& entry : std::filesystem::directory_iterator(scratch())) {
    if (entry.path() != out) {
      std::abort();
    }
  }
  std::filesystem::remove_all(out, ec);
  return 0;
}
