// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Layout windows and views (capability ui-windows-and-views): identity and
// limits, view mapping, loading a cell into a window, captions, per-window
// expansion, layer visibility, grid and crosshair state. Toolkit-free: the
// desktop shell renders a LayoutWindow into its canvas and observes changes
// through ViewObserver; core view commands mutate it.
// (Magic ref: windows/windMove.c, windows/windView.c, windows/windTrans.c,
// dbwind/DBWprocs.c, commands/CmdSubrs.c)
//
// Owner: VIEWER (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"

namespace hefesto::cmd {
class CommandContext;
}
#include "hefesto/diag/outcome.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::view {

using WindowId = std::int32_t;

inline constexpr std::size_t kMaxWindows = 31;  // "Window identity and limits"

/// A screen rectangle in pixels (inclusive corners, as in Magic).
struct ScreenRect {
  std::int32_t x0 = 0;
  std::int32_t y0 = 0;
  std::int32_t x1 = 0;
  std::int32_t y1 = 0;
  friend bool operator==(const ScreenRect&, const ScreenRect&) = default;
};

/// Magic's null-display screen (GrScreenRect of graphics/grNull.c); the
/// frame of a window created without an explicit frame in a headless session.
inline constexpr ScreenRect kNullScreen{0, 0, 511, 483};

/// "View mapping": integer scale in 1/65536 pixel per unit plus origin.
struct ViewMapping {
  ScreenRect screen;
  db::Rect surface;            // layout area mapped onto `screen`
  std::int64_t scale = 65536;  // 1/65536 pixel per layout unit
  std::int64_t origin_x = 0;   // 1/65536 pixel
  std::int64_t origin_y = 0;
  bool at_minimum = false;     // fit() clamped the scale to 1 ("At minimum scale!")

  /// Makes `area` visible in `screen` (WindMove, the formula of "View mapping").
  [[nodiscard]] static ViewMapping fit(const ScreenRect& screen, const db::Rect& area);
  [[nodiscard]] std::int32_t screen_x(db::Coord x) const noexcept;
  [[nodiscard]] std::int32_t screen_y(db::Coord y) const noexcept;
  /// The nearest layout point of a pixel, rounding half away from zero
  /// (WindPointToSurface).
  [[nodiscard]] db::Point to_layout(std::int32_t px, std::int32_t py) const noexcept;
  /// The layout box covering a screen rectangle (WindScreenToSurface).
  [[nodiscard]] db::Rect screen_to_surface(const ScreenRect& r) const noexcept;
  /// WindScroll: shifts the view by a layout offset and/or a screen offset
  /// (pixels; the layout appears to move by +offset), then re-derives the
  /// surface area so it still overlaps the screen (windFixSurfaceArea).
  [[nodiscard]] ViewMapping scrolled(db::Point surface_offset, db::Point screen_offset) const noexcept;
};

/// Chrome flags of a window ("Window chrome flags").
struct ChromeFlags {
  bool border = true;
  bool caption = true;
  bool scrollbars = true;
  friend bool operator==(const ChromeFlags&, const ChromeFlags&) = default;
};

/// Magic's chrome metrics (windows/windInt.h): border line width, scroll-bar
/// width (7 on the null display, 14 on X11/OpenGL/Cairo) and caption height
/// (default font height + 3; the null display's font is 1 pixel high).
struct ChromeMetrics {
  std::int32_t border = 2;
  std::int32_t scrollbar = 7;
  std::int32_t caption = 4;
};

/// WindOutToIn: the screen area inside `frame` for the given chrome.
[[nodiscard]] ScreenRect inner_area(const ScreenRect& frame, const ChromeFlags& flags,
                                    const ChromeMetrics& metrics = {});

// --- framing rules (pure, shared by commands and the shell) ----------------

/// "Loading a cell into a window": the margin max(0, (60 - w)/2) + (w + 1)/10
/// per axis around the cell bounding box.
[[nodiscard]] db::Rect load_frame(const db::Rect& bbox);
/// "view, xview and zoom": WindView, each side grown by (size + 1)/10 of the
/// already-grown size, in the order xbot, xtop, ybot, ytop.
[[nodiscard]] db::Rect view_frame(const db::Rect& bbox);
/// WindZoom: the surface rescaled about its centre by `factor` (single
/// precision, truncated toward zero).
[[nodiscard]] db::Rect zoom_area(const db::Rect& surface, float factor);
/// "Opening and closing windows" / "center and findbox": the box plus a
/// margin of max(2, w/20) in x and max(2, h/20) in y.
[[nodiscard]] db::Rect box_frame(const db::Rect& box);

/// "Window captions": `R EDITING E` when `editing`, else `R [NOT BEING EDITED]`,
/// names shortened to "..." + their last (limit - 3) characters (limits 90
/// and 175) and the whole caption truncated to 199 characters.
[[nodiscard]] std::string window_caption(std::string_view root, std::string_view edit, bool editing);

/// Grid state ("Grid display and grid command"): `spacing` is the unit box
/// (ox, oy)-(ox+sx, oy+sy).
struct GridState {
  bool visible = false;
  db::Rect spacing{0, 0, 1, 1};
  std::optional<std::int32_t> multiple;  // `grid multiple m` (1..255), Q-UI-21
};

/// What changed, for observers ("Change-driven redisplay"). `content`
/// means the window has a pending redisplay (LayoutWindow::take_redisplay);
/// `highlights` that its highlight frame changed (HighlightLayer::frame).
enum class Change : std::uint8_t { opened, closed, view, content, expansion, visibility, grid, caption, highlights };

/// The redisplay a window still owes ("Change-driven redisplay"): the whole
/// window, or a list of areas. `areas` are in root coordinates and are not
/// clipped to the screen (a host canvas keeps the content outside its screen
/// area); `screen` holds the same areas on screen (WindSurfaceToScreen),
/// clipped to the screen area and grown by the label extent where the rules
/// say so, without the areas that fall off screen.
struct Redisplay {
  bool all = false;
  std::vector<db::Rect> areas;
  std::vector<ScreenRect> screen;
  [[nodiscard]] bool empty() const noexcept { return !all && areas.empty(); }
};

class ViewObserver {
 public:
  ViewObserver() = default;
  ViewObserver(const ViewObserver&) = delete;
  ViewObserver& operator=(const ViewObserver&) = delete;
  virtual ~ViewObserver() = default;
  virtual void on_window_changed(WindowId id, Change change) = 0;
};

class LayoutWindow {
 public:
  LayoutWindow(WindowId id, db::CellDef* root);
  ~LayoutWindow();
  LayoutWindow(const LayoutWindow&) = delete;
  LayoutWindow& operator=(const LayoutWindow&) = delete;

  [[nodiscard]] WindowId id() const noexcept { return id_; }
  /// The window's private root use of the displayed definition (nullptr when empty).
  [[nodiscard]] const db::CellUse* root_use() const noexcept { return root_use_.get(); }
  [[nodiscard]] db::CellDef* root() const noexcept { return root_use_ ? root_use_->def : nullptr; }
  /// "Loading a cell into a window" steps 1 and 2: new root, expansion
  /// cleared, view framed with load_frame().
  void set_root(db::CellDef* def);

  /// "Window captions": "<root> EDITING <edit cell>" style text.
  [[nodiscard]] const std::string& caption() const noexcept { return caption_; }
  void set_caption(std::string caption) { caption_ = std::move(caption); }
  /// Host window name (`openwindow cell name`); empty when none.
  [[nodiscard]] const std::string& name() const noexcept { return name_; }
  void set_name(std::string name) { name_ = std::move(name); }

  [[nodiscard]] const ViewMapping& mapping() const noexcept { return mapping_; }
  void set_mapping(const ViewMapping& mapping) noexcept { mapping_ = mapping; }
  /// WindMove: maps `area` onto the current screen area.
  void move_to(const db::Rect& area) { mapping_ = ViewMapping::fit(mapping_.screen, area); }

  /// The frame (screen area plus chrome) and chrome flags of the window.
  [[nodiscard]] const ScreenRect& frame() const noexcept { return frame_; }
  void set_frame(const ScreenRect& frame) noexcept { frame_ = frame; }

  /// `grow` state (WindFullScreen): whether the window fills the screen, and
  /// the frame and depth (0 = front) it returns to.
  [[nodiscard]] bool full_screen() const noexcept { return full_screen_; }
  [[nodiscard]] const ScreenRect& saved_frame() const noexcept { return saved_frame_; }
  [[nodiscard]] std::size_t saved_depth() const noexcept { return saved_depth_; }

  // "Change-driven redisplay". mark_redisplay() takes an area in root
  // coordinates; `labels` grows its screen area by the label extent.
  void mark_redisplay(const db::Rect& root_area, bool labels);
  void mark_redisplay_all();
  [[nodiscard]] const Redisplay& pending_redisplay() const noexcept { return redisplay_; }
  /// Returns the pending redisplay and clears it.
  [[nodiscard]] Redisplay take_redisplay();
  /// The largest label extent seen in this window, as screen offsets added
  /// to an area's corners (Magic's dbw_expandAmounts: x0/y0 <= 0, x1/y1 >= 0).
  [[nodiscard]] const ScreenRect& label_extent() const noexcept { return label_extent_; }
  /// Widens the label extent to include `amounts` (it never shrinks).
  void include_label_extent(const ScreenRect& amounts) noexcept;
  [[nodiscard]] const ChromeFlags& chrome() const noexcept { return chrome_; }
  void set_chrome(const ChromeFlags& chrome) noexcept { chrome_ = chrome; }

  // "Per-window expansion state" (Magic keeps it on each use). The root use
  // is always expanded.
  [[nodiscard]] bool is_expanded(const db::CellUse& use) const;
  void set_expanded(const db::CellUse& use, bool expanded);
  void clear_expansion() { expanded_.clear(); }

  // "Layer visibility (see)".
  [[nodiscard]] const tech::TypeMask& visible_types() const noexcept { return visible_; }
  void set_visible_types(const tech::TypeMask& mask) noexcept { visible_ = mask; }
  [[nodiscard]] bool labels_visible() const noexcept { return labels_visible_; }
  void set_labels_visible(bool on) noexcept { labels_visible_ = on; }
  [[nodiscard]] bool cells_visible() const noexcept { return cells_visible_; }
  void set_cells_visible(bool on) noexcept { cells_visible_ = on; }
  [[nodiscard]] bool all_same() const noexcept { return all_same_; }
  void set_all_same(bool on) noexcept { all_same_ = on; }

  [[nodiscard]] GridState& grid() noexcept { return grid_; }
  [[nodiscard]] const GridState& grid() const noexcept { return grid_; }

  /// "Crosshair": the crosshair shown in this window, if any. Set through
  /// WindowManager::set_crosshair, which applies it to every window on the
  /// same root.
  [[nodiscard]] const std::optional<db::Point>& crosshair() const noexcept { return crosshair_; }
  void set_crosshair(std::optional<db::Point> p) noexcept { crosshair_ = p; }

 private:
  WindowId id_;
  std::unique_ptr<db::CellUse> root_use_;
  std::string caption_;
  std::string name_;
  ViewMapping mapping_;
  ScreenRect frame_;
  ChromeFlags chrome_;
  std::unordered_set<const db::CellUse*> expanded_;
  tech::TypeMask visible_ = tech::TypeMask::all();
  bool labels_visible_ = true;
  bool cells_visible_ = true;
  bool all_same_ = false;
  GridState grid_;
  std::optional<db::Point> crosshair_;
  bool full_screen_ = false;
  ScreenRect saved_frame_;
  std::size_t saved_depth_ = 0;
  Redisplay redisplay_;
  ScreenRect label_extent_;

  friend class WindowManager;  // full_screen() state
};

/// The open layout windows in front-to-back order.
class WindowManager {
 public:
  WindowManager();
  ~WindowManager();
  WindowManager(const WindowManager&) = delete;
  WindowManager& operator=(const WindowManager&) = delete;

  /// "Opening and closing windows": smallest free id, new window in front,
  /// `screen` used as the drawing area (host-managed chrome, Q-UI-40) and the
  /// root framed per "Loading a cell into a window". Fails with
  /// VIEW-WINDOW-001 ("Can't have more than 31 windows.") at the limit.
  [[nodiscard]] diag::Outcome<LayoutWindow*> open(db::CellDef* root, const ScreenRect& screen);
  /// Same, with Magic-managed chrome: the screen area is `frame` minus the
  /// chrome of the session defaults (`chrome_defaults()`).
  [[nodiscard]] diag::Outcome<LayoutWindow*> open_frame(db::CellDef* root, const ScreenRect& frame = kNullScreen);
  /// Closes a window; VIEW-WINDOW-002 when no window has that id.
  [[nodiscard]] diag::Status close(WindowId id);
  [[nodiscard]] LayoutWindow* find(WindowId id) const;
  /// A window id for a window of another client (the color window): the
  /// smallest id no window holds; it counts toward the 31-window limit
  /// (VIEW-WINDOW-001) until release_id().
  [[nodiscard]] diag::Outcome<WindowId> reserve_id();
  void release_id(WindowId id);
  [[nodiscard]] bool id_reserved(WindowId id) const;
  /// The window named `name` (LayoutWindow::name), or nullptr.
  [[nodiscard]] LayoutWindow* find_named(std::string_view name) const;
  /// The window commands apply to (the front window, or the one under the
  /// pointer as set by the shell); nullptr when none.
  [[nodiscard]] LayoutWindow* current() const;
  void set_current(WindowId id);
  void for_each(const std::function<void(LayoutWindow&)>& visit) const;  // front to back
  [[nodiscard]] std::size_t size() const noexcept;
  /// `over` / `underneath`: move a window to the front or the back.
  void raise(WindowId id);
  void lower(WindowId id);
  /// `grow` (WindFullScreen): toggles the window between the whole screen
  /// (`screen()`, depth 0) and the frame and depth it had before, keeping
  /// the layout area it shows (WindMove of its surface area).
  void toggle_full_screen(WindowId id);
  /// The screen windows live on (GrScreenRect): the null display's screen
  /// unless a host sets its own.
  [[nodiscard]] const ScreenRect& screen() const noexcept { return screen_; }
  void set_screen(const ScreenRect& screen) noexcept { screen_ = screen; }

  /// "Change-driven redisplay" (DBWAreaChanged): `area` of `def` changed.
  /// Every window showing `def` as its root, or through instances expanded
  /// in that window, gets the area mapped up to its root (array rule:
  /// the span of all elements when the area is more than half the cell's
  /// width or height, else each element). `layers` are the changed types:
  /// a window where none of them is visible skips the change; nullopt means
  /// labels may have changed too, which grows the screen area by the
  /// window's label extent. A zero-width or zero-height area changes nothing.
  /// `only` restricts the change to one window (an expansion change).
  void area_changed(const db::CellDef& def, const db::Rect& area, const std::optional<tech::TypeMask>& layers,
                    std::optional<WindowId> only = std::nullopt);
  /// `redraw`: every window's whole area.
  void redisplay_all();
  /// The update after a command: notifies Change::content for every window
  /// with a pending redisplay, unless the display is suspended.
  void update_display();

  /// Session chrome defaults copied by new windows ("Window chrome flags").
  [[nodiscard]] ChromeFlags& chrome_defaults() noexcept { return chrome_defaults_; }
  [[nodiscard]] ChromeMetrics& chrome_metrics() noexcept { return chrome_metrics_; }

  /// `crosshair X Y` / `crosshair off`: shown in every window whose root is `root`.
  void set_crosshair(const db::CellDef* root, std::optional<db::Point> point);

  /// WindScale (`scalegrid`): every window's surface area corners mapped
  /// through `scale_point` (TECH's DBScalePoint rule) and the window moved
  /// there (WindMove), so it shows the same layout after the database scale.
  void scale_all(const std::function<db::Point(db::Point)>& scale_point);

  /// `*winddebug` (windPrintCommands): the dispatcher prints every command
  /// it dispatches while set.
  [[nodiscard]] bool print_commands() const noexcept { return print_commands_; }
  void set_print_commands(bool on) noexcept { print_commands_ = on; }

  /// `updatedisplay suspend|resume`.
  [[nodiscard]] bool display_suspended() const noexcept { return suspended_; }
  void set_display_suspended(bool on) noexcept { suspended_ = on; }

  /// Reads an unavailable cell from its file when an instance of it is
  /// expanded (Magic's DBExpand reads it; `load` itself reads only the named
  /// cell). Installed by the CELL area, which owns cell files; a plain
  /// function so nothing it refers to can dangle. Returns false when the
  /// cell could not be read.
  using CellReader = bool (*)(cmd::CommandContext& ctx, db::CellDef& def);
  void set_cell_reader(CellReader reader) noexcept { cell_reader_ = reader; }
  [[nodiscard]] CellReader cell_reader() const noexcept { return cell_reader_; }

  /// Observers are not owned and must outlive the manager or be removed.
  void add_observer(ViewObserver& observer);
  void remove_observer(const ViewObserver& observer);
  void notify(WindowId id, Change change);

 private:
  [[nodiscard]] diag::Outcome<LayoutWindow*> insert(db::CellDef* root, const ScreenRect& frame,
                                                    const ScreenRect& screen);

  std::vector<std::unique_ptr<LayoutWindow>> windows_;  // front to back
  std::vector<WindowId> reserved_;                      // ids of other clients' windows
  [[nodiscard]] WindowId free_id() const;
  std::optional<WindowId> current_;
  std::vector<ViewObserver*> observers_;
  void propagate_change(const db::CellDef& def, const db::Rect& area, std::uint32_t windows,
                        const std::optional<tech::TypeMask>& layers);

  ChromeFlags chrome_defaults_;
  ChromeMetrics chrome_metrics_;
  ScreenRect screen_ = kNullScreen;
  bool suspended_ = false;
  bool print_commands_ = false;
  CellReader cell_reader_ = nullptr;
};

}  // namespace hefesto::view
