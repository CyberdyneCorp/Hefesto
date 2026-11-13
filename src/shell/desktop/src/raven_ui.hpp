// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The C++20 wrapper over Raven's C API (m0-foundations design.md D5):
// Runtime, Window, Box, Panel, ListPanel, CommandBar, Viewport, and for the
// forge screens (m1-core-viewer D6) Painted (custom-painted widgets through a
// Raven-free Painter), TextInput, Overlay, ScrollBox and Tabs. Every Raven call
// of Hefesto goes through raven_ui.cpp, which is the only file that includes
// <Raven.h>; this header is Raven-free (widgets are plain 32-bit handles), so
// the rest of the shell never sees a Raven type.
//
// Threading: Raven's UI thread is the thread that created the Runtime. Every
// method runs on it, except the viewport draw callback, which Raven calls on
// its render thread.

#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/render/renderer.hpp"
#include "hefesto/shell/canvas_controller.hpp"
#include "hefesto/shell/input_router.hpp"
#include "hefesto/shell/design_tokens.hpp"
#include "hefesto/shell/editing_models.hpp"
#include "hefesto/shell/input_script.hpp"
#include "hefesto/shell/theme_tokens.hpp"

namespace hefesto::shell::ui {

using WidgetId = std::uint32_t;
inline constexpr WidgetId kNoWidget = 0xFFFF'FFFFU;

struct RuntimeOptions {
  std::string app_name = "hefesto";
  bool vsync = true;
  bool validation = false;
  /// Taken around Raven's use of the Vulkan queue (shared with the canvas renderer).
  std::function<void()> queue_lock;
  std::function<void()> queue_unlock;
};

/// Raven's lifetime: Rinit on creation, Rshutdown on destruction. One per process.
class Runtime {
 public:
  /// Returns null and sets `error` when Raven cannot start (no display, no Vulkan driver).
  static std::unique_ptr<Runtime> start(RuntimeOptions options, std::string& error);
  ~Runtime();
  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;

  /// Draws one frame when needed; false once the application quits.
  bool wait_frame(double timeout_seconds);
  void quit();
  /// Called on the UI thread once per frame, before layout.
  void set_frame_callback(std::function<void(double)> callback);
  /// Raven's Vulkan device, shared with the canvas renderer (raven_vulkan.h).
  [[nodiscard]] render::VulkanDevice vulkan_device() const;
  [[nodiscard]] std::string version() const;

  /// Loads the bundled Inter / JetBrains Mono faces and the icons from
  /// `asset_dir` (fonts/, icons/). Returns false when a font file is missing
  /// or unreadable; Raven's Roboto then stands in for the missing faces.
  bool load_assets(const std::filesystem::path& asset_dir);
  /// Applies the forge tokens as the Raven theme: every color role, the
  /// metrics (radius, control and row heights, font size) and Inter as the
  /// theme font. Honors the system's reduce-motion setting.
  void apply_theme(ThemeMode mode);
  /// The loaded face nearest to `face` (Raven's bundled font if none).
  [[nodiscard]] WidgetId font(FontFace face) const;
  [[nodiscard]] WidgetId font(TextRole role) const { return font(text_spec(role).face); }
  /// The monospace font for commands and coordinates (JetBrains Mono when loaded).
  [[nodiscard]] WidgetId mono_font() const { return font(FontFace{FontFamily::mono, 400}); }
  /// The icon texture `assets/icons/<name>.svg`, rasterized for `scale`; kNoWidget if absent.
  [[nodiscard]] WidgetId icon(std::string_view name) const;
  [[nodiscard]] bool fonts_loaded() const noexcept { return fonts_loaded_; }
  /// Width of `text` in `role`, in logical units (for sizing painted widgets).
  [[nodiscard]] float text_width(TextRole role, std::string_view text) const;

 private:
  Runtime() = default;
  struct Callbacks;
  std::unique_ptr<Callbacks> callbacks_;
  std::map<int, WidgetId> faces_;  // key: family * 1000 + weight
  std::map<std::string, WidgetId, std::less<>> icons_;
  bool fonts_loaded_ = false;
};

/// A widget's bounds in its window, in logical units (after the last layout).
struct Rect {
  float x = 0.0F;
  float y = 0.0F;
  float width = 0.0F;
  float height = 0.0F;
};

/// Base of the widget wrappers: a Raven handle plus common properties.
class Widget {
 public:
  [[nodiscard]] WidgetId id() const noexcept { return id_; }
  [[nodiscard]] Rect rect() const;
  /// True while this widget has the window's keyboard focus.
  [[nodiscard]] bool has_focus() const;
  void set_accessible_name(std::string_view name);
  void set_tooltip(std::string_view text);
  void set_font(WidgetId font);
  void focus();
  /// Fixed logical size; a negative value leaves that axis free.
  void set_size(float width, float height);
  void set_min_size(float width, float height);
  /// false: collapsed (takes no space).
  void set_visible(bool visible);
  void set_enabled(bool enabled);
  void set_hit_testable(bool hit_testable);
  void set_font_size(float size);
  /// In an Overlay: anchors the widget to the top-right corner, `right` and
  /// `top` logical units from the edges (call after adding it).
  void place_top_right(float right, float top);
  /// Back to the default slot (fill both ways, no padding): a widget moved
  /// out of an Overlay keeps the placement place_top_right gave it otherwise.
  void place_fill();
  /// In an Overlay: centred across, `top` logical units from the top edge.
  void place_top_center(float top);

 protected:
  explicit Widget(WidgetId id) noexcept : id_(id) {}
  WidgetId id_;
};

/// Vertical or horizontal box panel.
class Box : public Widget {
 public:
  static Box vertical();
  static Box horizontal();
  /// Adds a child; fill > 0 makes it take that share of the free space.
  void add(const Widget& child, float fill = 0.0F);
  /// Puts `child` at `index` (moving it when it is already a child).
  void insert(const Widget& child, int index, float fill = 0.0F);
  /// Adds a child centered across the box's direction.
  void add_centered(const Widget& child);
  void clear();
  void set_padding(float left, float top, float right, float bottom);
  void set_spacing(float spacing);

 private:
  using Widget::Widget;
};

/// A background around one child.
class Panel : public Widget {
 public:
  static Panel create(const Rgba& background);
  void set_child(const Widget& child);
  void set_fill(const Rgba& background);
  void set_padding(float left, float top, float right, float bottom);

 private:
  using Widget::Widget;
};

/// Children drawn on top of each other (the canvas and its overlay).
class Overlay : public Widget {
 public:
  static Overlay create();
  void add(const Widget& child);

 private:
  using Widget::Widget;
};

/// A vertical scrollable stack.
class ScrollBox : public Widget {
 public:
  static ScrollBox create();
  void add(const Widget& child);
  void clear();

 private:
  using Widget::Widget;
};

/// Tabs over pages (Console / Diagnostics).
class Tabs : public Widget {
 public:
  static Tabs create();
  void add(const Widget& page, std::string_view title);
  void show(int index);

 private:
  using Widget::Widget;
};

/// A single-line input (layer filter). `on_change` runs on every edit.
class TextInput : public Widget {
 public:
  static std::unique_ptr<TextInput> create(std::string_view placeholder);
  ~TextInput();
  TextInput(const TextInput&) = delete;
  TextInput& operator=(const TextInput&) = delete;
  void on_change(std::function<void(std::string)> callback);
  /// Return pressed (M4 PCell dialog: an entry's handlers run on Return).
  void on_commit(std::function<void(std::string)> callback);
  /// Replaces the text (no change callback).
  void set_text(std::string_view text);
  [[nodiscard]] std::string text() const;
  /// This input's outline (Raven's R_COLOR_BORDER and R_COLOR_FOCUS set on
  /// the widget), e.g. the warning outline of a corrected PCell field;
  /// nullopt goes back to the theme's.
  void set_outline(const std::optional<Rgba>& color);

 private:
  explicit TextInput(WidgetId id);
  struct Callbacks;
  std::unique_ptr<Callbacks> callbacks_;
};

enum class Role : std::uint8_t { none, group, text, heading, button, toggle_button, checkbox, list, list_item, tree,
                                 tree_item, tab_list, tab, toolbar, status_bar, canvas, image, pane };

/// Has a screen reader read `text`; `assertive` interrupts what it is reading.
void announce(std::string_view text, bool assertive);

/// Runs `task` on the UI thread during the next frame and wakes the event
/// loop (Rtask_post). Thread-safe: the background DRC worker's ready
/// callback posts through it.
void post_task(std::function<void()> task);

/// Raven's Table in virtual mode (hefesto-app-ux "Desktop platform
/// integration through Raven", Tables): it stores no rows and asks for the
/// text of the cells it draws, so 200,000 rows cost no widget each. Sortable
/// columns, keyboard (arrows, Return) and screen readers come with Raven.
class Table : public Widget {
 public:
  static std::unique_ptr<Table> create();
  ~Table();
  Table(const Table&) = delete;
  Table& operator=(const Table&) = delete;

  /// Adds a column; `fill` > 0 takes that share of the free width.
  void add_column(std::string_view header, float width, float fill, bool sortable);
  /// Sets the row count and the cell text source (called for visible cells only).
  void set_rows(int count, std::function<std::string(int row, int column)> text);
  void set_selected(int row);
  [[nodiscard]] int selected() const;
  void scroll_to_row(int row);
  /// Shows the sorted column (-1: none) without reordering anything.
  void show_sort(int column, bool descending);
  /// Hides the column headers (M4 console transcript: a log, not a table).
  void set_header_visible(bool visible);
  /// A row picked with the mouse or the arrow keys.
  void on_select(std::function<void(int)> callback);
  /// A row activated (double click, Return).
  void on_activate(std::function<void(int)> callback);
  /// A header click: the application sorts its data.
  void on_sort(std::function<void(int column, bool descending)> callback);
  /// A key pressed while the table has the focus, before the table's own
  /// keys (Raven's widget event callback): `callback(key, modifiers)` (Raven
  /// key codes, kMod* bits) returns true to consume it.
  void on_key(std::function<bool(int key, std::uint8_t modifiers)> callback);

 private:
  explicit Table(WidgetId id);
  struct Callbacks;
  std::unique_ptr<Callbacks> callbacks_;
};

// --- M4 SHELL-B (tasks 10.4, 10.5): console input, file dialogs, frames ------------

/// Raven's multi-line text editor as the Tcl console's input
/// (M4-Console). Return and Shift-Return are decided by the window's key
/// hook (TclConsole::press_return), not by the editor.
class TextEditor : public Widget {
 public:
  static std::unique_ptr<TextEditor> create(std::string_view placeholder);
  ~TextEditor();
  TextEditor(const TextEditor&) = delete;
  TextEditor& operator=(const TextEditor&) = delete;
  /// Replaces the text and puts the cursor at its end (no change callback).
  void set_text(std::string_view text);
  [[nodiscard]] std::string text() const;
  /// The cursor's line (0-based) and the line count, for ↑/↓ history.
  [[nodiscard]] int cursor_line() const;
  [[nodiscard]] int line_count() const;
  /// The cursor as a line and a byte column (both from 0).
  [[nodiscard]] std::pair<int, int> cursor() const;
  void set_cursor(int line, int column);
  void set_font(WidgetId font, float size);
  void on_change(std::function<void(std::string)> callback);

 private:
  explicit TextEditor(WidgetId id);
  struct Callbacks;
  std::unique_ptr<Callbacks> callbacks_;
};

/// One filter of a native file dialog: a name and ';'-separated patterns.
struct FileDialogFilter {
  std::string name;
  std::string patterns;
};

/// A native open or save dialog (platform::FilePicker's desktop host).
struct FileDialogOptions {
  bool save = false;
  std::string title;
  std::string directory;
  std::string file_name;
  std::vector<FileDialogFilter> filters;
  std::string default_extension;
};

/// The system's open or save dialog for `parent` (kNoWidget: none);
/// nullopt when cancelled.
[[nodiscard]] std::optional<std::string> file_dialog(WidgetId parent, const FileDialogOptions& options);

/// Draws one frame from inside a long command (the console's pump): OS
/// events, posted tasks, layout and paint. Only from a callback that runs
/// outside Raven's frame update (an input event), as Rdialog_run does.
void pump_frame();

/// A short message over the content (Raven Toast), hidden after `seconds`.
class Toast : public Widget {
 public:
  static Toast create();
  void show(std::string_view text, StatusKind kind, double seconds);
  /// Places it at the bottom center of its overlay parent, 16 px from the edge.
  void place_bottom_center();

 private:
  using Widget::Widget;
};

/// Draws with Raven's painter in logical units; the only drawing surface the
/// rest of the shell sees. Colors are forge tokens or PDK colors.
class Painter {
 public:
  Painter(void* raven_painter, const Runtime& runtime) noexcept : p_(raven_painter), runtime_(runtime) {}
  void fill(float x, float y, float w, float h, float radius, const Rgba& c);
  void stroke(float x, float y, float w, float h, float radius, float thickness, const Rgba& c);
  void line(float x0, float y0, float x1, float y1, float thickness, const Rgba& c);
  /// A dashed rectangle outline (box tool, dashed wells): `dash` on, `gap` off.
  void dashed_rect(float x, float y, float w, float h, float thickness, float dash, float gap, const Rgba& c);
  /// Text with its top-left corner at (x, y), in the family, size and weight
  /// of `role`; uppercase roles are tracked 0.08 em.
  void text(TextRole role, float x, float y, std::string_view text, const Rgba& c);
  /// Text vertically centered on `cy`.
  void text_centered_y(TextRole role, float x, float cy, std::string_view text, const Rgba& c);
  [[nodiscard]] float text_width(TextRole role, std::string_view text) const;
  [[nodiscard]] float line_height(TextRole role) const;
  /// Text in the family and weight of `role` at `size_px` (canvas label
  /// text, whose size follows the zoom); not tracked.
  void text_px(TextRole role, float size_px, float x, float y, std::string_view text, const Rgba& c);
  /// Width and height of text_px's text.
  [[nodiscard]] std::pair<float, float> measure_px(TextRole role, float size_px, std::string_view text) const;
  /// An icon of `assets/icons/`, tinted with `c`, in a `size` square.
  void icon(std::string_view name, float x, float y, float size, const Rgba& c);
  void push_clip(float x, float y, float w, float h);
  void pop_clip();

 private:
  void* p_;
  const Runtime& runtime_;
};

/// A widget painted by the shell: chips, tool and action buttons, layer
/// rows, hierarchy rows, rulers, minimap, status bar. Clickable and
/// focusable when given a click handler (Enter / Space click it); draw the
/// focus ring with `focused()`.
class Painted : public Widget {
 public:
  using PaintFn = std::function<void(Painter&, float width, float height)>;
  /// A paint function that also gets the widget (hover and focus state).
  using StatefulPaintFn = std::function<void(Painter&, float width, float height, const Painted& self)>;
  static std::unique_ptr<Painted> create(const Runtime& runtime, PaintFn paint);
  static std::unique_ptr<Painted> create_stateful(const Runtime& runtime, StatefulPaintFn paint);
  ~Painted();
  Painted(const Painted&) = delete;
  Painted& operator=(const Painted&) = delete;

  void on_click(std::function<void(float x, float y)> callback);
  /// A right click (or the Menu key / Shift-F10 while focused): `callback`
  /// gets the window point where a context menu opens.
  void on_context(std::function<void(float x, float y)> callback);
  /// Press-and-hold of the left button (PressHold, kHoldDelay): `callback`
  /// gets the window point under the widget's bottom-left corner. With it,
  /// a mouse click runs on release (before the delay), not on press.
  void on_hold(std::function<void(float x, float y)> callback);
  /// What a drag from the widget carries (Raven's in-application drag:
  /// `label` follows the pointer; DropZone callbacks get `type` and `data`).
  struct DragData {
    std::string type;
    std::string data;
    std::string label;
  };
  /// A left press moved kDragThreshold or more (DragGesture): `callback`
  /// gets the press point (window units) and returns what to drag, or
  /// nullopt to stay a press. With it, a click runs on release (when no
  /// drag started), not on press.
  void on_drag(std::function<std::optional<DragData>(float x, float y)> callback);
  void set_role(Role role);
  void set_accessible_description(std::string_view text);
  [[nodiscard]] bool hovered() const;
  [[nodiscard]] bool focused() const;
  void invalidate();

 private:
  explicit Painted(WidgetId id);
  struct Callbacks;
  std::unique_ptr<Callbacks> callbacks_;
};

/// Where a drag started by Painted::on_drag can be dropped: `widget` and its
/// children (a deeper DropZone takes precedence). Raven highlights the zone
/// while the callback accepts; the callback stays installed while the
/// DropZone lives.
class DropZone {
 public:
  enum class Phase : unsigned char { enter, move, leave, drop };
  /// Gets the phase, the pointer in window units, the drag's type and data;
  /// returns whether a drop there is accepted.
  using Callback = std::function<bool(Phase, float x, float y, std::string_view type, std::string_view data)>;
  DropZone(const Widget& widget, Callback callback);
  ~DropZone();
  DropZone(const DropZone&) = delete;
  DropZone& operator=(const DropZone&) = delete;

 private:
  struct State;
  std::unique_ptr<State> state_;
};

/// A list of text rows (Console, Diagnostics).
class ListPanel : public Widget {
 public:
  static ListPanel create();
  void add_item(std::string_view text);
  void clear();
  /// Scrolls so that the last item is visible (log-style panels).
  void scroll_to_end();
  /// `callback(index)` when the user picks a row (click, arrow keys, Enter).
  void on_select(std::function<void(int)> callback);
  /// Unselects every row (so the same row can be picked again).
  void clear_selection();

 private:
  using Widget::Widget;
  struct Select;
  std::shared_ptr<Select> select_;  // shared by copies of the handle
};

class Window;

/// One item of a PopupMenu.
struct MenuItem {
  std::string label;
  bool checked = false;
  bool enabled = true;
  bool separator_before = false;
};

/// A Raven menu opened as a popover at a point of the window (command
/// history and completion, undo history, layer row and orientation menus).
/// Raven 0.2.2 has no anchored popover widget: its menus are the in-window
/// popups that take the keyboard (arrows, Return, Escape) and that screen
/// readers can press.
class PopupMenu {
 public:
  PopupMenu();
  ~PopupMenu();
  PopupMenu(const PopupMenu&) = delete;
  PopupMenu& operator=(const PopupMenu&) = delete;

  /// Replaces the items; `on_choose(index)` runs when item `index` is chosen.
  void set_items(std::string_view title, const std::vector<MenuItem>& items,
                 std::function<void(std::size_t)> on_choose);
  /// Opens the menu with its top-left corner at (x, y) of `window`
  /// (logical units; Raven keeps it inside the window).
  void open(const Window& window, float x, float y);
  [[nodiscard]] std::size_t size() const noexcept;

 private:
  struct State;
  std::unique_ptr<State> state_;
};

/// The single-line command input. Enter commits; Up/Down ask for history.
class CommandBar : public Widget {
 public:
  static std::unique_ptr<CommandBar> create(std::string_view placeholder);
  ~CommandBar();
  CommandBar(const CommandBar&) = delete;
  CommandBar& operator=(const CommandBar&) = delete;

  void on_commit(std::function<void(std::string)> callback);
  /// `callback(true)` for Up (older), `callback(false)` for Down; returns the text to show.
  void on_history(std::function<std::string(bool)> callback);
  void set_text(std::string_view text);
  /// Tab: `callback(text)` returns the completed text (nullopt: unchanged).
  void on_complete(std::function<std::optional<std::string>(std::string)> callback);

 private:
  explicit CommandBar(WidgetId id);
  struct Callbacks;
  std::unique_ptr<Callbacks> callbacks_;
};

/// What the viewport draw callback receives (render thread).
struct ViewportDraw {
  void* command_buffer = nullptr;  // VkCommandBuffer inside Raven's render pass
  void* render_pass = nullptr;     // VkRenderPass
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  std::uint64_t frame_index = 0;
};

/// The canvas: a Raven viewport in draw-callback mode (design.md D9).
class Viewport : public Widget {
 public:
  static std::unique_ptr<Viewport> create();
  ~Viewport();
  Viewport(const Viewport&) = delete;
  Viewport& operator=(const Viewport&) = delete;

  /// Called on Raven's render thread inside its render pass, with the viewport
  /// and scissor already set to the widget.
  void on_draw(std::function<void(const ViewportDraw&)> callback);
  /// Called on the UI thread; return true to consume the event.
  void on_input(std::function<bool(const CanvasInput&)> callback);
  /// Called on the UI thread when the pixel size changes.
  void on_resize(std::function<void(int, int)> callback);
  /// true: redraw every frame (benchmark); false: redraw on invalidate().
  void set_realtime(bool realtime);
  void invalidate();
  /// The pointer shape over the canvas (cursor glyphs, input_router.hpp).
  void set_cursor(CursorShape shape);

 private:
  explicit Viewport(WidgetId id);
  struct Callbacks;
  std::unique_ptr<Callbacks> callbacks_;
};

struct FrameStats {
  double layout_ms = 0.0;
  double paint_ms = 0.0;
  double render_ms = 0.0;
  std::uint32_t widget_count = 0;
  std::uint32_t draw_call_count = 0;
  std::uint64_t frame_index = 0;
};

/// What a PromptDialog's M4 alert parts are drawn with (the theme's
/// tokens and the loaded icon and fonts); left empty, they are not drawn.
struct DialogLook {
  WidgetId icon = kNoWidget;   // texture of PromptDialog::icon
  WidgetId title_font = kNoWidget;
  WidgetId mono = kNoWidget;   // detail values
  Rgba inset;                  // detail block and icon box fill (surface1)
  Rgba border;                 // their outline
  Rgba icon_color;
  Rgba muted;                  // keys, notes
  Rgba body;                   // the text under the title
  Rgba ok;                     // chip fill (14 %) and text
  Rgba ok_text;
  Rgba warning;
  Rgba warning_text;
};

class Window {
 public:
  Window(std::string_view title, int width, int height);
  [[nodiscard]] WidgetId id() const noexcept { return id_; }
  /// Sees every key before the widgets (Raven's window event hook): drops
  /// AppKit's function-key text, retypes a held key's text on macOS
  /// (TextKeyFilter), and offers each key press to `shortcut(key,
  /// modifiers)` (Raven key codes, kMod* bits), which consumes it by
  /// returning true (F6 focus cycling).
  void install_key_hook(std::function<bool(int, std::uint8_t)> shortcut);
  /// After install_key_hook: `release(x, y)` (window units) when the left
  /// button comes up during an in-application drag, before Raven ends it.
  /// The panel header drag drops through it, so that its drop zones can
  /// refuse Raven's drop (and its whole-widget highlight) and draw the
  /// board's band instead.
  void on_drag_release(std::function<void(float, float)> release);
  /// Closes every open menu and popup.
  void close_popups();
  ~Window();
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  void set_content(const Widget& content);
  /// Deletes the window now; returns once Raven no longer renders it, so no
  /// viewport draw callback runs afterwards. Idempotent.
  void destroy();
  [[nodiscard]] bool valid() const;
  void request_redraw();
  /// Logical-to-pixel scale of the window's monitor (Retina: 2).
  [[nodiscard]] float scale() const;
  /// The window's width in logical pixels (0 when unknown).
  [[nodiscard]] int logical_width() const;
  [[nodiscard]] FrameStats frame_stats() const;
  /// Saves the last rendered frame as a binary PPM; false on failure.
  bool capture_ppm(const std::string& path) const;
  /// Tests and scripted captures: a trackpad pinch at window point (x, y)
  /// (logical pixels) as the system would send it, `factor` spread over
  /// `steps` updates between a begin and an end.
  void inject_trackpad_pinch(float x, float y, double factor, int steps);
  /// A left-button drag from (x0, y0) to (x1, y1) in `steps` moves, as the
  /// system sends one (scripted checks of header drags).
  void inject_left_drag(float x0, float y0, float x1, float y1, int steps);
  /// One step of an --input script (input_script.hpp) as the system sends
  /// it: a pointer move, a button press or release, a key press and
  /// release, typed text.
  void inject_input(const InputStep& step);
  /// True while an in-application drag (Painted::on_drag) is in progress.
  [[nodiscard]] bool drag_active() const;
  /// The left button pressed at (x, y) and kept down (scripted captures of
  /// an open menu of the in-window bar).
  void inject_left_press(float x, float y);
  /// The window system Raven chose for this window: "x11", "wayland",
  /// "cocoa", "win32", or "none" (headless or unknown).
  [[nodiscard]] std::string window_system() const;
  /// True while the window is the one the user works in (Rwindow_is_active).
  [[nodiscard]] bool active() const;
  /// The Dock (macOS) or launcher (Linux) progress: indeterminate while
  /// `busy`, cleared otherwise. False where the system shows none.
  bool set_busy_progress(bool busy) const;
  /// Bounces the Dock icon / marks the window urgent when it is not active.
  void request_attention() const;
  void set_title(std::string_view title);
  void set_min_size(int width, int height);
  void set_clear_color(const Rgba& color);
  /// The system's open dialog for one file matching `patterns` ("*.mag");
  /// nullopt when cancelled.
  [[nodiscard]] std::optional<std::string> open_file_dialog(std::string_view title, std::string_view filter_name,
                                                            std::string_view patterns) const;
  /// M4 utility windows (PCell dialog): kept above the other windows.
  void set_topmost(bool topmost);
  /// The user asked to close the window (title bar): `callback` runs and the
  /// window stays open (the owner destroys it).
  void on_close_request(std::function<void()> callback);
  /// Shows a prompt dialog (title, wrapped text, one button per answer, the
  /// primary one focused). `wait`: runs it modally and returns the clicked
  /// button's answer + 1, or 0 when dismissed (Escape); otherwise opens it
  /// and returns 0 at once (scripted captures).
  /// `look` draws the M4 alert parts (icon, detail block); default: none.
  int run_dialog(const PromptDialog& dialog, bool wait, const DialogLook& look = {}) const;

 private:
  WidgetId id_ = kNoWidget;
  struct KeyHook;
  std::unique_ptr<KeyHook> key_hook_;
  struct CloseHook;
  std::unique_ptr<CloseHook> close_hook_;
};

// --- M4 (openspec/changes/m4-scripting/design.md D13) -------------------------------

/// One item of a MenuBar menu.
struct MenuBarEntry {
  std::string label;
  bool separator = false;
  bool checkable = false;
  bool checked = false;
  bool enabled = true;
  std::string description;  // accessible description (the item's command)
};

struct MenuBarMenuSpec {
  std::string title;
  std::vector<MenuBarEntry> entries;
};

/// Raven's MenuBar: the system menu bar on macOS (the first MenuBar of the
/// window, shown while it is active, taking no room), drawn in the window
/// elsewhere (board M4-Menu-Bar).
class MenuBar : public Widget {
 public:
  static std::unique_ptr<MenuBar> create();
  ~MenuBar();
  MenuBar(const MenuBar&) = delete;
  MenuBar& operator=(const MenuBar&) = delete;

  /// Replaces every menu; `on_choose(menu, entry)` runs when an item is chosen.
  void set_menus(const std::vector<MenuBarMenuSpec>& menus, std::function<void(std::size_t, std::size_t)> on_choose);
  /// True while the system shows the bar instead of the window.
  [[nodiscard]] bool native() const;
  /// false keeps the bar in the window on every system (Raven's
  /// Rmenu_bar_set_native): `--menu-bar-in-window` captures and tests.
  void set_native(bool native);

 private:
  explicit MenuBar(WidgetId id);
  struct State;
  std::unique_ptr<State> state_;
};

/// A Raven text label.
class Label : public Widget {
 public:
  static Label create(std::string_view text);
  void set_text(std::string_view text);
  void set_wrap(bool wrap);
  void set_selectable(bool selectable);
  void set_color(const Rgba& color);

 private:
  using Widget::Widget;
};

/// A Raven push button.
class Button : public Widget {
 public:
  static std::unique_ptr<Button> create(std::string_view text, bool primary);
  ~Button();
  Button(const Button&) = delete;
  Button& operator=(const Button&) = delete;
  void on_click(std::function<void()> callback);
  /// The accent fill of a primary button, or the plain control look: also
  /// marks the chosen button of a group (Raven buttons have no checked state).
  void set_primary(bool primary);

 private:
  explicit Button(WidgetId id);
  struct Callbacks;
  std::unique_ptr<Callbacks> callbacks_;
};

/// A Raven checkbox.
class CheckBox : public Widget {
 public:
  static std::unique_ptr<CheckBox> create(std::string_view text);
  ~CheckBox();
  CheckBox(const CheckBox&) = delete;
  CheckBox& operator=(const CheckBox&) = delete;
  void set_checked(bool checked);
  void on_toggle(std::function<void(bool)> callback);

 private:
  explicit CheckBox(WidgetId id);
  struct Callbacks;
  std::unique_ptr<Callbacks> callbacks_;
};

/// A Raven combo box (a select list).
class ComboBox : public Widget {
 public:
  static std::unique_ptr<ComboBox> create(const std::vector<std::string>& items);
  ~ComboBox();
  ComboBox(const ComboBox&) = delete;
  ComboBox& operator=(const ComboBox&) = delete;
  void set_selected(int index);
  void on_select(std::function<void(int)> callback);

 private:
  explicit ComboBox(WidgetId id);
  struct Callbacks;
  std::unique_ptr<Callbacks> callbacks_;
};

}  // namespace hefesto::shell::ui
