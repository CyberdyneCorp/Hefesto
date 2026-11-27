// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The top bar's "Extract" button (openspec/changes/m7-extraction/design.md
// D19, task 16.1): painted and disabled since M1, enabled in M7. A click
// runs `extract all` on the root of the active window as a typed command
// (Console echo, history, session log); the run reports through
// MaskSession::write_host, so the M5 host shows the running header, the
// Interrupt card after 2 s and cancels it. Enabled only when the session has
// an extract style (EditorState::extract.current), a root to extract and no
// job running; disabled otherwise with a tooltip that names what is missing.
//
// Toolkit-free and unit-tested (tests/unit/shell/m7_extract_button_test.cpp).
//
// Owner: SHELL.

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::shell {

/// The command the button runs, as typed.
inline constexpr std::string_view kExtractCommand = "extract all";

/// What the button's state depends on.
struct ExtractButtonInput {
  std::string technology;                 // the session technology's name
  std::optional<std::string> style;       // the current extract style; nullopt: none
  std::optional<std::string> root;        // the active window's root cell; nullopt: no window
  bool job_running = false;               // a long read, write or extraction is running
};

struct ExtractButtonView {
  bool enabled = false;
  std::string tooltip;
  friend bool operator==(const ExtractButtonView&, const ExtractButtonView&) = default;
};

/// The button's state: enabled with "Extract <root>: extract all (style
/// <style>)", or disabled with the reason (no extract style in the
/// technology, no cell in the active window, a job running).
[[nodiscard]] ExtractButtonView extract_button_view(const ExtractButtonInput& input);

/// The input from a session: its technology, extract style and the active
/// window's root.
[[nodiscard]] ExtractButtonInput extract_button_input(const editor::EditorState& editor, bool job_running);

/// The line the window runs for a click, or nullopt when the button is
/// disabled (a click on a disabled button does nothing).
[[nodiscard]] std::optional<std::string> extract_click_line(const ExtractButtonView& view);

/// The status line the app prints after an --input script (CI capture):
/// "hefesto: extract button enabled|disabled: <tooltip>".
[[nodiscard]] std::string extract_button_status(const ExtractButtonView& view);

}  // namespace hefesto::shell
