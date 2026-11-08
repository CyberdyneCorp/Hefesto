// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The main window "Hefesto": dock area with the canvas viewport and the
// layer, hierarchy and inspector panels (placeholders in M0), a minimal
// diagnostics panel (every diagnostic of the session, newest last; filtering
// and navigation to objects arrive with the M2 UI), the command bar and the
// status bar. Raven-free: it is built from the raven_ui wrappers.

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/render/camera.hpp"
#include "hefesto/render/canvas_index.hpp"
#include "hefesto/render/renderer.hpp"
#include "hefesto/shell/canvas_controller.hpp"
#include "hefesto/shell/command_bar_model.hpp"
#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/options.hpp"
#include "hefesto/shell/view_commands.hpp"
#include "raven_ui.hpp"

namespace hefesto::shell {

class MainWindow {
 public:
  /// `queue_mutex` is the lock shared with Raven around the Vulkan queue.
  /// The command bar runs commands through `core`'s Dispatcher; the window
  /// registers its view commands there. `core` must outlive the window.
  MainWindow(ui::Runtime& runtime, const ShellOptions& options, std::mutex& queue_mutex, CoreSession& core);
  ~MainWindow();
  MainWindow(const MainWindow&) = delete;
  MainWindow& operator=(const MainWindow&) = delete;

  /// Indexes and uploads rectangles to the canvas (UI thread).
  void show_layout(const std::vector<render::LayeredRect>& rects);

  [[nodiscard]] bool open() const { return window_.valid(); }
  [[nodiscard]] ui::FrameStats frame_stats() const { return window_.frame_stats(); }
  [[nodiscard]] std::size_t canvas_instances() const;
  bool capture(const std::string& path) const { return window_.capture_ppm(path); }
  [[nodiscard]] std::string window_system() const { return window_.window_system(); }

  /// Benchmark hooks: realtime redraw, and a programmatic pan in pixels.
  void set_realtime(bool realtime);
  void pan_pixels(double dx, double dy);

 private:
  /// Appends every diagnostic the Dispatcher reports to the diagnostics panel.
  class DiagnosticsPanelSink final : public diag::DiagnosticSink {
   public:
    explicit DiagnosticsPanelSink(ui::ListPanel& panel) : panel_(panel) {}
    void emit(const diag::Diagnostic& diagnostic) override;

   private:
    ui::ListPanel& panel_;
  };

  struct ViewSnapshot {
    render::ViewTransform transform;
    render::WorldWindow window;
    double units_per_pixel = 1.0;
  };

  void build_panels();
  void build_command_bar();
  void register_view_commands();
  void on_command(const std::string& text);
  void append_console(const std::string& command, const std::string& result);
  void wire_viewport();
  void apply_theme(ThemeMode mode);
  void camera_changed();
  void update_status(const std::string& message);
  void draw(const ui::ViewportDraw& target);

  ui::Runtime& runtime_;
  ShellOptions options_;
  std::mutex& queue_mutex_;

  ui::Window window_;
  ui::Box root_ = ui::Box::vertical();
  ui::Dock dock_ = ui::Dock::create();
  std::unique_ptr<ui::Viewport> viewport_;
  std::unique_ptr<ui::CommandBar> command_bar_;
  ui::Label status_ = ui::Label::create("");
  ui::ListPanel diagnostics_ = ui::ListPanel::create();
  ui::ListPanel console_ = ui::ListPanel::create();  // command echo and full results
  DiagnosticsPanelSink diagnostics_sink_{diagnostics_};
  std::string last_message_;

  std::unique_ptr<render::CanvasRenderer> renderer_;
  render::Camera camera_;
  CanvasController controller_{camera_};
  render::Bounds controller_bounds_;
  bool auto_fit_ = true;  // fit on resize until the user moves the view
  CoreSession& core_;
  CoreCommandSink core_sink_;
  CommandBarModel command_model_{core_sink_};

  mutable std::mutex view_mutex_;  // guards snapshot_ and index_ (render thread reads)
  ViewSnapshot snapshot_;
  std::shared_ptr<const render::CanvasIndex> index_;
  std::vector<render::LayerBatch> ranges_;  // render thread only
};

}  // namespace hefesto::shell
