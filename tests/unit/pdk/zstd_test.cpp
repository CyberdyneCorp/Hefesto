// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-pdk-manager "Safe archive unpacking": the Zstandard decoder of
// `.tar.zst` assets (the pinned hefesto::zstd library) and its size bound.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <filesystem>
#include <string>

#include "hefesto/pdk/diag_codes.hpp"
#include "hefesto/pdk/manager.hpp"
#include "hefesto/platform/process_runner.hpp"
#include "temp_dir.hpp"

namespace pdk = hefesto::pdk;
using hefesto::test::TempDir;

namespace {

// `printf 'hello hefesto\n' | zstd -19`
constexpr std::array<unsigned char, 27> kSmall = {0x28, 0xb5, 0x2f, 0xfd, 0x24, 0x0e, 0x71, 0x00, 0x00,
                                                  0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x68, 0x65, 0x66,
                                                  0x65, 0x73, 0x74, 0x6f, 0x0a, 0x30, 0x00, 0xc0, 0xbb};

// 1 MiB of zero bytes, `zstd -19`: 53 bytes that expand 20000-fold.
constexpr std::array<unsigned char, 53> kMegabyteOfZeros = {
    0x28, 0xb5, 0x2f, 0xfd, 0xa4, 0x00, 0x00, 0x10, 0x00, 0x4c, 0x00, 0x00, 0x08, 0x00, 0x01, 0x00, 0xfc, 0xff,
    0x39, 0x10, 0x02, 0x02, 0x00, 0x10, 0x00, 0x02, 0x00, 0x10, 0x00, 0x02, 0x00, 0x10, 0x00, 0x02, 0x00, 0x10,
    0x00, 0x02, 0x00, 0x10, 0x00, 0x02, 0x00, 0x10, 0x00, 0x03, 0x00, 0x10, 0x00, 0xf1, 0x3e, 0x16, 0xe1};

template <std::size_t N>
std::string bytes(const std::array<unsigned char, N>& data) {
  std::string out;
  for (const unsigned char c : data) {
    out.push_back(static_cast<char>(c));
  }
  return out;
}

}  // namespace

TEST_CASE("the zstd library decoder is linked and decodes a frame",
          "[cap:hefesto-pdk-manager][req:Safe archive unpacking]") {
  CHECK(pdk::have_zstd_library());
  TempDir dir;
  hefesto::test::write_file(dir.path() / "a.zst", bytes(kSmall));
  hefesto::platform::UnsupportedProcessRunner no_processes;
  auto decoder = pdk::make_default_decompressor(no_processes);
  REQUIRE(decoder->decompress(dir.path() / "a.zst", dir.path() / "a", 1024).ok());
  CHECK(hefesto::test::read_file(dir.path() / "a") == "hello hefesto\n");
}

// Regression: without the library the decoder ran `zstd -d` to completion and
// only then checked the size, so a small archive could fill the disk.
TEST_CASE("the zstd decoder stops at the size bound and removes the partial output",
          "[cap:hefesto-pdk-manager][req:Safe archive unpacking]") {
  TempDir dir;
  hefesto::test::write_file(dir.path() / "bomb.zst", bytes(kMegabyteOfZeros));
  hefesto::platform::UnsupportedProcessRunner no_processes;
  auto decoder = pdk::make_default_decompressor(no_processes);
  const auto status = decoder->decompress(dir.path() / "bomb.zst", dir.path() / "bomb", 4096);
  REQUIRE_FALSE(status.ok());
  CHECK(status.error().code == pdk::codes::kUnpackLimit);
  CHECK_FALSE(std::filesystem::exists(dir.path() / "bomb"));
  // A truncated frame is a bad archive.
  hefesto::test::write_file(dir.path() / "cut.zst", bytes(kSmall).substr(0, 15));
  const auto cut = decoder->decompress(dir.path() / "cut.zst", dir.path() / "cut", 1024);
  REQUIRE_FALSE(cut.ok());
  CHECK(cut.error().code == pdk::codes::kBadArchive);
}
