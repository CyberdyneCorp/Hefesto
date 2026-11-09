// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Offline PDK packs (capability hefesto-pdk-manager "Offline PDK packs";
// hefesto-files-and-pdks "PDK store": installing by importing a pack). A
// pack is one uncompressed tar file `<family>-<hash>.hefestopdk` holding
// `pack.json` and the asset `.tar.zst` files it names.
//
// Export uses the original assets when they are still in the download
// cache and match the recorded SHA-256; otherwise it rebuilds one asset per
// part (`common`, each library) from the installed files and records
// `"rebuilt": true`. Rebuilt assets are stored Zstandard frames (the core
// has no Zstandard compressor) and never carry a catalog signature.
//
// Import unpacks the pack safely into a scratch directory under the PDK
// root, verifies every asset against pack.json (size and SHA-256) and
// installs it as for a download, with no network access. No Hefesto
// catalog key exists yet, so every pack is unsigned: its install needs
// -accept-unverified and is recorded untrusted ("Script trust for managed
// PDKs").
//
// Owner: PDK (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/pdk/manager.hpp"

namespace hefesto::pdk {

inline constexpr std::string_view kPackExtension = ".hefestopdk";
inline constexpr std::string_view kPackManifestName = "pack.json";

/// The contents of pack.json.
struct PackManifest {
  std::string family;
  std::vector<std::string> variants;
  std::string version;  // full 40-hex hash
  std::optional<std::string> date;
  std::vector<std::string> libraries;
  std::vector<Asset> assets;  // content, filename, sha256, size (url empty)
  std::vector<std::string> license;  // SPDX identifiers
  std::string source;                // catalog location or "local"
  bool rebuilt = false;
  std::vector<std::string> partial;  // include prefixes of a filtered install
};

/// Parses pack.json. PDK-PACK-001 when it is malformed: missing fields,
/// a version that is not 40 hex digits, an asset file name with a path
/// separator or a leading '.', or a malformed SHA-256.
[[nodiscard]] diag::Outcome<PackManifest> parse_pack_manifest(std::string_view text);
[[nodiscard]] std::string pack_manifest_json(const PackManifest& manifest);

/// Writes `<out_dir>/<family>-<hash>.hefestopdk` for an installed version
/// and returns its path; PDK-INSTALL-004 when the version is not installed.
[[nodiscard]] diag::Outcome<platform::Path> export_pack(const PdkManager& manager, const Family& family,
                                                        std::string_view hash, const platform::Path& out_dir,
                                                        diag::DiagnosticSink& sink);

/// Imports a pack into the manager's root and returns its manifest.
/// `options.source`, `commit_date` and `include_prefixes` are taken from the
/// pack; the consent flags and `progress` come from the caller.
[[nodiscard]] diag::Outcome<PackManifest> import_pack(PdkManager& manager, const platform::Path& pack,
                                                      const InstallOptions& options, diag::DiagnosticSink& sink);

}  // namespace hefesto::pdk
