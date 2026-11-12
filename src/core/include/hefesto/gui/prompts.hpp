// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The file prompts of the wrapper (openspec/changes/m4-scripting/design.md
// D4/D12; capability ui-gui-managers "File prompts"): `promptload
// magic|gds|cif|getcell`, `promptsave magic|force|gds|cif`,
// `saveallandquit` and `prompt_dialog new|save`. The file dialogs are
// platform::FilePicker requests (the desktop's native dialog, a scripted
// picker in tests); a null picker or a cancelled request does nothing, as
// an empty answer of Tk's file dialog.
//
// (Magic ref: tcltk/wrapper.tcl:66-231,1033-1048)
//
// Owner: GUIMGR (design D2).

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/platform/file_picker.hpp"

namespace hefesto::cmd {
class CommandContext;
}

namespace hefesto::gui {

/// `magic::promptload <type>`; an unknown type does nothing.
[[nodiscard]] cmd::CommandResult prompt_load(cmd::CommandContext& ctx, platform::FilePicker* picker,
                                             std::string_view type);

/// `magic::promptsave <type>`; an unknown type does nothing.
[[nodiscard]] cmd::CommandResult prompt_save(cmd::CommandContext& ctx, platform::FilePicker* picker,
                                             std::string_view type);

/// `magic::saveallandquit`: `promptsave force`, then `quit -noprompt` when
/// no cell is modified, else `quit`.
[[nodiscard]] cmd::CommandResult save_all_and_quit(cmd::CommandContext& ctx, platform::FilePicker* picker);

/// `magic::prompt_dialog new|save`: the initial cell name of the dialog
/// (`(UNNAMED)`, or the window's cell) ...
[[nodiscard]] std::string prompt_dialog_initial(cmd::CommandContext& ctx, std::string_view type);
/// ... and the command its Okay button runs (`load <name>` / `save <name>`).
[[nodiscard]] std::vector<std::string> prompt_dialog_command(std::string_view type, std::string_view name);

}  // namespace hefesto::gui
