// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Where the unit tests find the pinned, gitignored reference data fetched by
// scripts/fetch_pdk.py (docs/building.md, "Reference data for tests"):
//   - the PDK data sets in .cache/pdks/ciel/<family>/versions/<pin>/:
//       sky130    sky130A tech files, the sky130_fd_sc_hd .mag library and the
//                 sky130_fd_pr .mag cells (fixed-layout PCell devices);
//       gf180mcu  gf180mcuD tech files, the gf180mcu_fd_sc_mcu7t5v0 .mag library
//                 and the gf180mcu_fd_pr .mag cells;
//       ihp       ihp-sg13g2 tech files (sg13g2_pr has no .mag cells);
//   - Magic's sources at fc5daa9e in build/ref/magic (scmos/, graphics/).
// Only the exact pinned versions are used, never another installed one, so a
// stale cache cannot silently change what a test checks; a test whose data
// set is missing skips. Owner: PDK (openspec/changes/m4-scripting/design.md D2, D15); the
// device libraries were added by NATIVE in M4 round 2.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace hefesto::test {

/// The open_pdks/ciel version of the sky130 and gf180mcu sets (OPEN_PDKS_PIN in scripts/fetch_pdk.py):
/// ciel release 5740c4a8, whose Magic files are those of the spec pin open_pdks c6df28c1 (Q-SCRIPT-9).
inline constexpr std::string_view kPdkVersion = "5740c4a81c42f9fe4af5ef2ee258a99151053d28";
/// The IHP-Open-PDK/ciel version of the ihp set (IHP_PIN in scripts/fetch_pdk.py).
inline constexpr std::string_view kIhpVersion = "5e6d592e4002946a4616f798c357f0f3c06cf3b6";

/// One pinned PDK data set: where it is installed and what a complete install holds.
struct PinnedPdk {
  std::string_view family;   // directory under .cache/pdks/ciel
  std::string_view version;  // the pin
  std::string_view variant;  // directory under the version directory
  std::string_view tech;     // the tech file in libs.tech/magic
  std::string_view library;  // the .mag library in libs.ref, or empty when the set has none
  std::string_view devices;  // the primitive-device .mag library in libs.ref, or empty
};

inline constexpr PinnedPdk kSky130A{"sky130", kPdkVersion, "sky130A", "sky130A.tech", "sky130_fd_sc_hd",
                                    "sky130_fd_pr"};
inline constexpr PinnedPdk kGf180mcuD{"gf180mcu", kPdkVersion, "gf180mcuD", "gf180mcuD.tech",
                                      "gf180mcu_fd_sc_mcu7t5v0", "gf180mcu_fd_pr"};
inline constexpr PinnedPdk kIhpSg13g2{"ihp-sg13", kIhpVersion, "ihp-sg13g2", "ihp-sg13g2.tech", "", ""};

/// `.cache/pdks/ciel/<family>/versions/<version>/<variant>` when it holds the
/// set's tech file and (if the set has them) its .mag libraries, else std::nullopt
/// (a cache fetched before the device libraries were added is incomplete).
inline std::optional<std::filesystem::path> pinned_variant(const PinnedPdk& pdk) {
  const std::filesystem::path root = std::filesystem::path{HEFESTO_SOURCE_DIR} / ".cache" / "pdks" / "ciel" /
                                     std::string{pdk.family} / "versions" / std::string{pdk.version} /
                                     std::string{pdk.variant};
  std::error_code ec;
  if (!std::filesystem::exists(root / "libs.tech/magic" / std::string{pdk.tech}, ec)) {
    return std::nullopt;
  }
  if (!pdk.library.empty() &&
      !std::filesystem::is_directory(root / "libs.ref" / std::string{pdk.library} / "mag", ec)) {
    return std::nullopt;
  }
  if (!pdk.devices.empty() &&
      !std::filesystem::is_directory(root / "libs.ref" / std::string{pdk.devices} / "mag", ec)) {
    return std::nullopt;
  }
  return root;
}

/// The set's pinned tech file, if fetched.
inline std::optional<std::filesystem::path> pinned_tech(const PinnedPdk& pdk) {
  const auto root = pinned_variant(pdk);
  return root ? std::optional{*root / "libs.tech/magic" / std::string{pdk.tech}} : std::nullopt;
}

/// The set's pinned .mag library directory (`libs.ref/<library>/mag`), if fetched and the set has one.
inline std::optional<std::filesystem::path> pinned_library(const PinnedPdk& pdk) {
  const auto root = pinned_variant(pdk);
  return root && !pdk.library.empty() ? std::optional{*root / "libs.ref" / std::string{pdk.library} / "mag"}
                                      : std::nullopt;
}

/// The set's pinned primitive-device .mag directory (`libs.ref/<devices>/mag`), if fetched and
/// the set has one: the cells the fixed-layout (`nocell`) PCell devices place.
inline std::optional<std::filesystem::path> pinned_devices(const PinnedPdk& pdk) {
  const auto root = pinned_variant(pdk);
  return root && !pdk.devices.empty() ? std::optional{*root / "libs.ref" / std::string{pdk.devices} / "mag"}
                                      : std::nullopt;
}

/// `.cache/pdks/ciel/sky130/versions/<kPdkVersion>/sky130A` when it holds the
/// tech files and the sky130_fd_sc_hd and sky130_fd_pr .mag libraries, else std::nullopt.
inline std::optional<std::filesystem::path> pinned_sky130a() { return pinned_variant(kSky130A); }

/// The pinned sky130A.tech, if fetched.
inline std::optional<std::filesystem::path> pinned_sky130a_tech() { return pinned_tech(kSky130A); }

/// The pinned gf180mcuD variant directory (tech files and the 7t5v0 .mag library), if fetched.
inline std::optional<std::filesystem::path> pinned_gf180mcud() { return pinned_variant(kGf180mcuD); }

/// The pinned gf180mcuD.tech, if fetched.
inline std::optional<std::filesystem::path> pinned_gf180mcud_tech() { return pinned_tech(kGf180mcuD); }

/// The pinned gf180mcu_fd_sc_mcu7t5v0 .mag directory, if fetched.
inline std::optional<std::filesystem::path> pinned_gf180mcu_7t5v0_mag() { return pinned_library(kGf180mcuD); }

/// The pinned ihp-sg13g2 variant directory (tech files only), if fetched.
inline std::optional<std::filesystem::path> pinned_ihp_sg13g2() { return pinned_variant(kIhpSg13g2); }

/// The pinned ihp-sg13g2.tech, if fetched.
inline std::optional<std::filesystem::path> pinned_ihp_sg13g2_tech() { return pinned_tech(kIhpSg13g2); }

}  // namespace hefesto::test
