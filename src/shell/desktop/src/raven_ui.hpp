// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The C++20 wrapper over Raven's C API (design.md D5): Runtime, Window, Dock,
// Box, Label, ListPanel, TreePanel, CommandBar and Viewport. Every Raven call
// of Hefesto goes through raven_ui.cpp, which is the only file that includes
// <Raven.h>; this header is Raven-free (widgets are plain 32-bit handles), so
// the rest of the shell never sees a Raven type.
//
// Threading: Raven's UI thread is the thread that created the Runtime. Every
// method runs on it, except the viewport draw callback, which Raven calls on
// its render thread.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "hefesto/render/renderer.hpp"
#include "hefesto/shell/canvas_controller.hpp"
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

  /// Applies the forge tokens as the Raven theme; loads Inter / JetBrains
  /// Mono from `font_dir` when given. Returns false if fonts were requested but
  /// could not be loaded (Raven's bundled fonts stay in use).
  bool apply_theme(ThemeMode mode, const std::string& font_dir);
  /// The monospace font for commands and coordinates (JetBrains Mono when loaded).
  [[nodiscard]] WidgetId mono_font() const noexcept { return mono_font_; }

 private:
  Runtime() = default;
  struct Callbacks;
  std::unique_ptr<Callbacks> callbacks_;
  WidgetId ui_font_ = kNoWidget;
  WidgetId mono_font_ = kNoWidget;
};

/// Base of the widget wrappers: a Raven handle plus common properties.
class Widget {
 public:
  [[nodiscard]] WidgetId id() const noexcept { return id_; }
  void set_accessible_name(std::string_view name);
  void set_tooltip(std::string_view text);
  void set_font(WidgetId font);
  void focus();

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
  void set_padding(float left, float top, float right, float bottom);
  void set_spacing(float spacing);

 private:
  using Widget::Widget;
};

class Label : public Widget {
 public:
  static Label create(std::string_view text);
  void set_text(std::string_view text);

 private:
  using Widget::Widget;
};

/// A list (the layer palette placeholder).
class ListPanel : public Widget {
 public:
  static ListPanel create();
  void add_item(std::string_view text);
  /// Scrolls so that the last item is visible (log-style panels).
  void scroll_to_end();

 private:
  using Widget::Widget;
};

/// A tree (the cell hierarchy placeholder).
class TreePanel : public Widget {
 public:
  static TreePanel create();
  /// Adds an item under `parent` (kNoWidget: top level); returns its handle.
  WidgetId add_item(WidgetId parent, std::string_view text);

 private:
  using Widget::Widget;
};

enum class DockSide : std::uint8_t { left, right, top, bottom, tab };

class Dock : public Widget {
 public:
  static Dock create();
  /// Adds `panel` with a tab titled `title` next to `relative` (null: the whole area).
  void add_panel(const Widget& panel, std::string_view title, DockSide side, const Widget* relative, float size);

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

 private:
  WidgetId id_ = kNoWidget;
};

}  // namespace hefesto::shell::ui
