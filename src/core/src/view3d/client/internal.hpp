// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private helpers of the `wind3d` client (view3d/client): the diagnostic
// codes, Magic's number tests and the window lookup shared by client.cpp,
// commands.cpp, render.cpp and scene.cpp.
//
// Owner: WIND3D (openspec/changes/m10-3d-view/design.md D3).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/view3d/client/commands.hpp"
#include "hefesto/view3d/client/session.hpp"

namespace hefesto::mask {
class OutputStyle;
}

namespace hefesto::view3d::detail {

// VIEW3D-CLIENT-* (codes.inc).
inline constexpr std::string_view kUsage = "VIEW3D-CLIENT-001";
inline constexpr std::string_view kOneWindow = "VIEW3D-CLIENT-002";
inline constexpr std::string_view kCannotOpen = "VIEW3D-CLIENT-003";
inline constexpr std::string_view kZoomValue = "VIEW3D-CLIENT-004";
inline constexpr std::string_view kNoStyle = "VIEW3D-CLIENT-005";
inline constexpr std::string_view kUnknownMaskLayer = "VIEW3D-CLIENT-006";
inline constexpr std::string_view kUnknownCommand = "VIEW3D-CLIENT-007";
inline constexpr std::string_view kReport = "VIEW3D-CLIENT-008";
inline constexpr std::string_view kNoWindow = "VIEW3D-CLIENT-009";

/// Magic's "No CIF output style set!" (cif/CIFmain.c:334; Q-MISC-73).
inline constexpr std::string_view kNoStyleText = "No CIF output style set!";

[[nodiscard]] cmd::CommandResult fail(std::string_view code, std::string text);
/// A usage message: Magic's TxError text, no change.
[[nodiscard]] cmd::CommandResult usage(std::string text);
/// TxPrintf: a note on standard output.
void print(cmd::CommandContext& ctx, std::string text);

/// StrIsNumeric + atof: a number at the start of `word` (strtod accepts a
/// prefix), converted to `float` as Magic's `(float)atof(...)`.
[[nodiscard]] std::optional<float> numeric(std::string_view word);
/// StrIsInt + atoi: an optional sign then digits only (the empty word is
/// an integer, 0); the value saturates to the int range where atoi
/// overflows (undefined in Magic; fixed in both modes).
[[nodiscard]] std::optional<int> integer(std::string_view word);
/// strncmp(word, prefix, 3) == 0.
[[nodiscard]] bool starts3(std::string_view word, std::string_view prefix);

[[nodiscard]] bool magic_compat(cmd::CommandContext& ctx);

/// The session and its open window, brought up to date with the output
/// style (sync_output_style); VIEW3D-CLIENT-009 when no 3D window is open.
struct Target {
  editor::EditorState& editor;
  WindowState& window;
};
[[nodiscard]] diag::Outcome<Target> target(cmd::CommandContext& ctx);

/// The current output style, or nullptr.
[[nodiscard]] const mask::OutputStyle* current_style(const editor::EditorState& editor);
/// The key of the current output style (OutputStyleKey).
[[nodiscard]] OutputStyleKey current_key(const editor::EditorState& editor);
/// The rendered cell's bounding box (empty when the cell is gone).
[[nodiscard]] db::Rect cell_bbox(const editor::EditorState& editor, const WindowState& window);
/// Set3DDefaults: the default view of the window's mode (f: the current
/// style's with magic-compat ON, the applied one OFF; Q-MISC-78), every
/// layer visible in both sets, no cut box.
void set_defaults(const editor::EditorState& editor, WindowState& window, bool magic_compat);
/// w3drefreshFunc: the front end's display redraws the window.
void redraw(const editor::EditorState& editor, const WindowState& window);

// The command groups (commands.cpp, render.cpp).
cmd::CommandResult render_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv);
cmd::CommandResult see_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv);
cmd::CommandResult cutbox_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv);
cmd::CommandResult render3d_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv);

}  // namespace hefesto::view3d::detail
