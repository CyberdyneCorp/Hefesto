// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The desktop's panel layout as data (board M2-Panels; capability
// hefesto-app-ux "Panel layout and focus cycling"; the M2 items carried to
// M4, openspec/changes/m4-scripting/design.md D17): which dock each panel is
// in (left, right, bottom, or floating in a window of its own inside the
// main window), the dock sizes, the collapsed panels and the docks shown as
// collapsed edge strips; dragging a panel header to another dock with a
// drop-zone highlight; the panel headers; and the layout saved per user and
// restored at launch.
//
// Every change is a `panel` command, so a drag is logged and replayable:
//   panel NAME ?show|hide|toggle?        collapse or restore (as M2)
//   panel NAME dock left|right|bottom    dock (also: return a floating panel)
//   panel NAME float ?X Y W H?           float, in window coordinates
//   panel reset                          the default arrangement
// run_panel_command() implements them on a PanelLayout; the shell's
// `panel` command calls it and arranges its widgets from the result.
//
// Panels are named by their `panel` word (layers, inspector, console, keys,
// drc, and the M4 managers), so a new panel needs no change here beyond
// default_panel_layout().
//
// Toolkit-independent; unit-tested in tests/unit/shell/panels_layout_test.cpp.
// Owner: CARRY (openspec/changes/m4-scripting/design.md D2).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::shell {

enum class DockZone : std::uint8_t { left, right, bottom, floating };

/// The `panel … dock` word of a zone ("left", "right", "bottom", "floating").
[[nodiscard]] std::string_view dock_zone_name(DockZone zone) noexcept;
[[nodiscard]] std::optional<DockZone> parse_dock_zone(std::string_view word) noexcept;

/// A rectangle in window coordinates (logical pixels, y down).
struct LayoutRect {
  float x = 0.0F;
  float y = 0.0F;
  float w = 0.0F;
  float h = 0.0F;

  [[nodiscard]] bool contains(float px, float py) const noexcept {
    return px >= x && px < x + w && py >= y && py < y + h;
  }
  friend bool operator==(const LayoutRect&, const LayoutRect&) = default;
};

struct PanelPlacement {
  std::string id;                 // the `panel` word: layers, inspector, console, ...
  DockZone dock = DockZone::left;
  DockZone home = DockZone::left;  // the dock a floating panel returns to ("Dock")
  bool collapsed = false;          // `panel NAME hide`
  LayoutRect floating;             // the floating window, kept while docked

  friend bool operator==(const PanelPlacement&, const PanelPlacement&) = default;
};

/// Sizes in logical pixels.
inline constexpr float kHeaderHeight = 32.0F;     // a collapsed panel keeps its 32 px header row
inline constexpr float kEdgeStripWidth = 32.0F;   // a dock collapsed to its edge strip
inline constexpr float kMinCanvasWidth = 600.0F;  // canvas and tool rail; below it the right, then the left dock become strips
inline constexpr float kMinDockSize = 160.0F;
inline constexpr float kMinFloatingSize = 160.0F;
inline constexpr float kDockBand = 48.0F;         // a drop this close to the canvas edge docks there

struct PanelLayout {
  /// In dock order: the order of a dock's panels is their order here (a
  /// side dock stacks them, the bottom dock shows them as tabs).
  std::vector<PanelPlacement> panels;
  float left_width = 240.0F;
  float right_width = 260.0F;
  float bottom_height = 180.0F;
  /// Docks shown as a collapsed edge strip (left, right, bottom).
  std::array<bool, 3> strip{false, false, false};

  [[nodiscard]] const PanelPlacement* find(std::string_view id) const noexcept;
  [[nodiscard]] PanelPlacement* find(std::string_view id) noexcept;
  /// The panels of `zone`, in order.
  [[nodiscard]] std::vector<std::string> in(DockZone zone) const;
  friend bool operator==(const PanelLayout&, const PanelLayout&) = default;
};

/// The default arrangement (`panel reset`, the M1 one): the Layers, Cells
/// and Libraries tabs and the DRC browser left (Layers shown; the browser,
/// collapsed, takes the Layers' place when shown, board M3-DRC-Browser),
/// Inspector and the technology manager right (techmanager collapsed),
/// Console and Keys as bottom tabs (collapsed), default sizes, no strip.
[[nodiscard]] PanelLayout default_panel_layout();

/// The panel whose strip shows `id` as a tab: "layers" for cells and
/// libraries (the left dock's tabs), "console" for keys; else `id` itself.
/// `panel ID dock|float` on a tab moves its whole strip (the tabs follow the
/// leader, right after it), and showing a tab shows the strip.
[[nodiscard]] std::string_view panel_group_leader(std::string_view id) noexcept;

/// The result of a `panel` command on a layout.
struct PanelCommandResult {
  bool ok = false;
  std::string message;  // the command's result ("shown", "default layout", ...) or the usage error
  bool changed = false;
};

/// The usage line of `panel`.
[[nodiscard]] std::string_view panel_command_usage() noexcept;

/// Runs `panel ARGS` on `layout`; `window` is the main window's size (a
/// floating panel stays inside it). Unknown panels and bad words fail with
/// the usage; nothing changes then.
[[nodiscard]] PanelCommandResult run_panel_command(PanelLayout& layout, const std::vector<std::string>& args,
                                                   const LayoutRect& window);

/// Keeps `r` inside `window` (at least kMinFloatingSize, at most the window).
[[nodiscard]] LayoutRect clamp_floating(const LayoutRect& r, const LayoutRect& window) noexcept;

// --- narrow windows and edge strips --------------------------------------------------

/// The layout the window shows at `width`: when the canvas would be
/// narrower than kMinCanvasWidth, the right dock collapses to its edge strip
/// first, then the left one (board M2-Panels, "At 1024 × 640 the right dock
/// collapses to its edge strip first"). The saved layout is not changed.
[[nodiscard]] PanelLayout fit_to_window(PanelLayout layout, float width);

/// The width the canvas keeps between the side docks at `width`.
[[nodiscard]] float canvas_width(const PanelLayout& layout, float width);

/// A dock shown as its edge strip: one entry per panel (clicking it runs
/// `panel NAME show`, which also opens the strip).
struct EdgeStrip {
  DockZone zone = DockZone::left;
  std::vector<std::string> panels;
};
[[nodiscard]] std::vector<EdgeStrip> edge_strips(const PanelLayout& layout);

// --- panel headers ------------------------------------------------------------------------

struct PanelHeaderButton {
  std::string icon;             // assets/icons name
  std::string accessible_name;  // "Collapse Layers", "Float Layers", ...
  std::string command;          // the `panel` command it issues
};

/// A panel's header (board M2-Panels): its title and count, the accessible
/// name with its state ("Layers, collapsed"), and the collapse/expand and
/// float/dock buttons.
struct PanelHeader {
  std::string title;
  std::string count;  // "24" (empty: no count)
  std::string accessible_name;
  std::vector<PanelHeaderButton> buttons;
  bool floating = false;
};
[[nodiscard]] PanelHeader panel_header(const PanelLayout& layout, std::string_view id, std::string_view title,
                                       std::optional<std::size_t> count = std::nullopt);

// --- drag to dock -----------------------------------------------------------------------

/// Where the docks are now, in window coordinates.
struct DockGeometry {
  LayoutRect window;
  LayoutRect left;    // the left dock (empty when it has no panel)
  LayoutRect right;
  LayoutRect bottom;
  LayoutRect canvas;
};

/// Where a dragged panel would go, and the rectangle to highlight: the
/// target dock (or, for an empty dock, the band at the canvas edge) as a
/// dashed ember drop zone, or the floating window's outline.
struct DropTarget {
  DockZone zone = DockZone::floating;
  LayoutRect highlight;
  friend bool operator==(const DropTarget&, const DropTarget&) = default;
};

/// A header drag in progress (press on a panel header, move, release; Esc
/// cancels and nothing changes). The drop is a `panel` command.
class PanelDrag {
 public:
  /// `panel` is where the dragged panel is now; the grab offset keeps the
  /// floating window under the pointer where it was taken.
  PanelDrag(std::string id, float press_x, float press_y, const LayoutRect& panel);

  /// The pointer moved: the target under it.
  const DropTarget& move(float x, float y, const DockGeometry& geometry);
  [[nodiscard]] const std::optional<DropTarget>& target() const noexcept { return target_; }
  [[nodiscard]] const std::string& id() const noexcept { return id_; }

  /// The command the release issues (`panel ID dock ZONE` or `panel ID
  /// float X Y W H`); std::nullopt before any move or after cancel().
  [[nodiscard]] std::optional<std::string> drop_command() const;
  void cancel() noexcept { target_.reset(); }

 private:
  std::string id_;
  float grab_x_;
  float grab_y_;
  LayoutRect panel_;
  std::optional<DropTarget> target_;
};

/// What the drop band says (board M2-Panels tile 2): "Drop to dock on the
/// right", and under it "Inspector was undocked from here" over the dock the
/// dragged panel left (its home, while it is not docked there). Empty for a
/// floating target: only the window's outline is drawn.
struct DropBandText {
  std::string caption;
  std::string note;
  friend bool operator==(const DropBandText&, const DropBandText&) = default;
};
/// Where the band is drawn: the target dock, or for an empty dock (whose
/// drop zone is the kDockBand strip at the canvas edge) the dock it would
/// open, at the layout's dock size; a floating target's window outline.
[[nodiscard]] LayoutRect drop_band_rect(const DropTarget& target, const PanelLayout& layout);
[[nodiscard]] DropBandText drop_band_text(const DropTarget& target, const PanelPlacement& panel,
                                          std::string_view title);

// --- saved per user -------------------------------------------------------------------------

/// The layout file in the user's storage folder (platform::default_storage_dir()).
inline constexpr std::string_view kPanelLayoutFile = "panel-layout.json";

/// The JSON text of a layout (version 1).
[[nodiscard]] std::string panel_layout_json(const PanelLayout& layout);

/// Reads a layout from its JSON text over `defaults`: panels of `defaults`
/// missing from the text are added where `defaults` has them (a panel added
/// by a later version), panels the text names that `defaults` lacks are
/// dropped. std::nullopt for text that is not a version-1 layout.
[[nodiscard]] std::optional<PanelLayout> parse_panel_layout(std::string_view text, const PanelLayout& defaults);

/// What restoring found.
struct RestoredLayout {
  PanelLayout layout;
  bool from_file = false;      // read from the file (else the defaults)
  std::string warning;         // a file that could not be read or parsed (the defaults are used)
};

/// The layout to show at launch: the saved one, else the defaults.
[[nodiscard]] RestoredLayout restore_panel_layout(platform::FileSystem& fs, const platform::Path& file,
                                                  const PanelLayout& defaults = default_panel_layout());

/// Saves the layout (atomically; the folder is created when missing).
[[nodiscard]] diag::Status save_panel_layout(platform::FileSystem& fs, const platform::Path& file,
                                             const PanelLayout& layout);

}  // namespace hefesto::shell
