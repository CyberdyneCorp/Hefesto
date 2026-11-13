// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The rail's box-tool modes as M2 deferred them to M4 (capability
// hefesto-input-and-touch "Desktop Paint, Label and Measure tools";
// board M2-Tool-States; openspec/changes/m4-scripting/design.md D17):
//   - the Label tool's text entry as a popover anchored to the swept box
//     on the canvas, issuing `label TEXT ?POSITION? ?LAYER?` on Enter;
//   - Escape right after a Measure sweep issuing `unmeasure` (the ruler it
//     drew is removed; tools.tcl's unmeasure deletes the rulers in the box);
//   - the Paint button disabled, with the tooltip "Choose a layer to paint",
//     while the layer palette has no paint layer.
//
// The shell shows it as a Raven application popup (Rpopup_open, Raven
// v0.2.3; docs/decisions/raven-go-no-go.md, draft 4) at label_popover_point():
// Escape, Cancel or a click outside closes it without a command.
//
// Toolkit-independent; unit-tested in tests/unit/shell/label_tool_test.cpp.
// Owner: CARRY (openspec/changes/m4-scripting/design.md D2).

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "hefesto/db/geometry.hpp"
#include "hefesto/shell/drc_models.hpp"  // command_word
#include "hefesto/shell/input_router.hpp"

namespace hefesto::shell {

// --- label popover ----------------------------------------------------------------

/// The label positions the popover offers (Magic's `label` directions;
/// empty: Magic picks the position from the box).
inline constexpr std::array<std::string_view, 10> kLabelPositions{"",  "n",  "ne", "e",  "se",
                                                                   "s", "sw", "w",  "nw", "center"};

/// Size of the popover card in logical pixels.
inline constexpr double kLabelPopoverWidth = 280.0;
inline constexpr double kLabelPopoverHeight = 120.0;

/// Where the popover goes for the box's on-screen rectangle (left `box_x`,
/// top `box_y`, width `box_w`; logical pixels, y down) in a canvas of
/// `canvas_w` × `canvas_h`: right of the box with an 8 px gap, flipped to
/// its left when it would leave the canvas, its top at the box's top, kept
/// inside the canvas. Returns {x, y}.
struct PopoverPoint {
  double x = 0.0;
  double y = 0.0;
  friend bool operator==(const PopoverPoint&, const PopoverPoint&) = default;
};
[[nodiscard]] PopoverPoint label_popover_point(double box_x, double box_y, double box_w, double canvas_w,
                                               double canvas_h);

/// The popover's state: open after a Label sweep (the box is already set to
/// the swept area by `box values`), with the text field focused.
class LabelPopover {
 public:
  /// Opens for the box `box` (root coordinates); `layer` prefills the layer
  /// field (the palette's paint layer; empty: Magic's rule).
  void open(const db::Rect& box, std::string layer = {});
  [[nodiscard]] bool is_open() const noexcept { return box_.has_value(); }
  [[nodiscard]] const std::optional<db::Rect>& box() const noexcept { return box_; }

  void set_text(std::string text) { text_ = std::move(text); }
  [[nodiscard]] const std::string& text() const noexcept { return text_; }
  /// One of kLabelPositions; anything else is ignored.
  void set_position(std::string_view position);
  [[nodiscard]] const std::string& position() const noexcept { return position_; }
  void set_layer(std::string layer) { layer_ = std::move(layer); }
  [[nodiscard]] const std::string& layer() const noexcept { return layer_; }

  /// The layer field applies only with a position (`label TEXT POS LAYER`;
  /// Magic's `label` takes the layer as its third word), so the popover
  /// enables it once a position is chosen.
  [[nodiscard]] bool layer_enabled() const noexcept { return !position_.empty(); }

  /// Enter: `label TEXT`, `label TEXT POS` or `label TEXT POS LAYER` (the
  /// layer only with a position), and the popover closes. std::nullopt (the
  /// popover stays open) while the text is empty.
  [[nodiscard]] std::optional<std::string> commit();
  /// Escape: closes without issuing anything (the box stays).
  void cancel() noexcept;

 private:
  std::optional<db::Rect> box_;
  std::string text_;
  std::string position_;
  std::string layer_;
};

// --- Escape after a measure ---------------------------------------------------------------

/// Tracks whether the last thing that ran was a Measure sweep, by the
/// session's command counter (editor::EditorState::command_count): any
/// command after it (typed, a key, the box moved) ends the window in which
/// Escape removes the measurement.
class MeasureEscape {
 public:
  /// The Measure sweep's `box values … ; measure` ran; `command_count` is
  /// the counter after it.
  void measured(std::uint64_t command_count) noexcept { after_ = command_count; }
  /// Escape with no sweep in progress: `unmeasure` when nothing ran since
  /// the measure, else std::nullopt. Either way the window closes.
  [[nodiscard]] std::optional<std::string> escape(std::uint64_t command_count) noexcept;
  [[nodiscard]] bool pending(std::uint64_t command_count) const noexcept {
    return after_.has_value() && *after_ == command_count;
  }

 private:
  std::optional<std::uint64_t> after_;
};

// --- Paint button -------------------------------------------------------------------------

/// The Paint rail button's state for the palette's paint layer: enabled with
/// its usual tooltip when a layer is set; otherwise disabled with the
/// tooltip "Choose a layer to paint" and "unavailable" in its accessible name.
struct PaintToolState {
  bool enabled = false;
  std::string tooltip;
  std::string accessible_name;
};
[[nodiscard]] PaintToolState paint_tool_state(std::string_view paint_layer, std::string_view label,
                                              std::string_view tooltip);

/// The rail mode once the paint layer is `paint_layer`: the Paint mode
/// cannot stay without a layer (it returns to the plain box tool).
[[nodiscard]] EditMode mode_for_paint_layer(EditMode mode, std::string_view paint_layer) noexcept;

}  // namespace hefesto::shell
