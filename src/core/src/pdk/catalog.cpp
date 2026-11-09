// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// PDK catalog: family table, selectors, library sets, versions, manifests
// (hefesto-pdk-manager). Family table and library resolution follow ciel
// (ciel ref: ciel/families.py:59-311, Apache-2.0); the GitHub source
// follows ciel/source.py:54-153.

#include "hefesto/pdk/catalog.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <set>

#include <nlohmann/json.hpp>

#include "hefesto/pdk/diag_codes.hpp"

namespace hefesto::pdk {

namespace {

using nlohmann::json;

// The built-in family table ("PDK families and variants", "Library sets per
// variant"). Variants are listed default first; `libraries` is every
// library available for the variant, `default` its default set.
constexpr std::string_view kBuiltinCatalog = R"json({
  "families": [
    {"name": "sky130", "repo": "open_pdks", "license": "Apache-2.0", "variants": [
      {"name": "sky130A",
       "libraries": ["sky130_fd_io", "sky130_fd_pr", "sky130_ml_xx_hd", "sky130_fd_sc_hd", "sky130_fd_sc_hdll",
                     "sky130_fd_sc_lp", "sky130_fd_sc_hvl", "sky130_fd_sc_ls", "sky130_fd_sc_ms", "sky130_fd_sc_hs",
                     "sky130_sram_macros"],
       "default": ["sky130_fd_io", "sky130_fd_pr", "sky130_fd_sc_hd", "sky130_fd_sc_hvl", "sky130_ml_xx_hd",
                   "sky130_sram_macros"]},
      {"name": "sky130B",
       "libraries": ["sky130_fd_io", "sky130_fd_pr", "sky130_ml_xx_hd", "sky130_fd_sc_hd", "sky130_fd_sc_hdll",
                     "sky130_fd_sc_lp", "sky130_fd_sc_hvl", "sky130_fd_sc_ls", "sky130_fd_sc_ms", "sky130_fd_sc_hs",
                     "sky130_sram_macros", "sky130_fd_pr_reram"],
       "default": ["sky130_fd_io", "sky130_fd_pr", "sky130_fd_sc_hd", "sky130_fd_sc_hvl", "sky130_ml_xx_hd",
                   "sky130_sram_macros", "sky130_fd_pr_reram"]}]},
    {"name": "gf180mcu", "repo": "open_pdks", "license": "Apache-2.0", "variants": [
      {"name": "gf180mcuD", "inherit": "gf180mcu"},
      {"name": "gf180mcuA", "inherit": "gf180mcu"},
      {"name": "gf180mcuB", "inherit": "gf180mcu"},
      {"name": "gf180mcuC", "inherit": "gf180mcu"}],
     "shared": {
       "libraries": ["gf180mcu_fd_io", "gf180mcu_fd_pr", "gf180mcu_fd_sc_mcu7t5v0", "gf180mcu_fd_sc_mcu9t5v0",
                     "gf180mcu_fd_ip_sram", "gf180mcu_osu_sc_gp12t3v3", "gf180mcu_osu_sc_gp9t3v3",
                     "gf180mcu_as_sc_mcu7t3v3", "gf180mcu_re_efuse", "gf180mcu_ocd_io", "gf180mcu_ocd_ip_sram",
                     "gf180mcu_ocd_sc_rt12t3v3", "gf180mcu_ocd_alpha_small", "gf180mcu_ocd_alpha_large",
                     "gf180mcu_ocd_alpha_misc"],
       "default": ["gf180mcu_fd_io", "gf180mcu_fd_pr", "gf180mcu_fd_sc_mcu7t5v0", "gf180mcu_fd_sc_mcu9t5v0",
                   "gf180mcu_fd_ip_sram"]}},
    {"name": "ihp-sg13", "repo": "IHP-Open-PDK", "license": "Apache-2.0", "variants": [
      {"name": "ihp-sg13g2",
       "libraries": ["sg13g2_io", "sg13g2_pr", "sg13g2_sram", "sg13g2_stdcell"],
       "default": ["sg13g2_io", "sg13g2_pr", "sg13g2_sram", "sg13g2_stdcell"]},
      {"name": "ihp-sg13cmos5l",
       "libraries": ["sg13cmos5l_io", "sg13cmos5l_sram", "sg13cmos5l_stdcell"],
       "default": ["sg13cmos5l_io", "sg13cmos5l_sram", "sg13cmos5l_stdcell"]}]}
  ]
})json";

diag::Diagnostic manifest_error(std::string text) {
  return diag::make(codes::kManifest, std::move(text));
}

std::vector<std::string> string_list(const json& object, std::string_view key) {
  std::vector<std::string> out;
  const auto it = object.find(key);
  if (it == object.end() || !it->is_array()) {
    return out;
  }
  for (const auto& item : *it) {
    if (item.is_string()) {
      out.push_back(item.get<std::string>());
    }
  }
  return out;
}

std::string string_field(const json& object, std::string_view key, std::string fallback = {}) {
  const auto it = object.find(key);
  return it != object.end() && it->is_string() ? it->get<std::string>() : fallback;
}

std::optional<std::string> optional_string(const json& object, std::string_view key) {
  const auto it = object.find(key);
  if (it == object.end() || !it->is_string()) {
    return std::nullopt;
  }
  return it->get<std::string>();
}

bool bool_field(const json& object, std::string_view key, bool fallback) {
  const auto it = object.find(key);
  return it != object.end() && it->is_boolean() ? it->get<bool>() : fallback;
}

Variant parse_variant(const json& item, const json& family) {
  Variant v{string_field(item, "name"), string_list(item, "libraries"), string_list(item, "default")};
  if (item.contains("inherit") && family.contains("shared") && family["shared"].is_object()) {
    v.libraries = string_list(family["shared"], "libraries");
    v.default_libraries = string_list(family["shared"], "default");
  }
  return v;
}

diag::Outcome<Family> parse_family(const json& item) {
  if (!item.is_object() || string_field(item, "name").empty()) {
    return manifest_error("catalog family entry without a name");
  }
  Family family;
  family.name = string_field(item, "name");
  family.upstream_repo = string_field(item, "repo", "open_pdks");
  family.license = string_field(item, "license", "Apache-2.0");
  family.manufacturable = bool_field(item, "manufacturable", true);
  const auto variants = item.find("variants");
  if (variants == item.end() || !variants->is_array() || variants->empty()) {
    return manifest_error("catalog family '" + family.name + "' has no variants");
  }
  for (const auto& v : *variants) {
    if (!v.is_object() || string_field(v, "name").empty()) {
      return manifest_error("catalog family '" + family.name + "' has a variant without a name");
    }
    family.variants.push_back(parse_variant(v, item));
  }
  return family;
}

bool contains(const std::vector<std::string>& list, std::string_view name) {
  return std::find(list.begin(), list.end(), name) != list.end();
}

void add_unique(std::vector<std::string>& list, const std::string& name) {
  if (!contains(list, name)) {
    list.push_back(name);
  }
}

// "YYYY-MM-DDT..." -> "YYYY.MM.DD"; empty when the text is too short.
std::string dotted_date(std::string_view iso) {
  if (iso.size() < 10) {
    return {};
  }
  std::string out{iso.substr(0, 10)};
  std::replace(out.begin(), out.end(), '-', '.');
  return out;
}

diag::Outcome<json> parse_json(std::string_view text, std::string_view what) {
  json doc = json::parse(text, nullptr, false);
  if (doc.is_discarded()) {
    return manifest_error(std::string{what} + " is not valid JSON");
  }
  return doc;
}

diag::Outcome<Asset> parse_asset(const json& item) {
  if (!item.is_object()) {
    return manifest_error("version manifest asset is not an object");
  }
  Asset asset;
  asset.content = string_field(item, "content");
  asset.filename = string_field(item, "filename");
  asset.url = string_field(item, "url");
  asset.sha256 = string_field(item, "sha256");
  std::transform(asset.sha256.begin(), asset.sha256.end(), asset.sha256.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (const auto size = item.find("size"); size != item.end() && size->is_number_unsigned()) {
    asset.size = size->get<std::uint64_t>();
  }
  if (asset.content.empty() || asset.filename.empty() || asset.url.empty()) {
    return manifest_error("version manifest asset needs content, filename and url");
  }
  if (!asset.sha256.empty() && (asset.sha256.size() != 64 ||
                                asset.sha256.find_first_not_of("0123456789abcdef") != std::string::npos)) {
    return manifest_error("asset '" + asset.filename + "' has a malformed sha256");
  }
  return asset;
}

// Prefix and hash of a release tag split at the last '-'.
std::optional<std::pair<std::string, std::string>> split_tag(std::string_view tag) {
  const auto dash = tag.rfind('-');
  if (dash == std::string_view::npos) {
    return std::nullopt;
  }
  return std::pair{std::string{tag.substr(0, dash)}, std::string{tag.substr(dash + 1)}};
}

bool tag_prefix_matches(const Family& family, std::string_view prefix) {
  return prefix == family.name || family.find_variant(prefix) != nullptr;
}

std::optional<std::string> released_on(const std::string& body) {
  static const std::regex kReleasedOn{R"(released on ([\d\-:TZ]+))"};
  std::smatch match;
  if (std::regex_search(body, match, kReleasedOn)) {
    return match[1].str();
  }
  return std::nullopt;
}

std::optional<PdkVersion> github_version(const json& release, const Family& family) {
  if (!release.is_object() || bool_field(release, "draft", false)) {
    return std::nullopt;
  }
  const auto split = split_tag(string_field(release, "tag_name"));
  if (!split || !tag_prefix_matches(family, split->first)) {
    return std::nullopt;
  }
  PdkVersion version;
  version.hash = split->second;
  version.commit_date = released_on(string_field(release, "body"));
  version.upload_date = optional_string(release, "published_at");
  version.prerelease = bool_field(release, "prerelease", false);
  return version;
}

}  // namespace

const Variant* Family::find_variant(std::string_view variant_name) const noexcept {
  const auto it =
      std::find_if(variants.begin(), variants.end(), [&](const Variant& v) { return v.name == variant_name; });
  return it == variants.end() ? nullptr : &*it;
}

std::string PdkVersion::short_form() const {
  std::string out = hash.substr(0, 7);
  if (commit_date) {
    if (const std::string date = dotted_date(*commit_date); !date.empty()) {
      out += " (" + date + ")";
    }
  }
  return out;
}

void sort_versions(std::vector<PdkVersion>& versions) {
  // Newest commit date first; ISO 8601 UTC strings compare chronologically.
  std::stable_sort(versions.begin(), versions.end(), [](const PdkVersion& a, const PdkVersion& b) {
    if (a.commit_date.has_value() != b.commit_date.has_value()) {
      return a.commit_date.has_value();
    }
    return a.commit_date.has_value() && *a.commit_date > *b.commit_date;
  });
}

Catalog Catalog::builtin() {
  auto catalog = from_json(kBuiltinCatalog);
  return catalog.ok() ? std::move(catalog).value() : Catalog{};
}

diag::Outcome<Catalog> Catalog::from_json(std::string_view text) {
  auto doc = parse_json(text, "catalog family table");
  if (!doc) {
    return doc.error();
  }
  const json& root = doc.value();
  if (!root.is_object() || !root.contains("families") || !root["families"].is_array()) {
    return manifest_error("catalog family table has no \"families\" array");
  }
  std::vector<Family> families;
  for (const auto& item : root["families"]) {
    auto family = parse_family(item);
    if (!family) {
      return family.error();
    }
    families.push_back(std::move(family).value());
  }
  return Catalog{std::move(families)};
}

const Family* Catalog::find_family(std::string_view name) const noexcept {
  const auto it = std::find_if(families_.begin(), families_.end(), [&](const Family& f) { return f.name == name; });
  return it == families_.end() ? nullptr : &*it;
}

diag::Outcome<Selection> Catalog::resolve(std::string_view selector) const {
  for (const auto& family : families_) {
    if (const Variant* variant = family.find_variant(selector)) {
      return Selection{&family, variant, true};
    }
  }
  const std::string_view name = selector == "ihp_sg13g2" ? std::string_view{"ihp-sg13"} : selector;
  if (const Family* family = find_family(name)) {
    return Selection{family, family->default_variant(), false};
  }
  return diag::make(codes::kSelect, "'" + std::string{selector} + "' is not a valid PDK family or variant.");
}

diag::Outcome<std::vector<std::string>> Catalog::resolve_libraries(const Variant& variant,
                                                                   const std::vector<std::string>& request) const {
  if (request.empty()) {
    return variant.default_libraries;
  }
  std::vector<std::string> out;
  for (const auto& element : request) {
    if (element == "all") {
      return variant.libraries;
    }
    if (element == "default") {
      for (const auto& lib : variant.default_libraries) {
        add_unique(out, lib);
      }
    } else if (contains(variant.libraries, element)) {
      add_unique(out, element);
    } else {
      return diag::make(codes::kLibrary, "Unknown library " + element + " for PDK variant " + variant.name);
    }
  }
  return out;
}

diag::Outcome<std::vector<PdkVersion>> parse_family_manifest(std::string_view text) {
  auto doc = parse_json(text, "family manifest");
  if (!doc) {
    return doc.error();
  }
  const json& root = doc.value();
  if (!root.is_object() || !root.contains("versions") || !root["versions"].is_array()) {
    return manifest_error("family manifest has no \"versions\" array");
  }
  std::vector<PdkVersion> versions;
  for (const auto& item : root["versions"]) {
    const std::string hash = item.is_object() ? string_field(item, "version") : std::string{};
    if (!is_version_hash(hash)) {
      return manifest_error("family manifest entry has no 40-hex \"version\"");
    }
    versions.push_back(PdkVersion{hash, optional_string(item, "date"), std::nullopt, bool_field(item, "prerelease", false)});
  }
  return versions;
}

diag::Outcome<std::vector<Asset>> parse_version_manifest(std::string_view text) {
  auto doc = parse_json(text, "version manifest");
  if (!doc) {
    return doc.error();
  }
  const json& root = doc.value();
  if (!root.is_object() || !root.contains("assets") || !root["assets"].is_array()) {
    return manifest_error("version manifest has no \"assets\" array");
  }
  std::vector<Asset> assets;
  for (const auto& item : root["assets"]) {
    auto asset = parse_asset(item);
    if (!asset) {
      return asset.error();
    }
    assets.push_back(std::move(asset).value());
  }
  return assets;
}

bool is_version_hash(std::string_view text) noexcept {
  return text.size() == 40 && text.find_first_not_of("0123456789abcdef") == std::string_view::npos;
}

diag::Outcome<std::vector<PdkVersion>> parse_github_releases(std::string_view text, const Family& family) {
  auto doc = parse_json(text, "GitHub release list");
  if (!doc) {
    return doc.error();
  }
  if (!doc.value().is_array()) {
    return manifest_error("GitHub release list is not an array");
  }
  std::vector<PdkVersion> versions;
  std::set<std::string> seen;
  for (const auto& release : doc.value()) {
    if (auto version = github_version(release, family); version && seen.insert(version->hash).second) {
      versions.push_back(std::move(*version));
    }
  }
  return versions;
}

diag::Outcome<std::vector<Asset>> parse_github_release_assets(std::string_view text) {
  auto doc = parse_json(text, "GitHub release");
  if (!doc) {
    return doc.error();
  }
  const json& release = doc.value();
  if (!release.is_object() || !release.contains("assets") || !release["assets"].is_array()) {
    return manifest_error("GitHub release has no \"assets\" array");
  }
  static constexpr std::string_view kSuffix = ".tar.zst";
  std::vector<Asset> assets;
  for (const auto& item : release["assets"]) {
    const std::string name = item.is_object() ? string_field(item, "name") : std::string{};
    if (name.size() <= kSuffix.size() || !name.ends_with(kSuffix)) {
      continue;
    }
    Asset asset;
    asset.filename = name;
    asset.content = name.substr(0, name.size() - kSuffix.size());
    asset.url = string_field(item, "browser_download_url");
    if (const std::string digest = string_field(item, "digest"); digest.starts_with("sha256:")) {
      asset.sha256 = digest.substr(7);
    }
    if (const auto size = item.find("size"); size != item.end() && size->is_number_unsigned()) {
      asset.size = size->get<std::uint64_t>();
    }
    assets.push_back(std::move(asset));
  }
  return assets;
}

std::vector<std::string> github_release_tags(const Family& family, std::string_view hash) {
  std::vector<std::string> tags{family.name + "-" + std::string{hash}};
  for (const auto& variant : family.variants) {
    tags.push_back(variant.name + "-" + std::string{hash});
  }
  return tags;
}

}  // namespace hefesto::pdk
