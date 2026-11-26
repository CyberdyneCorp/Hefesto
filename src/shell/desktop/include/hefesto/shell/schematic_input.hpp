// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Pointer and key input of the schematic canvas (capability
// schematic-editing "Desktop and touch interaction"; openspec/changes/
// m8-schematic design D15, task 13.2). The same gestures as the layout
// canvas (CanvasController): the wheel and a pinch zoom about the pointer,
// a trackpad scroll pans, an Option-drag or a middle-button drag pans; they
// change only the view (SchematicViewport: no command, no undo step). A
// left click selects the instance under the pointer with `schematic select
// instance <name>`. With the key focus on the canvas, a key runs its
// binding of the `schematic` macro table (SchematicKeyMap: xschem-style
// defaults, rebindable): a command line, or an editor action, which this
// controller turns into a line (descend the selected instance), a command
// bar prompt (wire, move, copy: their coordinates are typed) or a request
// to the view (properties, place). Ctrl-C asks for the interrupt.
// Copy and paste (schematic_clipboard.hpp): Command-C and Command-V on
// macOS, where Ctrl-C stays the interrupt; Ctrl-C and Ctrl-V elsewhere,
// where Ctrl-C is the interrupt while a netlist runs (set_running) and Copy
// otherwise. A paste goes where the pointer last was over the canvas (the
// view's centre before the pointer came in).
//
// Raven-free and unit-tested in tests/unit/shell/schematic_input_test.cpp.
// Owner: SHELL (openspec/changes/m8-schematic design D3, D15; task 13.2).

#pragma once

#include <optional>
#include <string>

#include "hefesto/shell/canvas_controller.hpp"
#include "hefesto/shell/schematic_bindings.hpp"
#include "hefesto/shell/schematic_canvas.hpp"

namespace hefesto::shell {

/// What one input event asks of the view.
struct SchematicInputResult {
  bool consumed = false;
  bool repaint = false;  // the view changed (pan, zoom)
  std::string line;      // a command line to run (logged, replayable)
  std::string prompt;    // a command to put in the command bar ("schematic wire ")
  std::string action;    // "properties" or "place": shown by the view
  bool interrupt = false;  // Ctrl-C
  bool copy = false;       // Copy: the view runs the clipboard link's copy
  std::optional<schem::Point> paste_at;  // Paste: where (schematic units)
};

class SchematicCanvasInput {
 public:
  /// `macos`: Command and Option are Meta for the bindings.
  explicit SchematicCanvasInput(bool macos) noexcept : macos_(macos) {}

  [[nodiscard]] SchematicKeyMap& keys() noexcept { return keys_; }
  [[nodiscard]] const SchematicKeyMap& keys() const noexcept { return keys_; }

  /// Applies `input` (canvas pixels, origin top left) to `viewport` and
  /// `scene` (picking).
  SchematicInputResult handle(const CanvasInput& input, SchematicViewport& viewport, const SchematicScene& scene);

  /// True once the user panned or zoomed: the view no longer fits itself.
  [[nodiscard]] bool user_view() const noexcept { return user_view_; }
  /// Another document: the view fits again, nothing is selected.
  void reset_view() noexcept;
  /// A netlist run is going on: Ctrl-C interrupts it (not Copy) off macOS.
  void set_running(bool running) noexcept { running_ = running; }
  /// The instance last clicked (empty: none), the target of `e` and `q`.
  [[nodiscard]] const std::string& selected() const noexcept { return selected_; }

 private:
  SchematicInputResult pointer_down(const CanvasInput& input, const SchematicViewport& viewport,
                                    const SchematicScene& scene);
  SchematicInputResult wheel(const CanvasInput& input, SchematicViewport& viewport);
  SchematicInputResult key(const CanvasInput& input, const SchematicViewport& viewport);
  [[nodiscard]] SchematicInputResult clipboard_key(const CanvasInput& input, const SchematicViewport& viewport) const;
  SchematicInputResult bound(const BoundAction& action) const;

  SchematicKeyMap keys_;
  bool macos_ = false;
  bool dragging_ = false;
  bool user_view_ = false;
  bool running_ = false;
  std::optional<CanvasPoint> pointer_;  // where the pointer last was over the canvas
  std::string selected_;
};

}  // namespace hefesto::shell
