// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The wiring tool's tracking feedback (board M2-Wiring; carried from M2 to
// M4, openspec/changes/m4-scripting/design.md D17): the HUD next to the
// cursor (type, width, direction and leg length in the current units), the
// "Wiring · tracking" mode chip, and the Wire inspector (type, width ±,
// the technology's wiring stack, Switch layer). Every control issues a
// `wire` / `trackwire` command, so it is logged and replayable; the preview
// leg itself is the wiring area's `wire show` selection (M2).
//
// Toolkit-independent; unit-tested in tests/unit/shell/wiring_hud_test.cpp.
// Owner: CARRY (openspec/changes/m4-scripting/design.md D2).

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/shell/editing_models.hpp"
#include "hefesto/shell/view_models.hpp"
#include "hefesto/wiring/wiring.hpp"

namespace hefesto::shell {

/// The direction word of a leg ("north", ...; empty for none).
[[nodiscard]] std::string_view leg_direction_name(wiring::LegDirection direction) noexcept;

// --- HUD ----------------------------------------------------------------------------

/// The HUD's text while tracking: "metal2 · 0.140 · east · 1.230" (type,
/// width, direction and length of the leg the next click paints, lengths in
/// `scale`'s units), or "metal2 · 0.140" when the pointer is within the box's
/// span (no leg). std::nullopt when the tool is not tracking or has no type.
[[nodiscard]] std::optional<std::string> wiring_hud_text(const editor::EditorState& editor, const db::Rect& box,
                                                         const db::Point& pointer, const UnitScale& scale);

/// Where the HUD goes: at +14, +20 px from the cursor, flipped to the left
/// (and above) of it when it would leave the canvas. Logical pixels, y down.
struct HudPoint {
  double x = 0.0;
  double y = 0.0;
  friend bool operator==(const HudPoint&, const HudPoint&) = default;
};
[[nodiscard]] HudPoint wiring_hud_point(double cursor_x, double cursor_y, double hud_w, double hud_h,
                                        double canvas_w, double canvas_h);

// --- mode chip ----------------------------------------------------------------------

/// The top bar's mode chip while the wiring tool tracks: its text and the
/// command its cancel (R or Esc, or a click) issues (`trackwire cancel`:
/// the preview is cleared, nothing is painted).
struct WiringChip {
  std::string text;            // "Wiring · tracking"
  std::string accessible_name;  // "Wiring tool tracking; Escape cancels"
  std::string cancel;          // "trackwire cancel"
};
[[nodiscard]] std::optional<WiringChip> wiring_chip(const editor::EditorState& editor);

// --- Wire inspector -----------------------------------------------------------------

/// One layer of the technology's wiring stack (the `wiring` section's
/// contacts, bottom to top): choosing it issues `wire type NAME`.
struct WireStackRow {
  std::string name;
  bool current = false;
  std::string command;
};

/// The Wire inspector (board M2-Wiring, tile 4) of the wiring tool.
struct WireInspectorView {
  std::string section = "WIRE";
  std::string type;           // the current type's name, "no material" before `wire type`
  std::string width;          // in `scale`'s units ("0.140")
  std::string units;          // "um" or "lambda"
  std::string width_command;  // the width field's prefix: "wire width " (the value is typed)
  std::vector<InspectorAction> actions;  // Width −, Width +, Switch layer
  std::vector<WireStackRow> stack;
};

/// The layers of `section` in stack order: each contact joins two layers;
/// the layers are listed as they first appear in the technology file.
[[nodiscard]] std::vector<std::string> wiring_stack(const wiring::WiringSection& section,
                                                    const tech::Technology& tech);

[[nodiscard]] WireInspectorView wire_inspector(const editor::EditorState& editor,
                                               const wiring::WiringSection& section, const UnitScale& scale);

}  // namespace hefesto::shell
