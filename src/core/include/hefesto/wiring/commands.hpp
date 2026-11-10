// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `wire` command and every subcommand (capability wiring-tool), and
// `trackwire`, the native form of Magic's Tcl `magic::trackwire` that the
// wiring tool's buttons run ("Wiring tool buttons"), registered by
// cmd::register_all_commands().
// (Magic ref: commands/CmdTZ.c:1689-2170 CmdWire, tcltk/tools.tcl:519-640)
//
// Owner: WIRE (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <optional>
#include <span>
#include <string_view>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/registry.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::wiring {

/// Adds `wire` and `trackwire`. Returns false if a name was taken.
bool register_wiring_commands(cmd::CommandRegistry& registry);

/// One default binding of the wiring tool's macro table: an X11-style key
/// name (`Button1`, `Shift_Button1`, `Control_Button1`, ...) and the command
/// text it runs (tcltk/tools.tcl magic::enable_tools). DISPATCH installs
/// them in the `wiring` table (ui-tools-and-input "Other tools' buttons").
struct ToolBinding {
  std::string_view key;
  std::string_view text;
};

/// The wiring tool's button bindings.
[[nodiscard]] std::span<const ToolBinding> tool_bindings() noexcept;

/// "wire command dispatch": the form a `wire` invocation that read the
/// cursor (`wire type`, `wire leg|horizontal|vertical` without `to`) is
/// logged as, `wire <option> at|to <x>i <y>i` with the cursor in edit-cell
/// coordinates, so a replay does not depend on the pointer; nullopt when the
/// invocation does not read the cursor or there is no cursor.
[[nodiscard]] std::optional<cmd::Invocation> logged_form(const cmd::Invocation& invocation,
                                                         const editor::EditorState& editor);

}  // namespace hefesto::wiring
