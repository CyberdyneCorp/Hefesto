// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-pdk-manager "Offline PDK packs" (export, import, tampering,
// malformed pack.json) and the tar / stored-Zstandard writers behind them.

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>

#include "harness.hpp"
#include "hefesto/pdk/diag_codes.hpp"
#include "hefesto/pdk/packs.hpp"
#include "hefesto/pdk/tar_reader.hpp"
#include "hefesto/pdk/tar_writer.hpp"
#include "hefesto/platform/process_runner.hpp"

namespace pdk = hefesto::pdk;
namespace fs = std::filesystem;
using namespace hefesto::test::pdk_harness;  // NOLINT(google-build-using-namespace)
using hefesto::test::read_file;
using hefesto::test::write_file;

namespace {

// A second, offline manager (no catalog documents) with the real decoder.
struct OfflineSide {
  hefesto::test::TempDir dir;
  hefesto::platform::LocalFileSystem files;
  hefesto::test::FakeHost host;
  hefesto::test::MapFetcher fetcher;  // empty: any network access fails
  hefesto::platform::UnsupportedProcessRunner no_processes;
  std::unique_ptr<pdk::Decompressor> decoder = pdk::make_default_decompressor(no_processes);
  pdk::PdkManager manager{files, host, fetcher, pdk::Catalog::builtin()};
  hefesto::diag::CollectingSink sink;
  pdk::InstallOptions options;

  OfflineSide() {
    manager.set_root(dir.path() / "root");
    manager.set_decompressor(decoder.get());
    options.accept_license = true;
    options.accept_unverified = true;
  }
};

fs::path exported(Harness& h) {
  auto pack = pdk::export_pack(h.manager, h.sky130(), kHashA, h.dir.path() / "out", h.sink);
  REQUIRE(pack.ok());
  return pack.value();
}

// Rewrites one byte of the member `name` inside an uncompressed tar file.
void tamper(const fs::path& pack, const std::string& name) {
  std::string bytes = read_file(pack);
  const auto header = bytes.find(name + std::string(1, '\0'));
  REQUIRE(header != std::string::npos);
  bytes[header + 512 + 600] ^= 0x01;  // inside the member's data
  write_file(pack, bytes);
}

}  // namespace

TEST_CASE("air-gapped workstation: export, then import with no network access",
          "[cap:hefesto-pdk-manager][req:Offline PDK packs]") {
  Harness h;
  REQUIRE(h.install("sky130A", kHashA, {"sky130_fd_sc_hd"}).ok());
  fs::create_directories(h.dir.path() / "out");
  const fs::path pack = exported(h);
  CHECK(pack.filename() == "sky130-" + kHashA + ".hefestopdk");
  CHECK_FALSE(fs::exists(h.dir.path() / "out" / ("." + pack.filename().string() + ".parts")));

  OfflineSide offline;
  auto manifest = pdk::import_pack(offline.manager, pack, offline.options, offline.sink);
  REQUIRE(manifest.ok());
  CHECK(manifest.value().rebuilt);  // the download cache is gone after a successful install
  CHECK(manifest.value().libraries == std::vector<std::string>{"sky130_fd_sc_hd"});
  CHECK(manifest.value().variants == std::vector<std::string>{"sky130A", "sky130B"});
  CHECK(offline.fetcher.requests.empty());
  CHECK(offline.fetcher.downloads.empty());
  const fs::path v = offline.manager.root() / "ciel/sky130/versions" / kHashA;
  CHECK(read_file(v / "sky130A/libs.tech/magic/sky130A.tech") == "tech A\n");
  CHECK(read_file(v / "sky130B/libs.ref/sky130_fd_sc_hd/mag/x.mag") == "magic\n");
  CHECK(read_file(v / "sky130A/SOURCES") == "open_pdks " + kHashA + "\n");
  CHECK_FALSE(fs::exists(offline.manager.root() / "ciel/.hefesto-import" / pack.filename()));
  // The pinned version resolves (projects that pin it open).
  const auto tech = offline.manager.tech_file(offline.manager.catalog().resolve("sky130A").value(), kHashA);
  REQUIRE(tech.ok());
  // Provenance: from a pack, never trusted (no catalog key).
  const auto record = offline.manager.provenance(h.sky130(), kHashA);
  REQUIRE(record.ok());
  CHECK(record.value().source == "pack:" + std::string{kBase});
  CHECK_FALSE(record.value().trusted);
  CHECK(record.value().assets.size() == 2);
}

TEST_CASE("tampered pack: import fails on that asset's SHA-256 check", "[cap:hefesto-pdk-manager][req:Offline PDK packs]") {
  Harness h;
  REQUIRE(h.install("sky130A", kHashA, {"sky130_fd_sc_hd"}).ok());
  fs::create_directories(h.dir.path() / "out");
  const fs::path pack = exported(h);
  tamper(pack, "sky130_fd_sc_hd.tar.zst");
  OfflineSide offline;
  auto manifest = pdk::import_pack(offline.manager, pack, offline.options, offline.sink);
  REQUIRE_FALSE(manifest.ok());
  CHECK(manifest.error().code == pdk::codes::kVerify);
  CHECK(manifest.error().text.find("sky130_fd_sc_hd.tar.zst: SHA-256 mismatch") != std::string::npos);
  CHECK_FALSE(fs::exists(offline.manager.root() / "ciel/sky130/versions" / kHashA));
}

TEST_CASE("an unsigned pack needs consent; a pack for an uninstalled version cannot be exported",
          "[cap:hefesto-pdk-manager][req:Offline PDK packs]") {
  Harness h;
  fs::create_directories(h.dir.path() / "out");
  const auto none = pdk::export_pack(h.manager, h.sky130(), kHashA, h.dir.path() / "out", h.sink);
  REQUIRE_FALSE(none.ok());
  CHECK(none.error().code == pdk::codes::kNotInstalled);
  REQUIRE(h.install("sky130A", kHashA, {"sky130_fd_sc_hd"}).ok());
  const fs::path pack = exported(h);
  OfflineSide offline;
  offline.options.accept_unverified = false;
  auto refused = pdk::import_pack(offline.manager, pack, offline.options, offline.sink);
  REQUIRE_FALSE(refused.ok());
  CHECK(refused.error().code == pdk::codes::kUnverified);
}

TEST_CASE("export reuses cached original assets when they still match",
          "[cap:hefesto-pdk-manager][req:Offline PDK packs]") {
  Harness h;
  REQUIRE(h.install("sky130A", kHashA, {"sky130_fd_sc_hd"}).ok());
  const fs::path cache = h.root / "ciel/sky130/.hefesto-downloads" / kHashA;
  for (const char* content : {"common", "sky130_fd_sc_hd"}) {
    write_file(cache / (std::string{content} + ".tar.zst"),
               h.fetcher.documents[std::string{kBase} + "/sky130/" + kHashA + "/" + content + ".tar.zst"]);
  }
  fs::create_directories(h.dir.path() / "out");
  const fs::path pack = exported(h);
  hefesto::test::TempDir scratch;
  std::ifstream in{pack, std::ios::binary};
  REQUIRE(pdk::unpack_tar(in, scratch.path(), 1U << 20U).ok());
  auto manifest = pdk::parse_pack_manifest(read_file(scratch.path() / "pack.json"));
  REQUIRE(manifest.ok());
  CHECK_FALSE(manifest.value().rebuilt);
  REQUIRE(manifest.value().assets.size() == 2);
  for (const auto& asset : manifest.value().assets) {
    CHECK(asset.sha256 == pdk::sha256_hex(read_file(cache / asset.filename)));
  }
}

TEST_CASE("malformed pack.json is rejected", "[cap:hefesto-pdk-manager][req:Offline PDK packs]") {
  const std::string asset = R"({"content":"common","filename":"common.tar.zst","sha256":"","size":1})";
  const std::string ok = R"({"family":"sky130","version":")" + kHashA + R"(","assets":[)" + asset + "]}";
  REQUIRE(pdk::parse_pack_manifest(ok).ok());
  for (const std::string& bad :
       {std::string{"[]"}, std::string{"{"}, std::string{R"({"family":"sky130","version":"x","assets":[]})"},
        R"({"family":"sky130","version":")" + kHashA + R"(","assets":[]})",
        R"({"family":"sky130","version":")" + kHashA +
            R"(","assets":[{"content":"common","filename":"../evil.tar.zst"}]})",
        R"({"family":"sky130","version":")" + kHashA + R"(","assets":[{"content":"common","filename":"pack.json"}]})",
        R"({"family":"sky130","version":")" + kHashA +
            R"(","assets":[{"content":"common","filename":"a.tar.zst","sha256":"XYZ"}]})",
        R"({"family":"sky130","version":")" + kHashA + R"(","libraries":[1],"assets":[)" + asset + "]}"}) {
    INFO(bad);
    const auto parsed = pdk::parse_pack_manifest(bad);
    REQUIRE_FALSE(parsed.ok());
    CHECK(parsed.error().code == pdk::codes::kPack);
  }
}

TEST_CASE("a pack without pack.json or with a missing asset is rejected",
          "[cap:hefesto-pdk-manager][req:Offline PDK packs]") {
  OfflineSide offline;
  const fs::path empty_pack = offline.dir.path() / "empty.hefestopdk";
  {
    std::ofstream out{empty_pack, std::ios::binary};
    pdk::TarWriter tar{out};
    REQUIRE(tar.add_bytes("readme", "x").ok());
    REQUIRE(tar.finish().ok());
  }
  auto none = pdk::import_pack(offline.manager, empty_pack, offline.options, offline.sink);
  REQUIRE_FALSE(none.ok());
  CHECK(none.error().code == pdk::codes::kPack);
  const fs::path missing = offline.dir.path() / "missing.hefestopdk";
  {
    std::ofstream out{missing, std::ios::binary};
    pdk::TarWriter tar{out};
    REQUIRE(tar.add_bytes("pack.json", R"({"family":"sky130","version":")" + kHashA +
                                           R"(","assets":[{"content":"common","filename":"common.tar.zst"}]})")
                .ok());
    REQUIRE(tar.finish().ok());
  }
  auto absent = pdk::import_pack(offline.manager, missing, offline.options, offline.sink);
  REQUIRE_FALSE(absent.ok());
  CHECK(absent.error().code == pdk::codes::kPack);
}

TEST_CASE("tar writer: long names round trip through the safe reader", "[cap:hefesto-pdk-manager][req:Offline PDK packs]") {
  const std::string long_name = "sky130A/libs.ref/" + std::string(120, 'n') + "/cell.mag";
  std::stringstream archive;
  pdk::TarWriter tar{archive};
  REQUIRE(tar.add_bytes("short.txt", "a").ok());
  REQUIRE(tar.add_bytes(long_name, std::string(1000, 'b')).ok());
  REQUIRE(tar.finish().ok());
  hefesto::test::TempDir dir;
  const auto report = pdk::unpack_tar(archive, dir.path(), 1U << 20U);
  REQUIRE(report.ok());
  CHECK(report.value().files == 2);
  CHECK(read_file(dir.path() / "short.txt") == "a");
  CHECK(read_file(dir.path() / long_name) == std::string(1000, 'b'));
}

TEST_CASE("stored Zstandard frames decode with the library decoder", "[cap:hefesto-pdk-manager][req:Offline PDK packs]") {
  hefesto::test::TempDir dir;
  for (const std::size_t size : {std::size_t{0}, std::size_t{1}, std::size_t{128 * 1024}, std::size_t{300 * 1024 + 7}}) {
    std::string data(size, '\0');
    for (std::size_t i = 0; i < size; ++i) {
      data[i] = static_cast<char>('a' + static_cast<char>(i % 26));
    }
    {
      std::ofstream file{dir.path() / "f.zst", std::ios::binary | std::ios::trunc};
      pdk::StoredZstdWriter frame{file};
      std::ostream out{&frame};
      out.write(data.data(), static_cast<std::streamsize>(data.size()));
      frame.finish();
    }
    hefesto::platform::UnsupportedProcessRunner no_processes;
    auto decoder = pdk::make_default_decompressor(no_processes);
    INFO(size);
    REQUIRE(decoder->decompress(dir.path() / "f.zst", dir.path() / "f", size + 1).ok());
    CHECK(read_file(dir.path() / "f") == data);
  }
}
