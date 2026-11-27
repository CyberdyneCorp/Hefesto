// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `*iroute` and `*mzroute` diagnostic commands (capability
// router-maze-iroute; openspec/changes/m10-plot-route/design.md D12,
// "Area contracts" IROUTE).
//
// Until IROUTE lands, both fail with CORE-NOTIMPL-010.
// cmd::register_all_commands calls register_route_diag_commands.
//
// Owner: IROUTE.

#pragma once

#include "hefesto/cmd/registry.hpp"

namespace hefesto::route {

/// Adds `*iroute` and `*mzroute` to `registry`; false on a duplicate
/// (CMD-REGISTRY-001).
[[nodiscard]] bool register_route_diag_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::route
