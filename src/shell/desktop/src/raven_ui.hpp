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

#include "hefesto/render/renderer.hpp"
#include "hefesto/shell/canvas_controller.hpp"
#include "hefesto/shell/design_tokens.hpp"
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

/// Base of the widget wrappers: a Raven handle plus common properties.
class Widget {
 public:
  [[nodiscard]] WidgetId id() const noexcept { return id_; }
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

 private:
  explicit TextInput(WidgetId id);
  struct Callbacks;
  std::unique_ptr<Callbacks> callbacks_;
};

enum class Role : std::uint8_t { none, group, text, heading, button, toggle_button, checkbox, list, list_item, tree,
                                 tree_item, tab_list, tab, toolbar, status_bar, canvas, image, pane };

/// Has a screen reader read `text`; `assertive` interrupts what it is reading.
void announce(std::string_view text, bool assertive);

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

/// A list of text rows (Console, Diagnostics).
class ListPanel : public Widget {
 public:
  static ListPanel create();
  void add_item(std::string_view text);
  void clear();
  /// Scrolls so that the last item is visible (log-style panels).
  void scroll_to_end();

 private:
  using Widget::Widget;
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

class Window {
 public:
  Window(std::string_view title, int width, int height);
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
  [[nodiscard]] FrameStats frame_stats() const;
  /// Saves the last rendered frame as a binary PPM; false on failure.
  bool capture_ppm(const std::string& path) const;
  /// The window system Raven chose for this window: "x11", "wayland",
  /// "cocoa", "win32", or "none" (headless or unknown).
  [[nodiscard]] std::string window_system() const;
  void set_title(std::string_view title);
  void set_min_size(int width, int height);
  void set_clear_color(const Rgba& color);
  /// The system's open dialog for one file matching `patterns` ("*.mag");
  /// nullopt when cancelled.
  [[nodiscard]] std::optional<std::string> open_file_dialog(std::string_view title, std::string_view filter_name,
                                                            std::string_view patterns) const;

 private:
  WidgetId id_ = kNoWidget;
};

}  // namespace hefesto::shell::ui
