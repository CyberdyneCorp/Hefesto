// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-pdk-manager: SHA-256 ("Download integrity verification") and
// safe tar unpacking ("Safe archive unpacking").

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <sstream>

#include "fakes.hpp"
#include "hefesto/pdk/diag_codes.hpp"
#include "hefesto/pdk/manager.hpp"
#include "hefesto/pdk/sha256.hpp"
#include "hefesto/pdk/tar_reader.hpp"
#include "temp_dir.hpp"

namespace pdk = hefesto::pdk;
using hefesto::test::TarWriter;
using hefesto::test::TempDir;

TEST_CASE("SHA-256 test vectors (FIPS 180-4)", "[cap:hefesto-pdk-manager][req:Download integrity verification]") {
  CHECK(pdk::sha256_hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  CHECK(pdk::sha256_hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  CHECK(pdk::sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  pdk::Sha256 million;
  const std::string chunk(1000, 'a');
  for (int i = 0; i < 1000; ++i) {
    million.update(chunk);
  }
  CHECK(million.finish_hex() == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_CASE("SHA-256 of a file equals the in-memory digest",
          "[cap:hefesto-pdk-manager][req:Download integrity verification]") {
  TempDir dir;
  std::string data;
  for (int i = 0; i < 100000; ++i) {
    data += static_cast<char>(i * 7);
  }
  hefesto::test::write_file(dir.path() / "blob", data);
  const auto digest = pdk::sha256_file_hex(dir.path() / "blob");
  REQUIRE(digest.ok());
  CHECK(digest.value() == pdk::sha256_hex(data));
  CHECK_FALSE(pdk::sha256_file_hex(dir.path() / "missing").ok());
}

TEST_CASE("regular files are written; directories created as needed", "[cap:hefesto-pdk-manager][req:Safe archive unpacking]") {
  TempDir dir;
  const std::string long_name = "sky130A/libs.ref/" + std::string(120, 'x') + "/cell.mag";
  std::istringstream in{TarWriter{}
                            .directory("sky130A/")
                            .file("sky130A/libs.tech/magic/sky130A.tech", "tech\n")
                            .file("./sky130A/SOURCES", "open_pdks h\n")
                            .pax_file(long_name, "magic\n")
                            .finish()};
  const auto r = pdk::unpack_tar(in, dir.path(), 1 << 20);
  REQUIRE(r.ok());
  CHECK(r.value().files == 3);
  CHECK(hefesto::test::read_file(dir.path() / "sky130A/libs.tech/magic/sky130A.tech") == "tech\n");
  CHECK(std::filesystem::exists(dir.path() / "sky130A/SOURCES"));
  CHECK(hefesto::test::read_file(dir.path() / long_name) == "magic\n");
}

TEST_CASE("path traversal entry aborts the unpack", "[cap:hefesto-pdk-manager][req:Safe archive unpacking]") {
  TempDir dir;
  std::istringstream in{TarWriter{}.file("../../.profile", "evil").finish()};
  const auto r = pdk::unpack_tar(in, dir.path() / "v", 1 << 20);
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().code == pdk::codes::kUnsafeEntry);
  CHECK(r.error().text.find("../../.profile") != std::string::npos);
  CHECK_FALSE(std::filesystem::exists(dir.path() / ".profile"));
  for (const char* name : {"/etc/passwd", "a/../../b", "C:evil", "a\\b"}) {
    INFO(name);
    CHECK_FALSE(pdk::safe_entry_path(dir.path(), name).ok());
  }
  CHECK(pdk::safe_entry_path(dir.path(), "a/./b").value() == dir.path() / "a" / "b");
  CHECK(pdk::safe_entry_path(dir.path(), "./").value().empty());
}

TEST_CASE("links, devices and FIFOs are never created", "[cap:hefesto-pdk-manager][req:Safe archive unpacking]") {
  TempDir dir;
  std::istringstream in{TarWriter{}.symlink("sky130A/link", "/etc/passwd").file("sky130A/f", "x").finish()};
  const auto r = pdk::unpack_tar(in, dir.path(), 1 << 20);
  REQUIRE(r.ok());
  CHECK(r.value().skipped == std::vector<std::string>{"sky130A/link"});
  CHECK_FALSE(std::filesystem::exists(std::filesystem::symlink_status(dir.path() / "sky130A/link")));
}

TEST_CASE("the unpacked size is bounded", "[cap:hefesto-pdk-manager][req:Safe archive unpacking]") {
  TempDir dir;
  std::istringstream in{TarWriter{}.file("a", std::string(600, 'a')).file("b", std::string(600, 'b')).finish()};
  const auto r = pdk::unpack_tar(in, dir.path(), 1000);
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().code == pdk::codes::kUnpackLimit);
}

TEST_CASE("malformed archives are rejected", "[cap:hefesto-pdk-manager][req:Safe archive unpacking]") {
  TempDir dir;
  std::istringstream garbage{std::string(1024, 'z')};
  const auto r = pdk::unpack_tar(garbage, dir.path(), 1 << 20);
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().code == pdk::codes::kBadArchive);
  std::string truncated = TarWriter{}.file("a", std::string(2000, 'a')).finish().substr(0, 1024);
  std::istringstream cut{truncated};
  CHECK_FALSE(pdk::unpack_tar(cut, dir.path(), 1 << 20).ok());
}

TEST_CASE("an entry filter selects the files to write", "[cap:hefesto-pdk-manager][req:Safe archive unpacking]") {
  TempDir dir;
  std::istringstream in{TarWriter{}.file("./keep/a", "1").file("drop/b", std::string(5000, 'b')).finish()};
  const auto r = pdk::unpack_tar(in, dir.path(), 100, [](std::string_view name) { return name.starts_with("keep/"); });
  REQUIRE(r.ok());
  CHECK(r.value().files == 1);
  CHECK(std::filesystem::exists(dir.path() / "keep/a"));
  CHECK_FALSE(std::filesystem::exists(dir.path() / "drop"));
}

// Regression: the padding after a file's data was padded again, so the
// entry after any file whose size is not a multiple of 512 was misread.
TEST_CASE("consecutive files of unaligned sizes unpack intact", "[cap:hefesto-pdk-manager][req:Safe archive unpacking]") {
  TempDir dir;
  std::istringstream in{TarWriter{}.file("a", std::string(600, 'a')).file("b", "b").file("c", std::string(512, 'c')).finish()};
  const auto r = pdk::unpack_tar(in, dir.path(), 1 << 20);
  REQUIRE(r.ok());
  CHECK(r.value().files == 3);
  CHECK(hefesto::test::read_file(dir.path() / "a") == std::string(600, 'a'));
  CHECK(hefesto::test::read_file(dir.path() / "b") == "b");
  CHECK(hefesto::test::read_file(dir.path() / "c") == std::string(512, 'c'));
}
