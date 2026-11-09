// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// PDK families, variants, library sets, versions and the catalog
// (capability hefesto-pdk-manager: "PDK families and variants", "Library
// sets per variant", "Version identity and ordering", "PDK catalog",
// "GitHub releases source compatibility").
//
// Owner: PDK (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/diag/outcome.hpp"

namespace hefesto::pdk {

struct Variant {
  std::string name;                          // e.g. "sky130A"
  std::vector<std::string> libraries;        // all available libraries
  std::vector<std::string> default_libraries;
};

struct Family {
  std::string name;              // "sky130", "gf180mcu", "ihp-sg13"
  std::string upstream_repo;     // "open_pdks" or "IHP-Open-PDK" (SOURCES line)
  std::vector<Variant> variants;  // default first
  bool manufacturable = true;
  std::string license = "Apache-2.0";  // SPDX identifier of the upstream PDK

  /// The default variant (the first one); nullptr for an empty family.
  [[nodiscard]] const Variant* default_variant() const noexcept {
    return variants.empty() ? nullptr : &variants.front();
  }
  [[nodiscard]] const Variant* find_variant(std::string_view variant_name) const noexcept;
};

/// A resolved selector: family plus the variant (the default when the
/// selector named the family).
struct Selection {
  const Family* family = nullptr;
  const Variant* variant = nullptr;
  bool variant_explicit = false;
};

/// A version: full 40-hex commit of the upstream repository.
struct PdkVersion {
  std::string hash;
  std::optional<std::string> commit_date;  // "YYYY-MM-DDTHH:MM:SSZ"
  std::optional<std::string> upload_date;
  bool prerelease = false;

  /// "abcdef1 (2025.01.02)" short form.
  [[nodiscard]] std::string short_form() const;
};

/// Newest commit date first; versions without a date sort last.
void sort_versions(std::vector<PdkVersion>& versions);

/// One downloadable asset of a version ("common" or a library name).
struct Asset {
  std::string content;
  std::string filename;  // <name>.tar.zst
  std::string url;       // https
  std::string sha256;    // 64 hex
  std::uint64_t size = 0;
};

/// The family table and library sets, as catalog data (not only constants).
class Catalog {
 public:
  explicit Catalog(std::vector<Family> families = {}) : families_(std::move(families)) {}

  /// The built-in table of the spec (sky130, gf180mcu, ihp-sg13), parsed
  /// from the embedded catalog document (data, not constants: from_json
  /// with an updated document adds families or libraries).
  [[nodiscard]] static Catalog builtin();
  /// Parses a family table document:
  /// {"families":[{"name","repo","license","manufacturable","variants":[
  ///   {"name","libraries":[...],"default":[...]}]}]} (unknown keys ignored).
  [[nodiscard]] static diag::Outcome<Catalog> from_json(std::string_view json);

  [[nodiscard]] const Family* find_family(std::string_view name) const noexcept;

  [[nodiscard]] const std::vector<Family>& families() const noexcept { return families_; }
  /// Variant name, family name, or the legacy `ihp_sg13g2`; PDK- error naming the selector otherwise.
  [[nodiscard]] diag::Outcome<Selection> resolve(std::string_view selector) const;
  /// `all`, `default` or library names; empty request = default set.
  [[nodiscard]] diag::Outcome<std::vector<std::string>> resolve_libraries(const Variant& variant,
                                                                          const std::vector<std::string>& request) const;

 private:
  std::vector<Family> families_;
};

/// Parses `<base>/<family>/manifest.json` (unknown keys ignored).
[[nodiscard]] diag::Outcome<std::vector<PdkVersion>> parse_family_manifest(std::string_view json);
/// Parses `<base>/<family>/<hash>/manifest.json`. `sha256` and `size`
/// are optional (ciel's own static-web source has neither); a missing
/// digest leaves `sha256` empty, which install treats as unverified.
[[nodiscard]] diag::Outcome<std::vector<Asset>> parse_version_manifest(std::string_view json);

/// True for 40 lower-case hex digits ("Version identity and ordering").
[[nodiscard]] bool is_version_hash(std::string_view text) noexcept;

// --- GitHub releases source ("GitHub releases source compatibility") -------

/// Parses a GitHub `GET /repos/{owner}/{repo}/releases` array into the
/// versions of `family`: non-draft releases whose tag `<prefix>-<hash>`
/// (split at the last '-') has the family or one of its variants as
/// prefix; commit date from the first `released on ([\d\-:TZ]+)` in the
/// body, upload date from `published_at`, prerelease from the release.
[[nodiscard]] diag::Outcome<std::vector<PdkVersion>> parse_github_releases(std::string_view json,
                                                                           const Family& family);
/// Parses one release object: the assets ending in `.tar.zst`, content =
/// file name without the suffix, SHA-256 from a `sha256:` digest when the
/// asset carries one.
[[nodiscard]] diag::Outcome<std::vector<Asset>> parse_github_release_assets(std::string_view json);
/// Tags to try for a version, in order: `<family>-<hash>`, then
/// `<variant>-<hash>` for each variant in table order.
[[nodiscard]] std::vector<std::string> github_release_tags(const Family& family, std::string_view hash);

}  // namespace hefesto::pdk
