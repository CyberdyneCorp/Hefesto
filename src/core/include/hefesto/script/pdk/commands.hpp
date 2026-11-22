// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Engine commands of the PDK command surface owned by PDKINT
// (openspec/changes/m4-scripting/design.md D2/D14; capability
// scripting-pdk-integration "Command surface used by device generators",
// "PDK startup file contract" step 5): `ext2spice scale` as a setting.
// The `ext2spice` command is M7's (netlist/spice/spice.hpp), whose `scale`
// option is SpiceSettings::scale (user decision M7-3, 2026-11-22);
// register_pdk_commands() is kept for sessions built without the M7
// registration.
//
// Owner: PDKINT (design D2).

#pragma once

#include <string_view>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/registry.hpp"

namespace hefesto::script {

/// The setting M4's partial `ext2spice scale` writes (session layer of
/// editor::EditorState::settings): "on" (Magic's default, `.option scale`
/// card) or "off". The M7 command keeps it in SpiceSettings::scale instead.
inline constexpr std::string_view kExt2spiceScale = "ext2spice-scale";

/// Registers `ext2spice` (layout table, as Magic's ext2spice module does).
/// False when a command of that name already exists.
[[nodiscard]] bool register_pdk_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::script
