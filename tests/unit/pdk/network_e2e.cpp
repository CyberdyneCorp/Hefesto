// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// End-to-end PDK install over the network (ctest label `network`; set
// HEFESTO_SKIP_NETWORK_TESTS=1 to skip): the real curl Fetcher, the real
// Zstandard decoder and the local install host install the pinned sky130A
// release 0fe599b2 (common + the 9.5 KB sky130_ml_xx_hd library, about
// 6.4 MB downloaded, the magic subset kept) from the ciel release assets
// into a temporary PDK root. A file:// static-web catalog pins the assets'
// SHA-256 and size (GitHub reports no digest for these assets), so the
// manager's own verification is exercised: a wrong digest installs nothing,
// the corrected catalog reuses the already-verified download. Not part of
// hefesto_pdk_tests so that `ctest -LE network` leaves it out.
//
// Network robustness: a transient fetch failure (PDK-FETCH-001) is retried
// up to kAttempts times with exponential backoff; when the network is
// clearly unavailable (curl cannot resolve or connect to GitHub, or GitHub
// keeps answering with an HTTP 5xx server error, before the test or after
// the last attempt) the test is skipped, not failed. Any other failure (a
// verification or unpack error, an HTTP 4xx, or a fetch failure that
// persists while GitHub is reachable and healthy) still fails the test.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string_view>
#include <thread>

#include "hefesto/diag/sink.hpp"
#include "hefesto/pdk/diag_codes.hpp"
#include "hefesto/pdk/manager.hpp"
#include "hefesto/platform_posix/process_runner.hpp"
#include "temp_dir.hpp"

namespace pdk = hefesto::pdk;
namespace fs = std::filesystem;
using hefesto::test::read_file;
using hefesto::test::write_file;

namespace {

const std::string kVersion = "0fe599b2afb6708d281543108caf8310912f54af";
const std::string kReleases = "https://github.com/fossi-foundation/ciel-releases/releases/download/sky130-" + kVersion;

struct PinnedAsset {
  const char* content;
  const char* sha256;
  std::uint64_t size;
};
constexpr PinnedAsset kCommon{"common", "970d671d28ffcc4dcc9bc9c22d5ed3924c4ec308751fa8536b195eb2d60c3209", 6372405};
constexpr PinnedAsset kLibrary{"sky130_ml_xx_hd", "e17a443edcc220e6e32d17f699e143f870d75dfac95c04904fbadcae8d6ed3a2",
                               9521};

std::string asset_json(const PinnedAsset& asset, const std::string& sha256) {
  const std::string name = std::string{asset.content} + ".tar.zst";
  return R"({"content":")" + std::string{asset.content} + R"(","filename":")" + name + R"(","url":")" + kReleases +
         "/" + name + R"(","sha256":")" + sha256 + R"(","size":)" + std::to_string(asset.size) + "}";
}

// The static-web catalog: <base>/sky130/manifest.json and <base>/sky130/<hash>/manifest.json.
void write_catalog(const fs::path& base, const std::string& library_sha) {
  write_file(base / "sky130/manifest.json",
             R"({"pdk":"sky130","versions":[{"version":")" + kVersion + R"(","date":"2024-08-17T01:34:30Z"}]})");
  write_file(base / "sky130" / kVersion / "manifest.json",
             "{\"assets\":[" + asset_json(kCommon, kCommon.sha256) + "," + asset_json(kLibrary, library_sha) + "]}");
}

bool skipped() {
  const char* skip = std::getenv("HEFESTO_SKIP_NETWORK_TESTS");  // NOLINT(concurrency-mt-unsafe)
  return skip != nullptr && *skip != '\0' && std::string_view{skip} != "0";
}

constexpr int kAttempts = 4;

// curl exit 6 (could not resolve host) and 7 (could not connect): no network.
// curl exit 22 with an HTTP 5xx status: GitHub itself is failing (e.g. 504).
bool network_unavailable(const hefesto::diag::Diagnostic& d) {
  if (d.code != pdk::codes::kFetch) {
    return false;
  }
  const bool offline = d.text.find("curl exit 6:") != std::string::npos || d.text.find("curl exit 7:") != std::string::npos;
  const bool server_error = d.text.find("curl exit 22:") != std::string::npos &&
                            d.text.find("returned error: 5") != std::string::npos;
  return offline || server_error;
}

void skip_if_offline(const hefesto::diag::Diagnostic& d) {
  if (network_unavailable(d)) {
    SKIP("network unavailable: " + d.text);
  }
}

// Skips the test when GitHub cannot be reached at all.
void require_network(hefesto::platform::ProcessRunner& runner) {
  const hefesto::platform::ProcessRequest probe{
      "curl", {"-sS", "-o", "/dev/null", "-I", "--max-time", "30", "https://github.com/"}, std::nullopt, {}};
  const auto out = runner.run(probe);
  REQUIRE(out.ok());  // the curl fetcher needs curl: its absence is a real failure
  if (out.value().exit_code == 6 || out.value().exit_code == 7) {
    SKIP("network unavailable: curl exit " + std::to_string(out.value().exit_code) + ": " + out.value().stderr_data);
  }
}

// Runs `op` until it succeeds, fails with something other than a fetch
// error, or kAttempts runs failed to fetch; waits 2 s, 4 s, then 6 s, between runs.
template <typename Op>
auto with_retries(Op op) -> decltype(op()) {
  auto result = op();
  for (int attempt = 1; attempt < kAttempts && !result.ok() && result.error().code == pdk::codes::kFetch; ++attempt) {
    UNSCOPED_INFO("attempt " << attempt << " failed to fetch: " << result.error().text);
    std::this_thread::sleep_for(std::chrono::seconds{2 * attempt});
    result = op();
  }
  if (!result.ok()) {
    skip_if_offline(result.error());
  }
  return result;
}

bool has_code(const hefesto::diag::CollectingSink& sink, std::string_view code, std::string_view text) {
  for (const auto& d : sink.snapshot()) {
    if (d.code == code && d.text.find(text) != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("network: install the pinned sky130A subset with the real fetcher, verified",
          "[cap:hefesto-pdk-manager][req:Download integrity verification][network]") {
  if (skipped()) {
    SKIP("HEFESTO_SKIP_NETWORK_TESTS is set");
  }
  hefesto::test::TempDir dir;
  hefesto::platform::LocalFileSystem files;
  hefesto::platform_posix::PosixProcessRunner runner;
  require_network(runner);
  auto host = pdk::make_local_install_host();
  auto fetcher = pdk::make_process_fetcher(runner, host->environment("GITHUB_TOKEN"));
  auto decoder = pdk::make_default_decompressor(runner);
  pdk::PdkManager manager{files, *host, *fetcher, pdk::Catalog::builtin()};
  manager.set_decompressor(decoder.get());
  const fs::path root = dir.path() / "root";
  manager.set_root(root);
  const fs::path catalog = dir.path() / "catalog";

  pdk::InstallOptions options;
  options.source.location = "file://" + catalog.string();
  options.accept_license = true;
  options.accept_unverified = true;  // no signed Hefesto catalog exists yet
  options.unpack_factor = 64;        // sky130_ml_xx_hd unpacks to 45x its size
  options.include_prefixes = {"sky130A/libs.tech/magic/", "sky130A/libs.ref/sky130_ml_xx_hd/"};
  std::uint64_t downloaded = 0;
  options.progress = [&downloaded](const pdk::InstallProgress& p) {
    if (p.phase == pdk::InstallPhase::download) {
      downloaded = std::max(downloaded, p.bytes);
    }
  };
  const auto selection = manager.catalog().resolve("sky130A").value();
  const fs::path version_dir = root / "ciel/sky130/versions" / kVersion;

  // 1. A wrong digest for the library: verification fails, nothing is installed.
  write_catalog(catalog, std::string(64, '0'));
  hefesto::diag::CollectingSink failed;
  const auto bad =
      with_retries([&] { return manager.install(selection, kVersion, {"sky130_ml_xx_hd"}, options, failed); });
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.error().code == pdk::codes::kVerify);
  CHECK(bad.error().text.find("sky130_ml_xx_hd.tar.zst: SHA-256 mismatch") != std::string::npos);
  CHECK(bad.error().text.find(kLibrary.sha256) != std::string::npos);  // the actual digest
  CHECK_FALSE(fs::exists(version_dir));
  CHECK(has_code(failed, pdk::codes::kInfo, std::string{"verified common.tar.zst sha256 "} + kCommon.sha256));
  CHECK(downloaded == kCommon.size);

  // 2. The corrected catalog: common is reused from the verified download cache.
  write_catalog(catalog, kLibrary.sha256);
  hefesto::diag::CollectingSink sink;
  const auto good =
      with_retries([&] { return manager.install(selection, kVersion, {"sky130_ml_xx_hd"}, options, sink); });
  INFO((good.ok() ? std::string{} : good.error().text));
  REQUIRE(good.ok());
  CHECK(has_code(sink, pdk::codes::kInfo, "using the downloaded common.tar.zst"));
  CHECK(has_code(sink, pdk::codes::kInfo, std::string{"verified sky130_ml_xx_hd.tar.zst sha256 "} + kLibrary.sha256));

  // ciel layout: version directory, SOURCES, provenance, no staging or cache left.
  CHECK(fs::is_regular_file(version_dir / "sky130A/libs.tech/magic/sky130A.tech"));
  CHECK(fs::is_regular_file(version_dir / "sky130A/libs.tech/magic/sky130A.magicrc"));
  CHECK(fs::is_regular_file(version_dir / "sky130A/libs.ref/sky130_ml_xx_hd/mag/font_65.mag"));
  CHECK_FALSE(fs::exists(version_dir / "sky130B/libs.tech"));  // filtered out
  CHECK(read_file(version_dir / "sky130A/SOURCES") == "open_pdks " + kVersion + "\n");
  CHECK_FALSE(fs::exists(root / "ciel/sky130/.hefesto-downloads" / kVersion));
  CHECK_FALSE(fs::exists(root / "ciel/sky130/versions" / ("." + kVersion + ".staging")));
  const auto record = manager.provenance(*selection.family, kVersion);
  REQUIRE(record.ok());
  CHECK(record.value().assets ==
        std::vector<std::pair<std::string, std::string>>{{"common.tar.zst", kCommon.sha256},
                                                         {"sky130_ml_xx_hd.tar.zst", kLibrary.sha256}});
  CHECK(record.value().partial == options.include_prefixes);

  // Enabled like ciel: <root>/sky130A -> ciel/sky130/versions/<hash>/sky130A, and the store resolves it.
  REQUIRE(manager.enable(*selection.family, kVersion).ok());
  CHECK(fs::read_symlink(root / "sky130A") == fs::path{"ciel/sky130/versions"} / kVersion / "sky130A");
  CHECK(read_file(root / "ciel/sky130/current") == kVersion + "\n");
  auto store = pdk::make_managed_pdk_store(manager, *host);
  const auto tech = store->find_technology("sky130A");
  REQUIRE(tech.ok());
  CHECK(tech.value() == version_dir / "sky130A/libs.tech/magic/sky130A.tech");
}

TEST_CASE("network: the GitHub releases source lists the pinned version's assets",
          "[cap:hefesto-pdk-manager][req:GitHub releases source compatibility][network]") {
  if (skipped()) {
    SKIP("HEFESTO_SKIP_NETWORK_TESTS is set");
  }
  hefesto::platform::LocalFileSystem files;
  hefesto::platform_posix::PosixProcessRunner runner;
  require_network(runner);
  auto host = pdk::make_local_install_host();
  auto fetcher = pdk::make_process_fetcher(runner, host->environment("GITHUB_TOKEN"));
  pdk::PdkManager manager{files, *host, *fetcher, pdk::Catalog::builtin()};
  const auto assets = with_retries(
      [&] { return manager.version_assets(*manager.catalog().find_family("sky130"), kVersion, pdk::CatalogSource{}); });
  INFO((assets.ok() ? std::string{} : assets.error().text));
  REQUIRE(assets.ok());
  const auto common = std::find_if(assets.value().begin(), assets.value().end(),
                                   [](const pdk::Asset& a) { return a.content == "common"; });
  REQUIRE(common != assets.value().end());
  CHECK(common->size == kCommon.size);
  CHECK(common->url == kReleases + "/common.tar.zst");
}
