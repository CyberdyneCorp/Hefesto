// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "main_window.hpp"

#include <cstdio>
#include <sstream>

#include "hefesto/diag/format.hpp"
#include "hefesto/render/scene.hpp"

namespace hefesto::shell {

namespace {

constexpr const char* kLayerNames[] = {"diff", "poly", "li1", "contact", "met1", "via", "met2", "met3"};

std::string zoom_text(const render::Camera& camera) {
  std::ostringstream out;
  out.precision(3);
  out << camera.units_per_pixel() << " nm/px";
  return out.str();
}

}  // namespace

MainWindow::MainWindow(ui::Runtime& runtime, const ShellOptions& options, std::mutex& queue_mutex,
                       CoreSession& core)
    : runtime_(runtime),
      options_(options),
      queue_mutex_(queue_mutex),
      window_("Hefesto", options.width, options.height),
      viewport_(ui::Viewport::create()),
      command_bar_(ui::CommandBar::create("Type a command (help)")),
      core_(core),
      core_sink_(core.dispatcher()) {
  render::QueueLock lock{[this] { queue_mutex_.lock(); }, [this] { queue_mutex_.unlock(); }};
  renderer_ = std::make_unique<render::CanvasRenderer>(runtime_.vulkan_device(), std::move(lock));
  apply_theme(options_.theme);
  build_panels();
  build_command_bar();
  root_.add(dock_, 1.0F);
  root_.add(*command_bar_);
  root_.add(status_);
  window_.set_content(root_);
  wire_viewport();
  register_view_commands();
  controller_.set_commands(&core_.dispatcher());
  core_.dispatcher().add_sink(diagnostics_sink_);
  if (core_.log_warning()) {
    core_.dispatcher().report(*core_.log_warning());
  }
  update_status("ready");
}

void MainWindow::DiagnosticsPanelSink::emit(const diag::Diagnostic& diagnostic) {
  panel_.add_item(diag::format_text(diagnostic));
}

MainWindow::~MainWindow() {
  core_.dispatcher().remove_sink(diagnostics_sink_);
  // First stop Raven from rendering the window: its draw callback uses the
  // members below (renderer, view mutex) on the render thread.
  window_.destroy();
}

void MainWindow::build_panels() {
  viewport_->set_accessible_name("Layout canvas");
  viewport_->set_tooltip("Layout canvas: wheel zooms, middle/right drag pans, F fits");
  dock_.add_panel(*viewport_, "Layout", ui::DockSide::tab, nullptr, 0.0F);

  auto layers = ui::ListPanel::create();
  for (const char* name : kLayerNames) {
    layers.add_item(name);
  }
  layers.set_accessible_name("Layers");
  dock_.add_panel(layers, "Layers", ui::DockSide::left, nullptr, 0.18F);

  auto hierarchy = ui::TreePanel::create();
  const ui::WidgetId top = hierarchy.add_item(ui::kNoWidget, "spike_top (synthetic)");
  hierarchy.add_item(top, "(cell hierarchy arrives in M1)");
  hierarchy.set_accessible_name("Cell hierarchy");
  dock_.add_panel(hierarchy, "Hierarchy", ui::DockSide::bottom, &layers, 0.45F);

  auto inspector = ui::Label::create("Nothing selected");
  inspector.set_accessible_name("Inspector");
  dock_.add_panel(inspector, "Inspector", ui::DockSide::right, nullptr, 0.2F);

  diagnostics_.set_font(runtime_.mono_font());
  diagnostics_.set_accessible_name("Diagnostics");
  dock_.add_panel(diagnostics_, "Diagnostics", ui::DockSide::bottom, viewport_.get(), 0.2F);
}

void MainWindow::build_command_bar() {
  command_bar_->set_accessible_name("Command");
  command_bar_->set_font(runtime_.mono_font());
  status_.set_font(runtime_.mono_font());
  status_.set_accessible_name("Status");
  command_bar_->on_commit([this](const std::string& text) { on_command(text); });
  command_bar_->on_history([this](bool older) { return older ? command_model_.previous() : command_model_.next(); });
}

void MainWindow::register_view_commands() {
  const ViewActions actions{[this] {
                              camera_.fit(controller_bounds_);
                              camera_changed();
                            },
                            [this](double factor) {
                              camera_.zoom_at(factor, camera_.viewport_width() / 2.0,
                                              camera_.viewport_height() / 2.0);
                              camera_changed();
                            },
                            [this](ThemeMode mode) { apply_theme(mode); },
                            [this](double dx, double dy) {
                              camera_.pan_pixels(dx, dy);
                              camera_changed();
                            }};
  if (!shell::register_view_commands(core_.registry(), actions)) {
    std::fprintf(stderr, "hefesto: a view command name is already registered\n");
  }
}

void MainWindow::on_command(const std::string& text) {
  const CommandOutcome outcome = command_model_.submit(text);
  command_bar_->set_text("");
  for (const std::string& d : outcome.diagnostics) {
    std::fprintf(stderr, "%s\n", d.c_str());
  }
  // Multi-line results (e.g. `help`) go to stdout; the status bar shows the first line.
  const std::string& status = command_model_.status_text();
  const std::size_t newline = status.find('\n');
  if (newline != std::string::npos) {
    std::fprintf(stdout, "%s\n", status.c_str());
  }
  update_status(status.substr(0, newline));
  if (core_.quit_requested()) {
    runtime_.quit();
  }
}

void MainWindow::wire_viewport() {
  viewport_->on_draw([this](const ui::ViewportDraw& target) { draw(target); });
  viewport_->on_resize([this](int width, int height) {
    camera_.set_viewport(width, height);
    controller_.set_pixel_ratio(static_cast<double>(window_.scale()));
    if (auto_fit_) {
      camera_.fit(controller_bounds_);
    }
    camera_changed();
    update_status(last_message_);
  });
  viewport_->on_input([this](const CanvasInput& input) {
    const bool changed = controller_.handle(input);
    auto_fit_ = auto_fit_ && !changed;
    if (changed) {
      camera_changed();
    }
    return changed;
  });
}

void MainWindow::apply_theme(ThemeMode mode) {
  options_.theme = mode;
  if (!runtime_.apply_theme(mode, options_.font_dir)) {
    std::fprintf(stderr, "hefesto: Inter / JetBrains Mono not found in %s; using Raven's fonts\n",
                 options_.font_dir.c_str());
  }
}

void MainWindow::show_layout(const std::vector<render::LayeredRect>& rects) {
  auto index = std::make_shared<const render::CanvasIndex>(render::CanvasIndex::build(rects));
  const render::UploadStats upload = renderer_->upload(index->instances());
  controller_bounds_ = index->bounds();
  controller_.set_bounds(controller_bounds_);
  camera_.fit(controller_bounds_);
  {
    const std::lock_guard guard(view_mutex_);
    index_ = std::move(index);
  }
  camera_changed();
  std::ostringstream msg;
  msg << rects.size() << " rectangles uploaded in " << upload.seconds * 1e3 << " ms";
  update_status(msg.str());
}

std::size_t MainWindow::canvas_instances() const {
  const std::lock_guard guard(view_mutex_);
  return index_ ? index_->instances().size() : 0;
}

void MainWindow::set_realtime(bool realtime) {
  viewport_->set_realtime(realtime);
}

void MainWindow::pan_pixels(double dx, double dy) {
  camera_.pan_pixels(dx, dy);
  camera_changed();
}

void MainWindow::camera_changed() {
  {
    const std::lock_guard guard(view_mutex_);
    snapshot_ = ViewSnapshot{camera_.transform(), camera_.visible_window(), camera_.units_per_pixel()};
  }
  viewport_->invalidate();
}

void MainWindow::update_status(const std::string& message) {
  last_message_ = message;
  status_.set_text(message + "  |  " + zoom_text(camera_) + "  |  " + std::string(to_string(options_.theme)));
}

void MainWindow::draw(const ui::ViewportDraw& target) {
  ViewSnapshot view;
  std::shared_ptr<const render::CanvasIndex> index;
  {
    const std::lock_guard guard(view_mutex_);
    view = snapshot_;
    index = index_;
  }
  if (index) {
    index->visible_ranges(view.window, view.units_per_pixel, ranges_);
  } else {
    ranges_.clear();
  }
  // The canvas keeps the design-system "canvas" color in both themes.
  const Rgba& bg = theme_tokens(ThemeMode::dark).canvas;
  render::DrawTarget draw_target{target.command_buffer, target.render_pass, target.x, target.y, target.width,
                                 target.height, false, true, {bg.r, bg.g, bg.b, 1.0F}};
  renderer_->record(draw_target, view.transform, ranges_);
}

}  // namespace hefesto::shell
