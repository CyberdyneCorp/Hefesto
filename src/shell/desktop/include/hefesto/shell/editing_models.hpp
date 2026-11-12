// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Toolkit-free view models of the M2 editing screens (DESIGN boards
// docs/design/mockups/M2-*.dc.html; capabilities hefesto-app-ux "Main window
// layout", hefesto-input-and-touch "Command entry on every platform",
// hefesto-design-system "Icons", "Status is never color-only"): the live
// tool rail, command-bar completion, multi-line results, the prompt dialogs
// of cmd::PromptHost, the selection inspector, undo/redo affordances and
// the feedback list. Like view_models.hpp, the Raven layer only draws what
// these functions return.
//
// Owner: SHELL (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/shell/input_router.hpp"
#include "hefesto/shell/view_models.hpp"

namespace hefesto::shell {

// --- tool rail --------------------------------------------------------------------

/// The rail button shown active (its icon id) for the core's current tool
/// and the rail mode: box -> "box" (or the mode's "paint" / "label" /
/// "measure"), pick -> "select", wiring -> "wire"; none for nettool and
/// tools made by `macro copy`.
[[nodiscard]] std::optional<std::string_view> active_rail_icon(std::string_view tool, EditMode mode) noexcept;

/// What a rail button does: the core command it runs (empty: none) and the
/// rail mode it sets. Box, Select and Wire switch Magic's tools (`tool box`,
/// `tool pick`, `tool wiring`); Paint, Label and Measure are modes of the
/// box tool (`tool box` first when another tool is current); Zoom to fit
/// runs `view`. std::nullopt for the disabled Place device.
struct RailAction {
  std::string command;
  EditMode mode = EditMode::none;
};
[[nodiscard]] std::optional<RailAction> rail_action(std::string_view icon, std::string_view current_tool);

/// A rail button's tooltip in its current state: the tool, its key and, for
/// a disabled Paint, why ("Paint: choose a layer to paint in the Layers panel").
[[nodiscard]] std::string rail_tooltip(const ToolButton& button, bool enabled);

// --- command bar --------------------------------------------------------------------

/// What Tab does to the command line (ui::complete): the last word is
/// completed as a command name when it is the first word, as a cell name
/// after the cell commands (`load`, `getcell`, `get`, `xload`, `edit`,
/// `dump`, `flush`, `save`, `cellname`, `instance`, `writeall`, `down`) and
/// as a layer name after the layer commands (`paint`, `erase`, `see`,
/// `fill`, `select`, `contact`, `genkey`, `splitpaint`, `spliterase`, `tech
/// lock`, `tech unlock`, `wire type`). One candidate completes the word and adds a space; several
/// extend it to their longest common prefix and are listed in
/// `candidates`; none leaves the line as it is.
struct Completion {
  std::string line;
  std::vector<std::string> candidates;
};
[[nodiscard]] Completion complete_command_line(std::string_view line, const cmd::CommandRegistry& registry,
                                               const editor::EditorState& editor);

/// The status-bar text of a command result: its first line, and when it has
/// more, " (+N lines, see Console)" so the rest is never lost (the status
/// bar opens the Console when clicked). `more_lines` is N.
struct ResultLine {
  std::string first;
  std::size_t more_lines = 0;
  [[nodiscard]] std::string text() const;
};
[[nodiscard]] ResultLine result_line(std::string_view output);

// --- prompt dialogs (cmd::PromptHost) -------------------------------------------------

struct DialogButton {
  std::string label;
  std::size_t answer = 0;  // index into the PromptHost answers
  bool primary = false;    // the default answer: focused, Return and Escape give it
};

/// One row of a dialog's detail block (board M4-Script-Consent tile 2): the
/// key, a monospace value, then an optional status chip and a muted note.
struct DialogDetail {
  enum class Tone : std::uint8_t { neutral, ok, warning };
  std::string key;
  std::string value;    // monospace; may be empty (a chip alone)
  std::string chip;     // "● digest verified"; empty: none
  Tone tone = Tone::neutral;
  std::string note;     // "+ 8 part files (device generators)"
  std::string tooltip;  // the whole text when the value is shortened
};

struct PromptDialog {
  std::string title;
  std::string text;
  std::vector<DialogButton> buttons;  // in display order, the default last
  /// M4 alert style (board M4-Script-Consent note 1); all optional:
  std::string icon;                    // assets/icons/<icon>.svg in a 32 px box left of the title
  std::vector<DialogDetail> details;   // the inset block under the text
  float width = 0.0F;                  // the content width; 0: measured (at least 420)
  std::size_t leading_buttons = 0;     // buttons kept at the left, before the gap
};

/// The dialog for a PromptHost question: the title is the question's first
/// sentence, the text the whole question (trimmed); one button per answer,
/// labelled with a verb when the question says what it does ("Discard" for
/// `yes` to throwing away changes, "Overwrite" for `yes` to an existing file,
/// "Recover" for a crash backup; "Cancel" for the `no` of those questions),
/// else the answer capitalized.
[[nodiscard]] PromptDialog prompt_dialog(std::string_view question, const std::vector<std::string>& answers,
                                         std::size_t default_answer);

/// The answer a closed dialog gives: `result` is the clicked button's
/// answer + 1, or 0 when the dialog was dismissed (Escape): the default.
[[nodiscard]] std::size_t dialog_answer(int result, std::size_t answer_count, std::size_t default_answer) noexcept;

// --- selection inspector ---------------------------------------------------------------

/// A button of the inspector: its label and the command it issues.
struct InspectorAction {
  std::string label;
  std::string command;
  /// true: the button opens a menu of choices instead (`orient`: the eight
  /// orientations, the current one checked).
  bool menu = false;
};

/// What the inspector shows for the selection (DESIGN board M2-Selection):
/// "SELECTION" with a kind title (Paint, Label, Port, Instance, Mixed), the
/// selection's bounding box, and per kind the layers with their tile counts,
/// the label's text, layer, position and port index, the instance's cell,
/// id, orientation and array; actions issue `what`, `identify`, `orient`,
/// `setlabel`, `port`, `array`, `select net`, `expand selection`, `delete`.
/// std::nullopt when the selection is empty or in another window's root.
[[nodiscard]] std::optional<InspectorView> selection_inspector_view(const editor::EditorState& editor,
                                                                    const db::CellDef* window_root,
                                                                    const UnitScale& scale,
                                                                    std::vector<InspectorAction>* actions = nullptr);

// --- undo and redo -------------------------------------------------------------------------

struct UndoAffordance {
  bool can_undo = false;
  bool can_redo = false;
  std::string undo_tooltip;  // "Undo (u): 3 steps"
  std::string redo_tooltip;  // "Redo (U): nothing to redo"
  bool recording = true;     // false: `undo disable` ("undo off" chip)
};
[[nodiscard]] UndoAffordance undo_affordance(const editor::EditorState& editor);

// --- feedback list ----------------------------------------------------------------------------

/// One row of the Feedback tab: "1  (x0, y0) – (x1, y1)  text", in display
/// units; `find` is `feedback find N`, which moves the box there.
struct FeedbackRow {
  std::string text;
  std::string find;
};
[[nodiscard]] std::vector<FeedbackRow> feedback_rows(const editor::EditorState& editor, const UnitScale& scale);

}  // namespace hefesto::shell
