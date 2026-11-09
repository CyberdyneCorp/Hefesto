// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// PDK updates (capability hefesto-pdk-manager "Updates"): which installed
// families have a newer stable version in the catalog, and the policy of
// the automatic check (at most once per 24 hours, off with the setting
// `pdk-update-check`). Nothing here runs on its own: the check happens when
// the user asks (`pdk updates`) or when a front end asks for the automatic
// check (`pdk updates -auto`), which honors the setting and the interval and
// otherwise touches no network. Installing an update never moves a pin.
//
// Owner: PDK (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/pdk/manager.hpp"

namespace hefesto::pdk {

/// Minimum time between two automatic checks.
inline constexpr std::chrono::hours kUpdateCheckInterval{24};

/// A family with a newer stable version than its newest installed one.
struct UpdateOffer {
  std::string family;
  PdkVersion installed;  // newest installed version (date from the catalog when listed)
  PdkVersion offered;    // newest stable catalog version
};

/// Compares every family that has an installed version with the catalog
/// (one remote_versions() call per such family), in catalog order.
[[nodiscard]] diag::Outcome<std::vector<UpdateOffer>> check_updates(PdkManager& manager, const CatalogSource& source);

/// True when an automatic check is allowed now: enabled, and no check was
/// recorded within kUpdateCheckInterval (a recorded time in the future,
/// e.g. after a clock change, does not block the check).
[[nodiscard]] bool auto_update_check_due(bool enabled, std::optional<std::chrono::system_clock::time_point> last,
                                         std::chrono::system_clock::time_point now) noexcept;

/// The time of the last recorded check (`<root>/ciel/.hefesto-update-check`,
/// seconds since the epoch); std::nullopt when missing or malformed.
[[nodiscard]] std::optional<std::chrono::system_clock::time_point> last_update_check(const platform::Path& root);
[[nodiscard]] diag::Status record_update_check(const platform::Path& root, std::chrono::system_clock::time_point now);

}  // namespace hefesto::pdk
