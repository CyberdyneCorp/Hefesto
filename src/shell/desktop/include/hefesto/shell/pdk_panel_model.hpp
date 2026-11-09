// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The PDK manager panel (capability hefesto-pdk-manager "Headless and
// scripted operation": the UI runs the same `pdk` commands as scripts, so
// every action goes through the core Dispatcher and the command log). This
// model lists the catalog's families and variants with their install state
// (status as shape + text) and turns the panel's buttons into `pdk`
// command lines. Toolkit-free and unit-tested.
//
// Owner: SHELL (openspec/changes/m1-core-viewer/design.md, D2 and D6).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/pdk/catalog.hpp"
#include "hefesto/pdk/manager.hpp"
#include "hefesto/shell/view_models.hpp"

namespace hefesto::shell {

/// The panel's buttons. Download verification (size + SHA-256 of every
/// asset, "Download integrity verification") is part of `install`; there is
/// no separate verify command.
enum class PdkAction : std::uint8_t { list, remote, install, enable, pin, uninstall };

/// Explicit acceptances an install may need ("Headless and scripted
/// operation": never a prompt; the flags are the `pdk install` options).
struct PdkInstallFlags {
  bool accept_license = false;     // -accept-license
  bool accept_unverified = false;  // -accept-unverified
};

/// Button text for an action ("Install", "Verify", ...).
[[nodiscard]] std::string_view pdk_action_label(PdkAction action) noexcept;

/// The command line an action runs for `variant` (e.g. "sky130A") and an
/// optional version hash: "pdk install sky130A <hash> -accept-license",
/// "pdk pin sky130A <hash>", "pdk list", "pdk remote sky130". Returns
/// nullopt when the action needs a version and none is given.
[[nodiscard]] std::optional<std::string> pdk_command(PdkAction action, std::string_view family,
                                                     std::string_view variant, std::string_view version,
                                                     PdkInstallFlags flags = {});

struct PdkRow {
  std::string family;
  std::string variant;
  std::size_t default_libraries = 0;
  std::optional<std::string> installed_version;  // short form of the enabled or newest installed
  std::optional<std::string> installed_hash;     // its full 40-hex hash (for the commands)
  bool enabled = false;
  StatusChip state;  // "● enabled abcdef1", "◆ installed", "◆ not installed"

  [[nodiscard]] std::string title() const { return variant + "  (" + family + ")"; }
};

/// One row per variant of `catalog`, in catalog order, with the state found
/// in `installed`.
[[nodiscard]] std::vector<PdkRow> pdk_rows(const pdk::Catalog& catalog,
                                           const std::vector<pdk::InstalledVersion>& installed);

}  // namespace hefesto::shell
