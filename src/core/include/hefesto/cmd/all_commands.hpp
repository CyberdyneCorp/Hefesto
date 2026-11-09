// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The full command set every front end registers: the M0 built-ins plus
// each M1 area's commands (tech, db, cell, view, pdk). Front ends call this
// instead of the per-area functions, so a workstream adds commands inside
// its own directory without touching hefesto-cli or the desktop shell
// (openspec/changes/m1-core-viewer/design.md, D4).

#pragma once

#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/pdk/commands.hpp"

namespace hefesto::cmd {

/// Registers every command. Returns false if any name was taken (each area
/// still registers the rest of its commands).
bool register_all_commands(CommandRegistry& registry);
/// The same, with the front end's PDK host mechanisms for `pdk` (hefesto-cli
/// and the desktop pass the platform_net HTTPS Fetcher).
bool register_all_commands(CommandRegistry& registry, pdk::CommandHosts pdk_hosts);

/// The editor session of `ctx`, or CMD-SESSION-001 when the front end runs
/// without one. Typical use in a handler:
///   auto editor = require_editor(ctx);
///   if (!editor) return CommandResult::failure(editor.error());
[[nodiscard]] diag::Outcome<editor::EditorState*> require_editor(const CommandContext& ctx);

}  // namespace hefesto::cmd
