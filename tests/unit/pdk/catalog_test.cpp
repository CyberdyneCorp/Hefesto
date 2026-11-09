// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-pdk-manager: families and variants, library sets, versions,
// catalog manifests and the GitHub releases source.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "hefesto/pdk/catalog.hpp"
#include "hefesto/pdk/diag_codes.hpp"

namespace pdk = hefesto::pdk;

namespace {

const pdk::Catalog& catalog() {
  static const pdk::Catalog c = pdk::Catalog::builtin();
  return c;
}

std::vector<std::string> variant_names(const pdk::Family& family) {
  std::vector<std::string> out;
  for (const auto& v : family.variants) {
    out.push_back(v.name);
  }
  return out;
}

bool has(const std::vector<std::string>& list, std::string_view name) {
  return std::find(list.begin(), list.end(), name) != list.end();
}

}  // namespace

TEST_CASE("built-in family table", "[cap:hefesto-pdk-manager][req:PDK families and variants]") {
  REQUIRE(catalog().families().size() == 3);
  const auto* sky = catalog().find_family("sky130");
  const auto* gf = catalog().find_family("gf180mcu");
  const auto* ihp = catalog().find_family("ihp-sg13");
  REQUIRE(sky != nullptr);
  REQUIRE(gf != nullptr);
  REQUIRE(ihp != nullptr);
  CHECK(variant_names(*sky) == std::vector<std::string>{"sky130A", "sky130B"});
  CHECK(variant_names(*gf) == std::vector<std::string>{"gf180mcuD", "gf180mcuA", "gf180mcuB", "gf180mcuC"});
  CHECK(variant_names(*ihp) == std::vector<std::string>{"ihp-sg13g2", "ihp-sg13cmos5l"});
  CHECK(sky->upstream_repo == "open_pdks");
  CHECK(ihp->upstream_repo == "IHP-Open-PDK");
  CHECK(sky->manufacturable);
}

TEST_CASE("family selector picks the default variant", "[cap:hefesto-pdk-manager][req:PDK families and variants]") {
  const auto gf = catalog().resolve("gf180mcu");
  REQUIRE(gf.ok());
  CHECK(gf.value().variant->name == "gf180mcuD");
  CHECK_FALSE(gf.value().variant_explicit);
  const auto b = catalog().resolve("sky130B");
  REQUIRE(b.ok());
  CHECK(b.value().family->name == "sky130");
  CHECK(b.value().variant->name == "sky130B");
  CHECK(b.value().variant_explicit);
}

TEST_CASE("legacy IHP selector", "[cap:hefesto-pdk-manager][req:PDK families and variants]") {
  const auto r = catalog().resolve("ihp_sg13g2");
  REQUIRE(r.ok());
  CHECK(r.value().family->name == "ihp-sg13");
}

TEST_CASE("unknown selector", "[cap:hefesto-pdk-manager][req:PDK families and variants]") {
  const auto r = catalog().resolve("sky90");
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().code == pdk::codes::kSelect);
  CHECK(r.error().text == "'sky90' is not a valid PDK family or variant.");
}

TEST_CASE("default library sets", "[cap:hefesto-pdk-manager][req:Library sets per variant]") {
  const auto& sky = *catalog().find_family("sky130");
  const std::vector<std::string> a_defaults{"sky130_fd_io",    "sky130_fd_pr",    "sky130_fd_sc_hd",
                                            "sky130_fd_sc_hvl", "sky130_ml_xx_hd", "sky130_sram_macros"};
  CHECK(catalog().resolve_libraries(sky.variants[0], {}).value() == a_defaults);
  const auto b = catalog().resolve_libraries(sky.variants[1], {}).value();
  CHECK(b.size() == 7);
  CHECK(has(b, "sky130_fd_pr_reram"));
  for (const auto& lib : a_defaults) {
    CHECK(has(b, lib));
  }
  const auto& gf = *catalog().find_family("gf180mcu");
  CHECK(catalog().resolve_libraries(gf.variants[2], {"default"}).value() ==
        std::vector<std::string>{"gf180mcu_fd_io", "gf180mcu_fd_pr", "gf180mcu_fd_sc_mcu7t5v0", "gf180mcu_fd_sc_mcu9t5v0",
                                 "gf180mcu_fd_ip_sram"});
  const auto& ihp = *catalog().find_family("ihp-sg13");
  CHECK(catalog().resolve_libraries(ihp.variants[0], {}).value() ==
        std::vector<std::string>{"sg13g2_io", "sg13g2_pr", "sg13g2_sram", "sg13g2_stdcell"});
  CHECK(catalog().resolve_libraries(ihp.variants[1], {}).value() ==
        std::vector<std::string>{"sg13cmos5l_io", "sg13cmos5l_sram", "sg13cmos5l_stdcell"});
}

TEST_CASE("all, default and named libraries; unknown library", "[cap:hefesto-pdk-manager][req:Library sets per variant]") {
  const auto& a = catalog().find_family("sky130")->variants[0];
  CHECK(catalog().resolve_libraries(a, {"all"}).value() == a.libraries);
  CHECK(catalog().resolve_libraries(a, {"sky130_fd_sc_hs"}).value() == std::vector<std::string>{"sky130_fd_sc_hs"});
  CHECK(catalog().resolve_libraries(a, {"default", "sky130_fd_sc_hs"}).value().size() == 7);
  const auto bad = catalog().resolve_libraries(a, {"sky130_fd_sc_xx"});
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.error().code == pdk::codes::kLibrary);
  CHECK(bad.error().text.find("sky130_fd_sc_xx") != std::string::npos);
}

TEST_CASE("the family table is catalog data", "[cap:hefesto-pdk-manager][req:Library sets per variant]") {
  const auto updated = pdk::Catalog::from_json(R"({"families":[{"name":"sky130","repo":"open_pdks","extra":1,
    "variants":[{"name":"sky130A","libraries":["sky130_fd_sc_hd","sky130_new_lib"],"default":["sky130_fd_sc_hd"]}]}]})");
  REQUIRE(updated.ok());
  const auto sel = updated.value().resolve("sky130A");
  REQUIRE(sel.ok());
  CHECK(updated.value().resolve_libraries(*sel.value().variant, {"sky130_new_lib"}).ok());
  CHECK_FALSE(pdk::Catalog::from_json(R"({"families":[{"name":"x","variants":[]}]})").ok());
  CHECK_FALSE(pdk::Catalog::from_json("not json").ok());
}

TEST_CASE("versions: newest commit date first, undated last", "[cap:hefesto-pdk-manager][req:Version identity and ordering]") {
  std::vector<pdk::PdkVersion> versions{{"a", "2025-01-02T00:00:00Z", std::nullopt, false},
                                        {"b", std::nullopt, std::nullopt, false},
                                        {"c", "2026-03-01T00:00:00Z", std::nullopt, false}};
  pdk::sort_versions(versions);
  CHECK(versions[0].hash == "c");
  CHECK(versions[1].hash == "a");
  CHECK(versions[2].hash == "b");
  const pdk::PdkVersion v{"0fe599b2afb6708d281543108caf8310912f54af", "2024-08-17T01:34:30Z", std::nullopt, false};
  CHECK(v.short_form() == "0fe599b (2024.08.17)");
  CHECK(pdk::is_version_hash(v.hash));
  CHECK_FALSE(pdk::is_version_hash("0fe599b"));
  CHECK_FALSE(pdk::is_version_hash("0FE599B2AFB6708D281543108CAF8310912F54AF"));
}

TEST_CASE("catalog manifests with sha256 and size; unknown keys ignored", "[cap:hefesto-pdk-manager][req:PDK catalog]") {
  const auto versions = pdk::parse_family_manifest(R"({"pdk":"sky130","x":1,"versions":[
    {"version":"0fe599b2afb6708d281543108caf8310912f54af","date":"2024-08-17T01:34:30Z","y":2},
    {"version":"bdc9412b3e468c102d01b7cf6337be06ec6e9c9a","date":"2024-01-01T00:00:00Z","prerelease":true}]})");
  REQUIRE(versions.ok());
  REQUIRE(versions.value().size() == 2);
  CHECK(versions.value()[1].prerelease);
  const std::string digest(64, 'a');
  const auto assets = pdk::parse_version_manifest(R"({"assets":[{"content":"common","filename":"common.tar.zst",
    "url":"https://example.org/common.tar.zst","sha256":")" + std::string(64, 'A') + R"(","size":42,"z":0}]})");
  REQUIRE(assets.ok());
  CHECK(assets.value()[0].sha256 == digest);  // lower-cased
  CHECK(assets.value()[0].size == 42);
  CHECK_FALSE(pdk::parse_version_manifest(R"({"assets":[{"content":"common","filename":"c","url":"u","sha256":"12"}]})").ok());
  CHECK_FALSE(pdk::parse_family_manifest(R"({"versions":[{"version":"short"}]})").ok());
}

TEST_CASE("GitHub releases: tags, dates, drafts and assets",
          "[cap:hefesto-pdk-manager][req:GitHub releases source compatibility]") {
  const auto& sky = *catalog().find_family("sky130");
  const auto versions = pdk::parse_github_releases(R"json([
    {"tag_name":"sky130-0fe599b2afb6708d281543108caf8310912f54af","draft":false,"prerelease":false,
     "published_at":"2025-04-17T21:39:37Z",
     "body":"sky130 variants built using open_pdks 0fe599b (released on 2024-08-17T01:34:30Z)"},
    {"tag_name":"sky130A-1111111111111111111111111111111111111111","draft":false,"prerelease":true,"body":""},
    {"tag_name":"sky130-2222222222222222222222222222222222222222","draft":true,"body":""},
    {"tag_name":"gf180mcu-3333333333333333333333333333333333333333","draft":false,"body":""}])json",
                                                   sky);
  REQUIRE(versions.ok());
  REQUIRE(versions.value().size() == 2);
  CHECK(versions.value()[0].commit_date == "2024-08-17T01:34:30Z");
  CHECK(versions.value()[0].upload_date == "2025-04-17T21:39:37Z");
  CHECK(versions.value()[1].hash == "1111111111111111111111111111111111111111");
  CHECK(versions.value()[1].prerelease);
  CHECK_FALSE(versions.value()[1].commit_date.has_value());

  const auto assets = pdk::parse_github_release_assets(R"({"assets":[
    {"name":"common.tar.zst","browser_download_url":"https://x/common.tar.zst","size":10,"digest":"sha256:)" +
                                                       std::string(64, 'b') + R"("},
    {"name":"notes.txt","browser_download_url":"https://x/notes.txt","size":1},
    {"name":"sky130_fd_sc_hd.tar.zst","browser_download_url":"https://x/hd.tar.zst","size":20}]})");
  REQUIRE(assets.ok());
  REQUIRE(assets.value().size() == 2);
  CHECK(assets.value()[0].content == "common");
  CHECK(assets.value()[0].sha256 == std::string(64, 'b'));
  CHECK(assets.value()[1].content == "sky130_fd_sc_hd");
  CHECK(assets.value()[1].sha256.empty());

  const auto& ihp = *catalog().find_family("ihp-sg13");
  CHECK(pdk::github_release_tags(ihp, "h") == std::vector<std::string>{"ihp-sg13-h", "ihp-sg13g2-h", "ihp-sg13cmos5l-h"});
}
