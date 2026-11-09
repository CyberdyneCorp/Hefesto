// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Where the unit tests find the pinned, gitignored reference data fetched by
// scripts/fetch_pdk.py (docs/building.md, "Reference data for tests"):
//   - the sky130A test subset of open_pdks at kPdkVersion in .cache/pdks;
//   - Magic's sources at fc5daa9e in build/ref/magic (scmos/, graphics/).
// Only the exact pinned PDK version is used, never another installed one, so
// a stale cache cannot silently change what a test checks. Owner: INTEGRATION.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace hefesto::test {

/// The ciel/open_pdks version scripts/fetch_pdk.py installs (PDK_VERSION there).
inline constexpr std::string_view kPdkVersion = "0fe599b2afb6708d281543108caf8310912f54af";

/// `.cache/pdks/ciel/sky130/versions/<kPdkVersion>/sky130A` when it holds the
/// tech files and the sky130_fd_sc_hd .mag library, else std::nullopt.
inline std::optional<std::filesystem::path> pinned_sky130a() {
  const std::filesystem::path root = std::filesystem::path{HEFESTO_SOURCE_DIR} / ".cache" / "pdks" / "ciel" /
                                     "sky130" / "versions" / std::string{kPdkVersion} / "sky130A";
  std::error_code ec;
  if (std::filesystem::exists(root / "libs.tech/magic/sky130A.tech", ec) &&
      std::filesystem::is_directory(root / "libs.ref/sky130_fd_sc_hd/mag", ec)) {
    return root;
  }
  return std::nullopt;
}

/// The pinned sky130A.tech, if fetched.
inline std::optional<std::filesystem::path> pinned_sky130a_tech() {
  const auto root = pinned_sky130a();
  return root ? std::optional{*root / "libs.tech/magic/sky130A.tech"} : std::nullopt;
}

}  // namespace hefesto::test
