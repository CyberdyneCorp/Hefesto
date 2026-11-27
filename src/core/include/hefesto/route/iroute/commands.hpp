// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `iroute` command and `iroute saveParameters` (capability
// router-maze-iroute; openspec/changes/m10-plot-route/design.md D12, D14,
// "Area contracts" IROUTE).
//
// Until IROUTE lands, `iroute` fails with CORE-NOTIMPL-010.
// cmd::register_all_commands calls register_iroute_commands.
//
// Owner: IROUTE.

#pragma once

#include <ostream>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/route/iroute/session.hpp"

namespace hefesto::tech {
class Technology;
}

namespace hefesto::route {

/// Adds `iroute` to `registry`; false on a duplicate (CMD-REGISTRY-001).
[[nodiscard]] bool register_iroute_commands(cmd::CommandRegistry& registry);

/// `iroute saveParameters`: Magic's text for `session` (spacings formatted
/// per Q-LEGACY-43, branch point of design D5).
[[nodiscard]] diag::Status save_parameters(const IrouteSession& session, const tech::Technology& tech,
                                           bool magic_compat, std::ostream& out);

}  // namespace hefesto::route
