// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `wind3d` client (openspec/changes/m10-3d-view/design.md D9). Scaffold
// stubs (M10 task 2.3): open_wind3d answers UI-CLIENT-002 with the text
// `specialopen wind3d` gives today, register_wind3d_commands registers
// nothing. exit_mask_factor forwards to the camera's Q-MISC-78 rule
// (view3d::exit_mask_factor). WIND3D replaces the stubs in section 8 (and
// retires the UI-CLIENT-002 `wind3d` path, task 8.1).
//
// Owner: WIND3D (design D3).

#include "hefesto/view3d/client/commands.hpp"

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/ui/diag_codes.hpp"

namespace hefesto::view3d {

float WindowState::exit_mask_factor(bool magic_compat, float current_factor) const {
  return view3d::exit_mask_factor(magic_compat, applied_factor, current_factor);
}

diag::Status open_wind3d(editor::EditorState& /*editor*/, const OpenRequest& /*request*/) {
  return diag::make(ui::codes::kModuleUnavailable, "the wind3d window type is not available yet");
}

bool register_wind3d_commands(cmd::CommandRegistry& /*registry*/) {
  return true;
}

}  // namespace hefesto::view3d
