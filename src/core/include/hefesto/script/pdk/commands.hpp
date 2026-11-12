// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Engine commands of the PDK command surface owned by PDKINT
// (openspec/changes/m4-scripting/design.md D2/D14; capability
// scripting-pdk-integration "Command surface used by device generators",
// "PDK startup file contract" step 5): `ext2spice scale` as a setting.
// The rest of `ext2spice` arrives with extraction (M7) and answers
// CORE-NOTIMPL-004 until then.
//
// Owner: PDKINT (design D2).

#pragma once

#include <string_view>

#include "hefesto/cmd/registry.hpp"

namespace hefesto::script {

/// The setting `ext2spice scale` writes (session layer of
/// editor::EditorState::settings): "on" (Magic's default, `.option scale`
/// card) or "off".
inline constexpr std::string_view kExt2spiceScale = "ext2spice-scale";

/// Registers `ext2spice` (layout table, as Magic's ext2spice module does).
/// False when a command of that name already exists.
[[nodiscard]] bool register_pdk_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::script
