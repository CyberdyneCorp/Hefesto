// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-pdk-manager: install, verify, unpack, layout, enable, uninstall,
// provenance and the managed PDK store, against an in-memory catalog and a
// temporary PDK root.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "fakes.hpp"
#include "harness.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/pdk/diag_codes.hpp"
#include "hefesto/pdk/manager.hpp"
#include "hefesto/platform/process_runner.hpp"
#include "temp_dir.hpp"

namespace pdk = hefesto::pdk;
namespace fs = std::filesystem;
using hefesto::test::TarWriter;
using hefesto::test::write_file;

using namespace hefesto::test::pdk_harness;  // NOLINT(google-build-using-namespace)

TEST_CASE("install layout and SOURCES", "[cap:hefesto-pdk-manager][req:Install layout compatible with ciel]") {
  Harness h;
  REQUIRE(h.install("sky130A", kHashA, {"sky130_fd_sc_hd"}).ok());
  const auto v = h.version_dir(kHashA);
  CHECK(hefesto::test::read_file(v / "sky130A/libs.tech/magic/sky130A.tech") == "tech A\n");
  CHECK(fs::exists(v / "sky130A/libs.ref/sky130_fd_sc_hd/mag/x.mag"));
  CHECK(hefesto::test::read_file(v / "sky130A/SOURCES") == "open_pdks " + kHashA + "\n");
  CHECK(hefesto::test::read_file(v / "sky130B/SOURCES") == "open_pdks " + kHashA + "\n");
  CHECK_FALSE(fs::exists(h.root / "ciel/sky130/versions" / ("." + kHashA + ".staging")));
  const auto installed = h.manager.installed();
  REQUIRE(installed.ok());
  REQUIRE(installed.value().size() == 2);  // both variants of the version
  CHECK(installed.value()[0].variant == "sky130A");
  CHECK(installed.value()[0].libraries == std::vector<std::string>{"sky130_fd_sc_hd"});
  CHECK(installed.value()[0].version.commit_date == std::nullopt);  // no commit date given to install
  CHECK_FALSE(installed.value()[0].enabled);
}

TEST_CASE("PDK root: explicit, PDK_ROOT, ~/.ciel", "[cap:hefesto-pdk-manager][req:Install layout compatible with ciel]") {
  Harness h;
  h.host.home = "/home/u";
  CHECK(h.manager.resolve_root(fs::path{"/chosen"}) == fs::path{"/chosen"});
  CHECK(h.manager.resolve_root(std::nullopt) == fs::path{"/home/u/.ciel"});
  h.host.env["PDK_ROOT"] = "/pdk";
  CHECK(h.manager.resolve_root(std::nullopt) == fs::path{"/pdk"});
}

TEST_CASE("shared root with ciel; volare installs are read-only",
          "[cap:hefesto-pdk-manager][req:Install layout compatible with ciel]") {
  Harness h;
  write_file(h.version_dir(kHashA) / "sky130A/libs.tech/magic/sky130A.tech", "x");
  write_file(h.root / "ciel/sky130/current", kHashA + "\n");
  write_file(h.root / "volare/sky130/versions" / kHashB / "sky130A/libs.tech/magic/sky130A.tech", "x");
  const auto installed = h.manager.installed();
  REQUIRE(installed.ok());
  REQUIRE(installed.value().size() == 2);
  CHECK(installed.value()[0].version.hash == kHashA);
  CHECK(installed.value()[0].enabled);
  CHECK_FALSE(installed.value()[0].read_only);
  CHECK(installed.value()[1].version.hash == kHashB);
  CHECK(installed.value()[1].read_only);
  CHECK(h.fetcher.downloads.empty());
  CHECK(h.manager.current(h.sky130()) == kHashA);
}

TEST_CASE("corrupted asset: nothing installed, both digests reported",
          "[cap:hefesto-pdk-manager][req:Download integrity verification]") {
  Harness h;
  h.corrupt(kHashA, "common");
  const auto status = h.install("sky130A", kHashA, {"sky130_fd_sc_hd"});
  REQUIRE_FALSE(status.ok());
  CHECK(status.error().code == pdk::codes::kVerify);
  CHECK(status.error().text.find("common.tar.zst") != std::string::npos);
  CHECK(status.error().text.find("expected ") != std::string::npos);
  CHECK(status.error().text.find(", got ") != std::string::npos);
  CHECK_FALSE(fs::exists(h.version_dir(kHashA)));
  CHECK_FALSE(fs::exists(h.root / "ciel/sky130/.hefesto-downloads" / kHashA / "common.tar.zst"));  // deleted
}

TEST_CASE("resume after network loss, then full verification",
          "[cap:hefesto-pdk-manager][req:Download integrity verification]") {
  Harness h;
  h.fetcher.fail_after_bytes = 1200;  // the first download stops part-way
  const auto first = h.install("sky130A", kHashA, {"sky130_fd_sc_hd"});
  REQUIRE_FALSE(first.ok());
  CHECK(first.error().code == pdk::codes::kFetch);
  CHECK_FALSE(fs::exists(h.version_dir(kHashA)));
  REQUIRE(h.install("sky130A", kHashA, {"sky130_fd_sc_hd"}).ok());
  const std::string common = std::string{kBase} + "/sky130/" + kHashA + "/common.tar.zst";
  CHECK(h.fetcher.resumed_from[common] == 1200);
  CHECK(h.sink.snapshot().size() > 0);
  CHECK(fs::exists(h.version_dir(kHashA) / "sky130A/libs.tech/magic/sky130A.tech"));
}

TEST_CASE("an asset whose size differs is rejected", "[cap:hefesto-pdk-manager][req:Download integrity verification]") {
  Harness h;
  h.fetcher.documents[std::string{kBase} + "/sky130/" + kHashA + "/common.tar.zst"] += std::string(512, '\0');
  const auto status = h.install("sky130A", kHashA, {"sky130_fd_sc_hd"});
  REQUIRE_FALSE(status.ok());
  CHECK(status.error().text.find("size mismatch") != std::string::npos);
}

TEST_CASE("an unverified source needs explicit consent", "[cap:hefesto-pdk-manager][req:PDK catalog]") {
  Harness h;
  h.options.accept_unverified = false;
  const auto status = h.install("sky130A", kHashA, {"sky130_fd_sc_hd"});
  REQUIRE_FALSE(status.ok());
  CHECK(status.error().code == pdk::codes::kUnverified);
  CHECK(h.fetcher.downloads.empty());
  CHECK_FALSE(pdk::source_verified(h.options.source));
}

TEST_CASE("path traversal in an asset aborts and leaves nothing", "[cap:hefesto-pdk-manager][req:Safe archive unpacking]") {
  Harness h;
  const std::string evil = TarWriter{}.file("sky130A/libs.tech/ok", "1").file("../../.profile", "evil").finish();
  const std::string url = std::string{kBase} + "/sky130/" + kHashA + "/common.tar.zst";
  h.fetcher.documents[url] = evil;
  const std::string manifest_url = std::string{kBase} + "/sky130/" + kHashA + "/manifest.json";
  std::string& manifest = h.fetcher.documents[manifest_url];
  const auto from = manifest.find("{\"content\":\"sky130_fd_sc_hd\"");
  manifest = "{\"assets\":[{\"content\":\"common\",\"filename\":\"common.tar.zst\",\"url\":\"" + url +
             "\",\"sha256\":\"" + pdk::sha256_hex(evil) + "\",\"size\":" + std::to_string(evil.size()) + "}," +
             manifest.substr(from);
  const auto status = h.install("sky130A", kHashA, {"sky130_fd_sc_hd"});
  REQUIRE_FALSE(status.ok());
  CHECK(status.error().code == pdk::codes::kUnsafeEntry);
  CHECK(status.error().text.find("../../.profile") != std::string::npos);
  CHECK_FALSE(fs::exists(h.version_dir(kHashA)));
  CHECK_FALSE(fs::exists(h.root / "ciel/sky130/versions" / ("." + kHashA + ".staging")));
  CHECK_FALSE(fs::exists(h.dir.path() / ".profile"));
  CHECK_FALSE(fs::exists(h.root / ".profile"));
}

TEST_CASE("cancelled install leaves no version directory", "[cap:hefesto-pdk-manager][req:Safe archive unpacking]") {
  Harness h;
  int parts = 0;
  h.options.cancelled = [&parts] { return ++parts > 1; };  // cancel after the first of three assets
  const auto status = h.install("sky130A", kHashA, {"sky130_fd_sc_hd", "sky130_fd_pr"});
  REQUIRE_FALSE(status.ok());
  CHECK_FALSE(fs::exists(h.version_dir(kHashA)));
  CHECK_FALSE(fs::exists(h.root / "ciel/sky130/versions" / ("." + kHashA + ".staging")));
}

TEST_CASE("adding a library later downloads only that library",
          "[cap:hefesto-pdk-manager][req:Incremental library downloads]") {
  Harness h;
  REQUIRE(h.install("sky130A", kHashA, {"sky130_fd_sc_hd"}).ok());
  h.fetcher.downloads.clear();
  REQUIRE(h.install("sky130A", kHashA, {"sky130_fd_sc_hs"}).ok());
  CHECK(h.fetcher.downloads == std::vector<std::string>{std::string{kBase} + "/sky130/" + kHashA + "/sky130_fd_sc_hs.tar.zst"});
  CHECK(fs::exists(h.version_dir(kHashA) / "sky130A/libs.ref/sky130_fd_sc_hs/mag/x.mag"));
  h.fetcher.downloads.clear();
  REQUIRE(h.install("sky130A", kHashA, {"sky130_fd_sc_hs"}).ok());  // already present
  CHECK(h.fetcher.downloads.empty());
  REQUIRE(h.manager.remove_library(h.sky130(), kHashA, "sky130_fd_sc_hs").ok());
  CHECK_FALSE(fs::exists(h.version_dir(kHashA) / "sky130A/libs.ref/sky130_fd_sc_hs"));
  CHECK_FALSE(fs::exists(h.version_dir(kHashA) / "sky130B/libs.ref/sky130_fd_sc_hs"));
}

TEST_CASE("unknown library fails before any download", "[cap:hefesto-pdk-manager][req:Library sets per variant]") {
  Harness h;
  const auto status = h.install("sky130A", kHashA, {"sky130_fd_sc_xx"});
  REQUIRE_FALSE(status.ok());
  CHECK(status.error().code == pdk::codes::kLibrary);
  CHECK(h.fetcher.downloads.empty());
}

TEST_CASE("a headless install needs the license acceptance flag",
          "[cap:hefesto-pdk-manager][req:Headless and scripted operation]") {
  Harness h;
  h.options.accept_license = false;
  const auto status = h.install("sky130A", kHashA, {"sky130_fd_sc_hd"});
  REQUIRE_FALSE(status.ok());
  CHECK(status.error().code == pdk::codes::kLicense);
  CHECK(h.fetcher.downloads.empty());
  h.options.accept_license = true;
  REQUIRE(h.install("sky130A", kHashA, {"sky130_fd_sc_hd"}).ok());
  h.options.accept_license = false;  // acceptance is recorded per family
  REQUIRE(h.install("sky130A", kHashA, {"sky130_fd_pr"}).ok());
}

TEST_CASE("enabling a version writes symlinks and current", "[cap:hefesto-pdk-manager][req:Enabling a version]") {
  Harness h;
  REQUIRE(h.install("sky130A", kHashA, {"sky130_fd_sc_hd"}).ok());
  REQUIRE(h.manager.enable(h.sky130(), kHashA).ok());
  CHECK(fs::read_symlink(h.root / "sky130A") == fs::path{"ciel/sky130/versions"} / kHashA / "sky130A");
  CHECK(fs::read_symlink(h.root / "sky130B") == fs::path{"ciel/sky130/versions"} / kHashA / "sky130B");
  CHECK(hefesto::test::read_file(h.root / "ciel/sky130/current") == kHashA + "\n");
  CHECK(fs::exists(h.root / "sky130A/libs.tech/magic/sky130A.tech"));
  CHECK(h.manager.enable(h.sky130(), kHashB).error().code == pdk::codes::kNotInstalled);
}

TEST_CASE("enabling over a real directory fails and changes nothing", "[cap:hefesto-pdk-manager][req:Enabling a version]") {
  Harness h;
  REQUIRE(h.install("sky130A", kHashA, {"sky130_fd_sc_hd"}).ok());
  fs::create_directories(h.root / "sky130A");
  const auto status = h.manager.enable(h.sky130(), kHashA);
  REQUIRE_FALSE(status.ok());
  CHECK(status.error().code == pdk::codes::kEnableConflict);
  CHECK(status.error().text.find("remove " + (h.root / "sky130A").string()) != std::string::npos);
  CHECK_FALSE(fs::exists(h.root / "ciel/sky130/current"));
  CHECK_FALSE(fs::exists(fs::symlink_status(h.root / "sky130B")));
}

TEST_CASE("uninstalling the current version removes symlinks and current",
          "[cap:hefesto-pdk-manager][req:Uninstall and cleanup]") {
  Harness h;
  REQUIRE(h.install("sky130A", kHashA, {"sky130_fd_sc_hd"}).ok());
  REQUIRE(h.manager.enable(h.sky130(), kHashA).ok());
  REQUIRE(h.manager.uninstall(h.sky130(), kHashA).ok());
  CHECK_FALSE(fs::exists(fs::symlink_status(h.root / "sky130A")));
  CHECK_FALSE(fs::exists(fs::symlink_status(h.root / "sky130B")));
  CHECK_FALSE(fs::exists(h.root / "ciel/sky130/current"));
  CHECK_FALSE(fs::exists(h.version_dir(kHashA)));
  CHECK(h.manager.uninstall(h.sky130(), kHashA).error().code == pdk::codes::kNotInstalled);
}

TEST_CASE("an update installs next to the old version", "[cap:hefesto-pdk-manager][req:Updates]") {
  Harness h;
  REQUIRE(h.install("sky130A", kHashA, {"sky130_fd_sc_hd"}).ok());
  h.publish(kHashB, "2025-01-01T00:00:00Z");
  REQUIRE(h.install("sky130A", kHashB, {"sky130_fd_sc_hd"}).ok());
  CHECK(fs::exists(h.version_dir(kHashA)));
  CHECK(fs::exists(h.version_dir(kHashB)));
  const auto remote = h.manager.remote_versions(h.sky130(), h.options.source);
  REQUIRE(remote.ok());
  CHECK(remote.value().front().hash == kHashB);
}

TEST_CASE("provenance of an installed version", "[cap:hefesto-pdk-manager][req:Licenses and provenance]") {
  Harness h;
  h.options.commit_date = "2024-08-17T01:34:30Z";
  REQUIRE(h.install("sky130A", kHashA, {"sky130_fd_sc_hd"}).ok());
  const auto p = h.manager.provenance(h.sky130(), kHashA);
  REQUIRE(p.ok());
  CHECK(p.value().repository == "open_pdks");
  CHECK(p.value().version == kHashA);
  CHECK(p.value().date == "2024-08-17T01:34:30Z");
  CHECK(p.value().source == kBase);
  CHECK(p.value().license == "Apache-2.0");
  CHECK_FALSE(p.value().trusted);
  REQUIRE(p.value().assets.size() == 2);
  for (const auto& [name, digest] : p.value().assets) {
    CHECK(digest.size() == 64);
  }
  const auto listed = h.manager.installed();
  CHECK(listed.value()[0].version.commit_date == "2024-08-17T01:34:30Z");
  write_file(h.root / "ciel/sky130/versions" / kHashB / "sky130A/libs.tech/magic/sky130A.tech", "x");
  const auto external = h.manager.provenance(h.sky130(), kHashB);  // installed by ciel
  REQUIRE(external.ok());
  CHECK(external.value().source == "external");
  CHECK(external.value().assets.empty());
}

TEST_CASE("a filtered install unpacks only the selected paths", "[cap:hefesto-pdk-manager][req:Safe archive unpacking]") {
  Harness h;
  h.options.include_prefixes = {"sky130A/libs.tech/magic/sky130A.tech", "sky130A/libs.ref/sky130_fd_sc_hd/mag/"};
  REQUIRE(h.install("sky130A", kHashA, {"sky130_fd_sc_hd"}).ok());
  CHECK(fs::exists(h.version_dir(kHashA) / "sky130A/libs.tech/magic/sky130A.tech"));
  CHECK(fs::exists(h.version_dir(kHashA) / "sky130A/libs.ref/sky130_fd_sc_hd/mag/x.mag"));
  CHECK_FALSE(fs::exists(h.version_dir(kHashA) / "sky130A/libs.tech/magic/sky130A.magicrc"));
  CHECK_FALSE(fs::exists(h.version_dir(kHashA) / "sky130B"));
  CHECK(h.manager.provenance(h.sky130(), kHashA).value().partial == h.options.include_prefixes);
}

TEST_CASE("GitHub source: the variant tag is tried after the family tag",
          "[cap:hefesto-pdk-manager][req:GitHub releases source compatibility]") {
  Harness h;
  const std::string hash = "0123456789abcdef0123456789abcdef01234567";
  h.fetcher.documents["https://api.github.com/repos/o/r/releases/tags/ihp-sg13g2-" + hash] =
      R"({"assets":[{"name":"common.tar.zst","browser_download_url":"https://x/c","size":1}]})";
  pdk::CatalogSource source{"github:o/r"};
  const auto assets = h.manager.version_assets(*h.manager.catalog().find_family("ihp-sg13"), hash, source);
  REQUIRE(assets.ok());
  REQUIRE(assets.value().size() == 1);
  CHECK(assets.value()[0].content == "common");
  const auto missing = h.manager.version_assets(h.sky130(), hash, source);
  REQUIRE_FALSE(missing.ok());
  CHECK(missing.error().code == pdk::codes::kUnknownVersion);
}

TEST_CASE("the managed store resolves the pinned version before the enabled one",
          "[cap:hefesto-files-and-pdks][req:PDK store]") {
  Harness h;
  for (const auto& hash : {kHashA, kHashB}) {
    write_file(h.version_dir(hash) / "sky130A/libs.tech/magic/sky130A.tech", hash);
  }
  write_file(h.root / "ciel/sky130/current", kHashB + "\n");
  auto pinned = pdk::make_managed_pdk_store(h.manager, h.host, pdk::PinnedVersion{"sky130", "sky130A", kHashA});
  CHECK(pinned->find_technology("sky130A").value() == h.version_dir(kHashA) / "sky130A/libs.tech/magic/sky130A.tech");
  auto enabled = pdk::make_managed_pdk_store(h.manager, h.host);
  CHECK(enabled->find_technology("sky130A").value() == h.version_dir(kHashB) / "sky130A/libs.tech/magic/sky130A.tech");
  CHECK_FALSE(enabled->find_technology("gf180mcuD").ok());
  write_file(h.dir.path() / "open_pdks/gf180mcuD/libs.tech/magic/gf180mcuD.tech", "x");  // PDKPATH install
  h.host.env["PDKPATH"] = (h.dir.path() / "open_pdks/gf180mcuD").string();
  CHECK(enabled->find_technology("gf180mcuD").ok());
  CHECK(enabled->roots() == std::vector<fs::path>{h.root});
}

TEST_CASE("the technology file of a pinned version", "[cap:hefesto-pdk-manager][req:Per-project version pinning]") {
  Harness h;
  REQUIRE(h.install("sky130A", kHashA, {"sky130_fd_sc_hd"}).ok());
  CHECK(h.manager.tech_file(h.variant("sky130A"), kHashA).value() ==
        h.version_dir(kHashA) / "sky130A/libs.tech/magic/sky130A.tech");
  CHECK(h.manager.tech_file(h.variant("sky130A"), kHashB).error().code == pdk::codes::kNotInstalled);
}

TEST_CASE("a front-end session PDK: manager, root and store over it",
          "[cap:hefesto-files-and-pdks][req:PDK store]") {
  hefesto::test::TempDir dir;
  hefesto::platform::LocalFileSystem files;
  hefesto::platform::UnsupportedProcessRunner no_processes;
  auto session = pdk::make_session_pdk(files, no_processes, dir.path());
  REQUIRE(session.manager);
  CHECK(session.manager->root() == dir.path());
  write_file(dir.path() / "ciel/sky130/versions" / kHashA / "sky130A/libs.tech/magic/sky130A.tech", "tech\n");
  REQUIRE(session.manager->enable(*session.manager->catalog().find_family("sky130"), kHashA).ok());
  auto store = pdk::make_managed_pdk_store(*session.manager, *session.host);
  const auto tech = store->find_technology("sky130A");
  REQUIRE(tech.ok());
  CHECK(tech.value() == dir.path() / "ciel/sky130/versions" / kHashA / "sky130A/libs.tech/magic/sky130A.tech");
  // Without process support downloads fail cleanly instead of running curl.
  const auto text = session.fetcher->get_text("https://example.invalid/x");
  REQUIRE_FALSE(text.ok());
  CHECK(text.error().code == hefesto::diag::codes::kPlatformUnsupported);
}
