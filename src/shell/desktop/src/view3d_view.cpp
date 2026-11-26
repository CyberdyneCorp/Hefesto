// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The desktop's 3D view (view3d_view.hpp). Owner: SHELL.

#include "view3d_view.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/render/mesh3d_shading.hpp"
#include "hefesto/script/binding/window_commands.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/view3d/camera/view_state.hpp"
#include "hefesto/view3d/client/commands.hpp"
#include "hefesto/view3d/client/scene.hpp"
#include "hefesto/view3d/stack/ghost.hpp"
#include "hefesto/view3d/stack/substrate.hpp"
#include "screen_paint.hpp"
#include "side_panels.hpp"

namespace hefesto::shell {

namespace {

constexpr float kRowHeight = 36.0F;
constexpr float kControlHeight = 28.0F;
constexpr int kSeparateWidth = 960;
constexpr int kSeparateHeight = 640;
constexpr std::string_view kViewportTooltip =
    "3D view: drag orbits; Option-drag, right-drag or two-finger scroll pans; wheel or pinch zooms; click picks; "
    "keys run the 3D window's bindings (L/l level, arrows scroll, Z/z zoom, space defaults)";

Rgba rgb_of(const std::array<float, 3>& c) {
  return Rgba{c[0], c[1], c[2], 1.0F};
}

Rgba alpha_of(Rgba c, float a) {
  c.a *= a;
  return c;
}

}  // namespace

/// What the worker's callbacks reach (through tasks posted to the UI loop).
struct View3dView::Shared {
  std::mutex mutex;
  double progress = 0.0;
  bool progress_posted = false;
};

/// The widgets of one layer row (a pool that only grows; rows past the
/// current stack are hidden).
struct View3dView::Row {
  ui::Box box = ui::Box::horizontal();
  std::unique_ptr<ui::Painted> label;
  std::unique_ptr<ui::Painted> hide;
  std::unique_ptr<ui::Painted> isolate;
  std::unique_ptr<ui::Painted> alpha;
};

View3dView::View3dView(ScreenContext& ctx, std::mutex& queue_mutex, std::function<float()> pixel_ratio)
    : ctx_(ctx),
      queue_mutex_(queue_mutex),
      pixel_ratio_(std::move(pixel_ratio)),
      viewport_(ui::Viewport::create()),
      shared_(std::make_shared<Shared>()) {
  build_stage();
  build_side();
  std::weak_ptr<Shared> alive = shared_;
  view3d::WorkerCallbacks callbacks;
  callbacks.on_progress = [alive, this](double fraction) {
    const std::shared_ptr<Shared> s = alive.lock();
    if (!s) return;
    const std::lock_guard lock(s->mutex);
    s->progress = fraction;
    if (s->progress_posted) return;
    s->progress_posted = true;
    ui::post_task([alive, this] {
      const std::shared_ptr<Shared> s2 = alive.lock();
      if (!s2) return;
      {
        const std::lock_guard l(s2->mutex);
        s2->progress_posted = false;
      }
      stage_paint_->invalidate();
      if (status_paint_) status_paint_->invalidate();
    });
  };
  callbacks.on_done = [alive, this](const view3d::MeshHandle& model) {
    ui::post_task([alive, this, model] {
      if (alive.lock()) take_model(model);
    });
  };
  worker_ = std::make_unique<view3d::MeshWorker>(std::move(callbacks));
  ctx_.editor().view3d.display = this;
  ctx_.editor().windows.add_observer(*this);
}

View3dView::~View3dView() {
  close_separate_window();  // its viewport wraps the target's images
  if (ctx_.editor().view3d.display == this) ctx_.editor().view3d.display = nullptr;
  ctx_.editor().windows.remove_observer(*this);
  worker_.reset();  // cancels and joins: no callback runs after this
  shared_.reset();  // tasks posted earlier see it gone
  viewport_.reset();  // destroys the wrapped texture before the target's images
  renderer_.reset();
  target_.reset();
}

// --- widgets -------------------------------------------------------------------------

void View3dView::build_stage() {
  stage_paint_ = ui::Painted::create(ctx_.runtime, [this](ui::Painter& p, float w, float h) { paint_stage(p, w, h); });
  stage_paint_->set_hit_testable(false);
  stage_paint_->set_role(ui::Role::text);  // the header chip; the gizmo is decoration
  viewport_->set_accessible_name("3D view");
  viewport_->set_tooltip(kViewportTooltip);
  stage_.add(*viewport_);
  stage_.add(*stage_paint_);
  stage_.set_accessible_name("3D view");
  connect_viewport(*viewport_, false);
  build_toolbar();
  stage_.add(toolbar_);
  toolbar_.place_bottom_left(kView3dToolbarMargin, kView3dToolbarMargin);  // floats over the view, as the board
  root_.add(stage_, 1.0F);
}

void View3dView::build_side() {
  auto edge = make_separator(ctx_, true);
  edge->set_accessible_hidden(true);
  root_.add(*edge);
  painted_.push_back(std::move(edge));
  side_panel_.set_fill(ctx_.tokens().surface1);
  side_panel_.set_size(kView3dSideWidth, -1.0F);
  side_panel_.set_accessible_name("3D view controls");
  ui::set_container_role(side_panel_, ui::Role::pane);
  side_column_.set_padding(kView3dSidePad, kView3dSidePad, kView3dSidePad, kView3dSidePad);
  side_column_.set_spacing(6.0F);
  build_actions(side_column_);
  // The layer stack scrolls (sky130A has 349 rows); View and Inspect stay
  // shown below it, as the board pins projection and presets to the bottom.
  side_.set_spacing(6.0F);
  side_.set_padding(0.0F, 0.0F, kView3dScrollBarRoom, 0.0F);  // the rows end before the scroll bar
  build_layer_section(side_);
  side_scroll_.add(side_);
  side_column_.add(side_scroll_, 1.0F);
  side_bottom_.set_spacing(6.0F);
  build_view_section(side_bottom_);
  build_inspect_section(side_bottom_);
  build_section_panel(side_bottom_);
  side_column_.add(side_bottom_);
  side_panel_.set_child(side_column_);
  root_.add(side_panel_);
}

void View3dView::add_section(ui::Box& box, std::string_view title) {
  auto label = make_painted(ctx_, [title = std::string{title}](ui::Painter& p, float, float h, const ThemeTokens& t) {
    paint::section_label(p, 0.0F, h - 16.0F, title, t);
  });
  label->set_size(-1.0F, 24.0F);
  label->set_role(ui::Role::heading);
  label->set_accessible_name(title);
  box.add(*label);
  painted_.push_back(std::move(label));
}

View3dControlState View3dView::control_state() const {
  const view3d::WindowState* w = window();
  View3dControlState s;
  s.window_open = w != nullptr;
  s.mask_mode = w != nullptr && w->mode == view3d::Mode::mask_layer;
  s.cut_box = w != nullptr && w->cut_box.has_value();
  s.perspective = options_.perspective;
  s.span_connected_layers = options_.span_connected_layers;
  s.frame_time = options_.show_frame_time;
  s.picked = picking_.picked().has_value();
  s.net_isolated = picking_.net_isolated();
  s.section_on = section_line_.has_value();
  s.substrate_on = substrate_on_;
  s.substrate_available = true;
  s.export_available = export_registered() && ctx_.editor().file_picker != nullptr;
  s.visibility_editable = w != nullptr && visibility_editable(*w);
  s.separate_window = separate_window_ != nullptr;
  s.separate_window_available = true;
  return s;
}

ui::Painted& View3dView::add_button(ui::Box& box, View3dControl id, std::function<void()> click, float width) {
  Control control{make_interactive(ctx_,
                                   [this, id](ui::Painter& p, float w, float h, const ThemeTokens& t, bool hovered,
                                              bool focused) {
                                     const View3dControlView v = control_view(id, control_state());
                                     const bool active = v.on.value_or(false);
                                     paint::button(p, w, h, v.label, "",
                                                   active ? paint::ButtonStyle::primary : paint::ButtonStyle::secondary,
                                                   paint::WidgetState{hovered, focused, v.enabled, active}, t);
                                   }),
                  id};
  const View3dControlView v = control_view(id, control_state());
  ui::Painted& widget = *control.widget;
  widget.set_size(width > 0.0F ? width : paint::button_width(ctx_.runtime, v.label, false), kControlHeight);
  widget.set_role(v.on ? ui::Role::toggle_button : ui::Role::button);
  widget.set_accessible_name(v.accessible_name);
  widget.set_tooltip(v.tooltip);
  widget.on_click([this, id, click = std::move(click)](float, float) {
    if (control_view(id, control_state()).enabled) click();
  });
  box.add(widget);
  controls_.push_back(std::move(control));
  return widget;
}

void View3dView::add_pair(ui::Box& box, std::string_view group_name,
                          std::pair<View3dControl, std::function<void()>> left,
                          std::pair<View3dControl, std::function<void()>> right) {
  constexpr float kGap = 6.0F;
  const float cell = grid_cell_width(side_content_width(), 2, kGap);
  ui::Box& line = boxes_.emplace_back(ui::Box::horizontal());
  line.set_spacing(kGap);
  line.set_accessible_name(group_name);
  ui::set_container_role(line, ui::Role::group);
  add_button(line, left.first, std::move(left.second), cell);
  add_button(line, right.first, std::move(right.second), cell);
  box.add(line);
}

void View3dView::build_actions(ui::Box& box) {
  // The board's header actions (Cross-section, Export glTF…), P2.
  add_pair(box, "3D actions", {View3dControl::cross_section, [this] { toggle_section(); }},
           {View3dControl::export_model, [this] { export_model(); }});
}

void View3dView::build_layer_section(ui::Box& box) {
  add_section(box, "Layer stack");
  rows_box_.set_spacing(2.0F);
  rows_box_.set_accessible_name("Layer stack, top layer first");
  ui::set_container_role(rows_box_, ui::Role::list);
  box.add(rows_box_);
  auto note = make_painted(ctx_, [](ui::Painter& p, float, float h, const ThemeTokens& t) {
    p.text_centered_y(TextRole::meta, 0.0F, h / 2.0F, "Heights from the tech file, in microns.", t.text_secondary);
  });
  note->set_size(-1.0F, 20.0F);
  note->set_role(ui::Role::text);
  note->set_accessible_name("Heights and thicknesses come from the tech file (render and height), in microns.");
  box.add(*note);
  painted_.push_back(std::move(note));
  ui::Box& line = boxes_.emplace_back(ui::Box::horizontal());
  add_button(line, View3dControl::show_all_layers, [this] { run(show_all_command(path())); });
  box.add(line);
}

void View3dView::build_view_section(ui::Box& box) {
  add_section(box, "View");
  const auto projection = [this](bool perspective) {
    return [this, perspective] {
      options_.perspective = perspective;
      dirty_ = true;
      repaint();
    };
  };
  add_pair(box, "Projection", {View3dControl::orthographic, projection(false)},
           {View3dControl::perspective, projection(true)});
  // The board's two-column camera grid: fits the panel at any window width
  // (in one row, "Side" ran past the panel at 1280 x 800).
  const auto preset = [this](view3d::Preset which) { return [this, which] { apply_camera(which); }; };
  add_pair(box, "Camera", {View3dControl::isometric, preset(view3d::Preset::isometric)},
           {View3dControl::top, preset(view3d::Preset::top)});
  add_pair(box, "Camera", {View3dControl::front, preset(view3d::Preset::front)},
           {View3dControl::side, preset(view3d::Preset::side)});
}

void View3dView::build_inspect_section(ui::Box& box) {
  add_section(box, "Inspect");
  ui::Box& net = boxes_.emplace_back(ui::Box::horizontal());
  add_button(net, View3dControl::isolate_net, [this] {
    if (picking_.toggle_isolate_net(uploaded_.get(), ctx_.editor())) {
      highlight_dirty_ = true;
      dirty_ = true;
      repaint();
    }
  });
  box.add(net);
  ui::Box& span = boxes_.emplace_back(ui::Box::horizontal());
  add_button(span, View3dControl::span_connected_layers, [this] {
    options_.span_connected_layers = !options_.span_connected_layers;
    sync_pending_ = true;  // a full build with the J1 option
    repaint();
  });
  box.add(span);
  add_pair(box, "View options",
           {View3dControl::substrate,
            [this] {
              substrate_on_ = !substrate_on_;
              dirty_ = true;
              repaint();
            }},
           {View3dControl::frame_time, [this] {
              options_.show_frame_time = !options_.show_frame_time;
              dirty_ = true;
              repaint();
            }});
  add_pair(box, "Window", {View3dControl::separate_window, [this] { toggle_separate_window(); }},
           {View3dControl::close_view, [this] { run(close_command(path())); }});
}

void View3dView::build_section_panel(ui::Box& box) {
  section_paint_ = make_painted(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens&) {
    paint_section(p, w, h);
  });
  section_paint_->set_size(-1.0F, 132.0F);
  section_paint_->set_role(ui::Role::image);
  section_paint_->set_accessible_name("Cross-section A–A′");
  section_paint_->set_visible(false);
  box.add(*section_paint_);
}

void View3dView::build_toolbar() {
  toolbar_.set_spacing(kView3dToolbarSpacing);
  toolbar_.set_accessible_name("3D view toolbar");
  ui::set_container_role(toolbar_, ui::Role::toolbar);
  for (int i = 0; i < 4; ++i) {
    ui::Box& row = toolbar_rows_.emplace_back(ui::Box::horizontal());
    row.set_spacing(kView3dToolbarSpacing);
    toolbar_.add(row);
  }
  ui::Box& mode = boxes_.emplace_back(ui::Box::horizontal());
  mode.set_spacing(4.0F);
  mode.set_accessible_name("Layer source (cif)");
  ui::set_container_role(mode, ui::Role::group);
  const auto is_mask = [this] { return window() != nullptr && window()->mode == view3d::Mode::mask_layer; };
  add_button(mode, View3dControl::mask_layers, [this, is_mask] {
    if (!is_mask()) run(mode_command(path()));
  });
  add_button(mode, View3dControl::layout_layers, [this, is_mask] {
    if (is_mask()) run(mode_command(path()));
  });
  toolbar_groups_.push_back({&mode, paint::button_width(ctx_.runtime, "Mask layers", false) +
                                        paint::button_width(ctx_.runtime, "Layout layers", false) + 4.0F});
  ui::Box& level = boxes_.emplace_back(ui::Box::horizontal());
  level.set_spacing(4.0F);
  level.set_accessible_name("Rendering level");
  ui::set_container_role(level, ui::Role::group);
  constexpr float kStep = 30.0F;
  add_button(level, View3dControl::level_down, [this] { run(level_command(path(), -1)); }, kStep);
  level_paint_ = make_painted(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens&) {
    paint_level(p, w, h);
  });
  const float level_width = std::ceil(ctx_.runtime.text_width(TextRole::body, level_text(8)) + 20.0F);
  level_paint_->set_size(level_width, kControlHeight);
  level_paint_->set_role(ui::Role::text);
  level_paint_->set_accessible_name(level_text(1));
  level_paint_->set_tooltip("Rendering level (level): MSAA at level 1 and above");
  level.add(*level_paint_);
  add_button(level, View3dControl::level_up, [this] { run(level_command(path(), 1)); }, kStep);
  toolbar_groups_.push_back({&level, 2.0F * kStep + level_width + 8.0F});
  ui::Box& cut = boxes_.emplace_back(ui::Box::horizontal());
  add_button(cut, View3dControl::cut_box,
             [this] { run(cutbox_command(path(), !(window() != nullptr && window()->cut_box.has_value()))); });
  toolbar_groups_.push_back({&cut, paint::button_width(ctx_.runtime, "Cut box", false)});
  status_paint_ = make_painted(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens&) {
    paint_status(p, w, h);
  });
  status_paint_->set_size(160.0F, kControlHeight);
  status_paint_->set_role(ui::Role::status_bar);
  status_paint_->set_accessible_name("Mesh status");
  toolbar_groups_.push_back({status_paint_.get(), 160.0F});
  layout_toolbar(0.0F);
}

void View3dView::layout_toolbar(float width) {
  toolbar_width_ = width;
  std::vector<float> widths;
  widths.reserve(toolbar_groups_.size());
  for (const ToolbarGroup& g : toolbar_groups_) widths.push_back(g.width);
  // Before the first layout (width 0) every group goes on one row.
  const float available = width > 0.0F ? width - 2.0F * kView3dToolbarMargin : 1.0e9F;
  const std::vector<std::size_t> rows = flow_rows(widths, available, kView3dToolbarSpacing);
  std::vector<bool> used(toolbar_rows_.size(), false);
  for (std::size_t i = 0; i < toolbar_groups_.size(); ++i) {
    const std::size_t row = std::min(rows[i], toolbar_rows_.size() - 1);
    toolbar_rows_[row].add(*toolbar_groups_[i].widget);  // moves it from its previous row
    used[row] = true;
  }
  for (std::size_t r = 0; r < toolbar_rows_.size(); ++r) toolbar_rows_[r].set_visible(used[r]);
}

void View3dView::fit_toolbar() {
  ToolbarGroup& status = toolbar_groups_.back();
  const float needed = std::ceil(ctx_.runtime.text_width(TextRole::mono_small, status_line()) + 34.0F);
  bool relayout = false;
  if (std::abs(needed - status.width) > 0.5F) {
    status.width = needed;
    status_paint_->set_size(needed, kControlHeight);
    relayout = true;
  }
  const float width = stage_.rect().width;
  if (width > 0.0F && std::abs(width - toolbar_width_) > 0.5F) relayout = true;
  if (relayout) layout_toolbar(width);
}

float View3dView::toolbar_cover() const {
  const ui::Rect r = toolbar_.rect();
  return r.height > 0.0F ? r.height + kView3dToolbarMargin : 0.0F;
}

View3dView::Row& View3dView::row_widgets(std::size_t index) {
  while (rows_widgets_.size() <= index) {
    const std::size_t i = rows_widgets_.size();
    auto row = std::make_unique<Row>();
    row->box.set_spacing(kView3dRowSpacing);
    row->label = make_painted(ctx_, [this, i](ui::Painter& p, float w, float h, const ThemeTokens&) {
      paint_row(p, w, h, i, false, false);
    });
    row->label->set_size(-1.0F, kRowHeight);
    row->label->set_role(ui::Role::list_item);
    row->box.add(*row->label, 1.0F);
    const auto make = [this, &row](std::function<std::string()> label, std::function<void()> click,
                                   std::string_view tooltip) {
      auto w = make_interactive(ctx_, [label](ui::Painter& p, float wd, float ht, const ThemeTokens& t, bool hov,
                                              bool foc) {
        paint::button(p, wd, ht, label(), "", paint::ButtonStyle::ghost, paint::WidgetState{hov, foc, true, false}, t);
      });
      w->set_size(kView3dRowButtonWidth, kRowHeight);
      w->set_role(ui::Role::button);
      w->set_tooltip(tooltip);
      w->on_click([click](float, float) { click(); });
      row->box.add(*w);
      return w;
    };
    row->hide = make([this, i] { return i < rows_.size() && !rows_[i].visible ? std::string{"Show"} : "Hide"; },
                     [this, i] { toggle_row(i); }, "Show or hide the layer: see / see no");
    row->isolate = make([] { return std::string{"Solo"}; }, [this, i] { isolate_row(i); },
                        "Isolate: hide every other layer (see no *, see layer)");
    row->alpha = make([this, i] {
                        const float a = i < rows_.size() ? rows_[i].alpha : 1.0F;
                        return std::to_string(static_cast<int>(a * 100.0F + 0.5F)) + "%";
                      },
                      [this, i] { cycle_alpha(i); }, "Transparency: 100 %, 50 %, 20 % (kept outside Magic's state)");
    rows_box_.add(row->box);
    rows_widgets_.push_back(std::move(row));
  }
  return *rows_widgets_[index];
}

// --- session -------------------------------------------------------------------------

const view3d::WindowState* View3dView::window() const {
  const std::optional<view3d::WindowState>& w = ctx_.core.editor().view3d.window;
  return w ? &*w : nullptr;
}

bool View3dView::window_open() const {
  return window() != nullptr;
}

std::string View3dView::path() const {
  const view3d::WindowState* w = window();
  return w != nullptr ? w->toolkit_path : std::string{view3d::kDefaultToolkitPath};
}

bool View3dView::magic_compat() const {
  return ctx_.core.project().compat.magic_compat;
}

std::int32_t View3dView::mesh_units() const noexcept {
  return uploaded_ ? uploaded_->units_per_root : 1;  // the drawn geometry's units
}

void View3dView::run(const std::string& line) {
  if (ctx_.run) ctx_.run(line);
}

void View3dView::set_active(bool active) {
  active_ = active;
  dirty_ = true;
  if (active) viewport_->focus();
}

std::string View3dView::route_typed(const std::string& line) const {
  if (!active_ || !window_open()) return line;
  const cmd::CommandRegistry& registry = ctx_.core.registry();
  return route_typed_line(line, path(), [&registry](std::string_view word) {
    return registry.find(word, view3d::kWind3dClient) != nullptr;
  });
}

void View3dView::refresh() {
  highlight_dirty_ = true;
  dirty_ = true;
}

void View3dView::redraw(const editor::EditorState& /*editor*/, const view3d::WindowState& /*window*/) {
  sync_pending_ = true;  // done in on_frame, outside the command
}

void View3dView::closed(const view3d::WindowState& /*window*/) {
  sync_pending_ = true;
}

void View3dView::on_window_changed(view::WindowId id, view::Change change) {
  if (change != view::Change::content || !have_key_) return;
  const view::LayoutWindow* layout = ctx_.editor().windows.find(id);
  if (layout == nullptr) return;
  const std::optional<ChangeAreas> areas = change_areas(*layout, key_.cell);
  if (!areas) return;
  if (areas->all) {
    have_key_ = false;  // a full build on the next frame
  } else {
    worker_->changed(areas->areas);
  }
  sync_pending_ = true;  // the bounding box (the clip area) may have changed
}

// The client announces a new window's toolkit path through
// ::hefesto::tk::window_command (hefesto-cli's wrapper mode defines it): the
// desktop defines it too, so `<path> <cmd>` is a Tcl command routed to the
// `wind3d` table, as in Magic (the controls and focus routing use it).
void View3dView::ensure_window_hook() {
  script::Interpreter* interp = ctx_.editor().script.interp.get();
  if (interp == nullptr || interp->has_command("::hefesto::tk::window_command")) return;
  cmd::Dispatcher& dispatcher = ctx_.core.dispatcher();
  interp->define_command("::hefesto::tk::window_command", [interp, &dispatcher](const cmd::Invocation& inv) {
    return script::register_window_commands(*interp, dispatcher, inv.args);
  });
}

void View3dView::ensure_path_command() {
  script::Interpreter* interp = ctx_.editor().script.interp.get();
  const view3d::WindowState* w = window();
  if (interp == nullptr || w == nullptr || interp->has_command(w->toolkit_path)) return;
  const std::vector<std::string> paths{w->toolkit_path};
  (void)script::register_window_commands(*interp, ctx_.core.dispatcher(), paths);
}

void View3dView::on_frame() {
  ensure_window_hook();
  const bool open = window_open();
  if (open != was_open_) {
    was_open_ = open;
    if (!open) reset_window_state();
    if (open) ensure_path_command();  // a window opened before the hook was defined
    sync_pending_ = open;
    if (open_changed_) open_changed_(open);
  }
  fit_toolbar();
  if (separate_close_requested_) close_separate_window();
  if (!open) return;
  if (sync_pending_) sync();
  apply_default_camera();
  if (dirty_) draw_frame();
}

void View3dView::reset_window_state() {
  close_separate_window();
  worker_->cancel();
  model_.reset();
  drawn_stack_.reset();
  build_stack_.reset();
  have_key_ = false;
  picking_.reset();
  highlight_ = nullptr;
  pick_caption_.clear();
  rows_.clear();
  ghost_.clear();
  drawable_.clear();
  pending_drawable_.clear();
  options_.alpha.clear();
  options_.alpha_override.clear();  // a new window starts from the ghosted defaults
  section_line_.reset();
  section_first_.reset();
  section_ = {};
  section_paint_->set_visible(false);
  (void)camera_latch_.take(false);  // and from the default camera
  refresh_rows();
  stage_paint_->invalidate();
  if (status_paint_) status_paint_->invalidate();
}

void View3dView::sync() {
  sync_pending_ = false;
  const view3d::WindowState* w = window();
  if (w == nullptr || !ctx_.editor().technology) return;
  // The stack and source of the window's mode: layout layers, or the mask
  // layers of the current output style (MASKSRC, task 10.2).
  diag::CollectingSink stack_diagnostics;
  view3d::WindowBuild wb = view3d::window_build(ctx_.editor(), *w, magic_compat(), stack_diagnostics);
  const BuildKey key = build_key(wb.inputs, options_.span_connected_layers);
  if (!have_key_ || !(key == key_)) {
    // A `render` statement whose style does not resolve, once per build (decision J2).
    for (const diag::Diagnostic& d : stack_diagnostics.take()) ctx_.core.dispatcher().report(d);
    start_build(key, std::move(wb.inputs));
  }
  refresh_rows();
  repaint();
  dirty_ = true;
  viewport_->invalidate();
}

void View3dView::start_build(const BuildKey& key, view3d::BuildInputs inputs) {
  key_ = key;
  have_key_ = true;
  build_stack_ = inputs.stack;
  const display::StyleSet* styles = ctx_.editor().styles ? &*ctx_.editor().styles : nullptr;
  // The rows and frames keep the drawn stack's flags until this build's
  // model is drawn (take_model): the two stacks' layer ids may differ.
  pending_drawable_.clear();
  for (const render::LayerPaint& paint : render::layer_paints(*build_stack_, styles)) {
    pending_drawable_.push_back(paint.drawable);
  }
  const bool drawing = drawn_stack_ != nullptr;
  if (!drawing) drawable_ = pending_drawable_;
  ghost_.building(*build_stack_, view3d::ghosted_by_default(*build_stack_, *ctx_.editor().technology), drawing,
                  options_);
  if (key.cell == nullptr) {
    ++build_id_;  // a model of an earlier build is stale now
    worker_->cancel();
    return;
  }
  held_tech_.push_back(ctx_.editor().technology);
  {
    const std::lock_guard lock(shared_->mutex);
    shared_->progress = 0.0;
  }
  view3d::BuildInputs in = build_inputs(ctx_.editor(), std::move(inputs), options_.span_connected_layers);
  in.build_id = ++build_id_;
  worker_->build(std::move(in));
}

void View3dView::take_model(const view3d::MeshHandle& model) {
  if (worker_->idle() && held_tech_.size() > 1) held_tech_.erase(held_tech_.begin(), held_tech_.end() - 1);
  if (!model || !build_stack_ || !window_open()) return;
  // A model of an earlier build (one finishing as a new one started, posted
  // before or after start_build): its stack, mode, units or style differ
  // from build_stack_'s, even with as many layers; the next one replaces it.
  if (!model_is_current(*model, build_id_)) return;
  model_ = model;
  drawn_stack_ = build_stack_;
  drawable_ = pending_drawable_;
  ghost_.drawn(*drawn_stack_, options_);
  picking_.model_changed();  // the isolated net follows the new ordinals
  update_section(false);
  highlight_dirty_ = true;
  dirty_ = true;
  refresh_rows();
  stage_paint_->invalidate();
  if (status_paint_) status_paint_->invalidate();
  viewport_->invalidate();
}

void View3dView::refresh_rows() {
  const view3d::WindowState* w = window();
  const view3d::LayerStack* stack = drawn_stack_ ? drawn_stack_.get() : build_stack_.get();
  if (w != nullptr && stack != nullptr) rebuild_rows(*w, *stack);
  const bool editable = w != nullptr && visibility_editable(*w);
  for (std::size_t i = 0; i < std::max(rows_.size(), rows_widgets_.size()); ++i) {
    Row& row = row_widgets(i);
    const bool shown = i < rows_.size();
    row.box.set_visible(shown);
    if (shown) update_row(row, rows_[i], editable);
  }
}

void View3dView::rebuild_rows(const view3d::WindowState& window, const view3d::LayerStack& stack) {
  rows_ = layer_rows(stack, window, options_.alpha, drawable_, ghost_.ghosted());
  std::reverse(rows_.begin(), rows_.end());  // the panel lists the top layer first
  const display::StyleSet* styles = ctx_.editor().styles ? &*ctx_.editor().styles : nullptr;
  const std::vector<render::LayerPaint> paints = render::layer_paints(stack, styles);
  row_colors_.clear();
  for (const View3dLayerRow& r : rows_) {
    const bool known = !r.layers.empty() && r.layers.front() < paints.size();
    row_colors_.push_back(known ? rgb_of(paints[r.layers.front()].rgb) : Rgba{1.0F, 1.0F, 1.0F, 1.0F});
  }
}

void View3dView::update_row(Row& row, const View3dLayerRow& r, bool editable) {
  row.label->set_accessible_name(row_accessible_name(r));
  row.label->set_tooltip(row_tooltip(r));  // the full name and range the row may ellipsize
  row.hide->set_accessible_name((r.visible ? "Hide " : "Show ") + r.name);
  row.isolate->set_accessible_name("Isolate " + r.name);
  row.alpha->set_accessible_name(alpha_button_name(r));
  row.hide->set_enabled(editable);
  row.isolate->set_enabled(editable);
  row.label->invalidate();
  row.hide->invalidate();
  row.alpha->invalidate();
}

void View3dView::repaint() {
  const View3dControlState state = control_state();
  for (Control& c : controls_) {
    const View3dControlView v = control_view(c.id, state);
    c.widget->set_accessible_name(v.accessible_name);
    c.widget->set_enabled(v.enabled);
    c.widget->invalidate();
  }
  const view3d::WindowState* w = window();
  level_paint_->set_accessible_name(level_text(w != nullptr ? w->level : 1));
  level_paint_->invalidate();
  stage_paint_->invalidate();
  if (status_paint_) status_paint_->invalidate();
  section_paint_->invalidate();
}

// --- rows ----------------------------------------------------------------------------

void View3dView::toggle_row(std::size_t index) {
  const view3d::WindowState* w = window();
  if (index >= rows_.size() || w == nullptr || !visibility_editable(*w)) return;
  run(see_command(path(), rows_[index].name, !rows_[index].visible));
}

void View3dView::isolate_row(std::size_t index) {
  const view3d::WindowState* w = window();
  if (index >= rows_.size() || w == nullptr || !visibility_editable(*w)) return;
  for (const std::string& line : isolate_commands(path(), rows_[index].name)) run(line);
}

void View3dView::cycle_alpha(std::size_t index) {
  const view3d::LayerStack* stack = drawn_stack_ ? drawn_stack_.get() : build_stack_.get();
  if (index >= rows_.size() || stack == nullptr) return;
  set_row_alpha(options_, rows_[index], next_alpha(rows_[index].alpha), stack->size());
  refresh_rows();
  dirty_ = true;
  viewport_->invalidate();
}

// --- drawing -------------------------------------------------------------------------

bool View3dView::ensure_renderer() {
  if (renderer_) return true;
  if (!renderer_error_.empty()) return false;
  try {
    render::QueueLock lock{[m = &queue_mutex_] { m->lock(); }, [m = &queue_mutex_] { m->unlock(); }};
    target_ = std::make_unique<render::VulkanOffscreenTarget>(ctx_.runtime.vulkan_device(), std::max(1, pixels_.first),
                                                              std::max(1, pixels_.second), std::move(lock));
    renderer_ = std::make_unique<render::VulkanMesh3dRenderer>(*target_);
  } catch (const std::exception& e) {
    renderer_error_ = std::string{"The 3D renderer could not start: "} + e.what();
    target_.reset();
    return false;
  }
  return true;
}

void View3dView::draw_frame() {
  const view3d::WindowState* w = window();
  const view3d::LayerStack* stack = drawn_stack_ ? drawn_stack_.get() : build_stack_.get();
  const bool shown = active_ || separate_window_ != nullptr;
  if (!shown || w == nullptr || stack == nullptr || pixels_.first <= 0 || !ensure_renderer()) return;
  if (model_ && model_ != uploaded_) {
    renderer_->upload(*model_);
    uploaded_ = model_;
    uploaded_stack_ = drawn_stack_;
  }
  if (highlight_dirty_) {
    update_pick_scope(uploaded_stack_.get());
    highlight_ = picking_.highlight(uploaded_.get(), ctx_.editor());
    highlight_dirty_ = false;
  }
  const display::StyleSet* styles = ctx_.editor().styles ? &*ctx_.editor().styles : nullptr;
  const std::vector<bool> hidden = view3d::hidden_layers(ctx_.editor(), *stack, *w, magic_compat());
  render::DrawParams params =
      draw_params(FrameInputs{w, stack, styles, highlight_, &options_, view3d_background(ctx_.mode), &hidden});
  // The board's substrate slab and the cross-section's clipping plane (round 5).
  if (substrate_on_ && uploaded_ && uploaded_stack_) {
    params.substrate = view3d::substrate_slab(*uploaded_, *uploaded_stack_);
  }
  if (section_line_) params.section_plane = section_clip_plane(segment_in_mesh(*section_line_, mesh_units()));
  const auto start = std::chrono::steady_clock::now();
  try {
    renderer_->draw(w->view, params, *target_);
  } catch (const std::exception& e) {
    renderer_error_ = std::string{"3D frame failed: "} + e.what();
    return;
  }
  timer_.add(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
  const render::ShownImage image = target_->shown();
  shown_viewport().show_vulkan_image(image.view, image.width, image.height);
  dirty_ = false;
  stage_paint_->invalidate();
  if (status_paint_) status_paint_->invalidate();
}

// --- input ---------------------------------------------------------------------------

bool View3dView::handle_input(const CanvasInput& input) {
  if (!window_open()) return false;
  const ui::Rect r = shown_viewport().rect();
  const int w = static_cast<int>(r.width);
  const int h = static_cast<int>(r.height);
  switch (input.kind) {
    case InputKind::pointer_down:
    case InputKind::pointer_move:
    case InputKind::pointer_up:
      return handle_pointer(input);
    case InputKind::wheel:
      if (auto g = wheel_gesture(input.wheel_x, input.wheel_y, input.precise_wheel, input.modifiers, w, h)) {
        apply_gesture(*g);
      }
      return true;
    case InputKind::pinch:
      if (auto g = pinch_gesture(input.scale, input.modifiers)) apply_gesture(*g);
      return true;
    case InputKind::key_down:
      if (auto text = key_macro(ctx_.editor().macros, input.native_key, input.modifiers)) {
        run(window_command(path(), *text));
        return true;
      }
      return false;
    default:
      return false;
  }
}

bool View3dView::handle_pointer(const CanvasInput& input) {
  const auto x = static_cast<float>(input.x);
  const auto y = static_cast<float>(input.y);
  if (input.kind == InputKind::pointer_down) {
    shown_viewport().focus();
    pointer_.press(x, y, input.button, input.modifiers);
    return true;
  }
  if (input.kind == InputKind::pointer_move) {
    const ui::Rect r = shown_viewport().rect();
    if (auto g = pointer_.move(x, y, static_cast<int>(r.width), static_cast<int>(r.height))) apply_gesture(*g);
    return pointer_.pressed();
  }
  if (auto click = pointer_.release(x, y)) {
    if (!section_click(click->first, click->second)) pick_at(click->first, click->second);
  }
  return true;
}

void View3dView::apply_gesture(const view3d::Gesture& gesture) {
  std::optional<view3d::WindowState>& w = ctx_.editor().view3d.window;
  if (!w) return;
  view3d::apply(w->view, gesture);  // as the matching `view`/`scroll`/`zoom` would
  dirty_ = true;
  shown_viewport().invalidate();
}

void View3dView::pick_at(float x, float y) {
  // The id was drawn from the uploaded model; a newer model_ waiting for
  // the next frame has other ordinals.
  const view3d::LayerStack* stack = uploaded_stack_.get();
  if (!renderer_ || stack == nullptr) return;
  const float ratio = shown_ratio();
  const view3d::PickId id = renderer_->read_pick(static_cast<int>(x * ratio), static_cast<int>(y * ratio));
  update_pick_scope(stack);
  const std::optional<std::string> caption = picking_.click(uploaded_.get(), id, ctx_.editor(), *stack);
  pick_caption_ = caption.value_or(std::string{});
  highlight_dirty_ = true;
  dirty_ = true;
  repaint();
  if (picking_.selected_in_2d() && selection_changed_) selection_changed_();
}

void View3dView::update_pick_scope(const view3d::LayerStack* stack) {
  const view3d::WindowState* w = window();
  db::CellDef* cell = w != nullptr ? ctx_.editor().cells.find(w->cell) : nullptr;
  resolver_.set_scope(view3d::PickContext{stack, cell, magic_compat()});
}

// --- camera (Q-MISC-84) --------------------------------------------------------------

CameraFrame View3dView::camera_frame() const {
  return CameraFrame{window(), model_.get(), drawn_stack_.get(), drawable_, pixels_.first, pixels_.second};
}

void View3dView::apply_camera(view3d::Preset which, std::optional<float> scale_z) {
  for (const std::string& line : preset_camera_commands(camera_frame(), which, scale_z)) run(line);
}

void View3dView::apply_default_camera() {
  // Waits for the first model, so the camera frames what is drawn.
  const CameraFrame frame = camera_frame();
  if (!frame_ready(frame)) return;
  if (camera_latch_.take(window_open())) {
    for (const std::string& line : default_camera_commands(frame)) run(line);
  }
}

// --- painting ------------------------------------------------------------------------

void View3dView::paint_stage(ui::Painter& p, float w, float h) {
  const ThemeTokens& t = ctx_.tokens();
  const view3d::WindowState* win = window();
  if (win == nullptr) {
    p.text_centered_y(TextRole::body, 24.0F, h / 2.0F, "No 3D window: Options > 3D Display or render3d opens one",
                      t.text_secondary);
    return;
  }
  const View3dHeader header = view3d_header(*win);
  stage_paint_->set_accessible_name(header.title + ", " + header.subtitle);
  if (separate_window_) {
    p.text_centered_y(TextRole::body, 24.0F, h / 2.0F,
                      "Shown in a separate window: close it, or turn off Separate window, to bring it back",
                      t.text_secondary);
  }
  p.fill(8.0F, 8.0F, std::max(p.text_width(TextRole::cell_subtitle, header.subtitle),
                              p.text_width(TextRole::cell_title, header.title)) + 20.0F,
         42.0F, 6.0F, alpha_of(t.surface0, 0.85F));
  p.text(TextRole::cell_title, 18.0F, 13.0F, header.title, t.text_primary);
  p.text(TextRole::cell_subtitle, 18.0F, 31.0F, header.subtitle, t.text_secondary);
  // The gizmo and the pick caption sit above the floating toolbar.
  const float bottom = h - toolbar_cover();
  paint_gizmo(p, bottom);
  std::string caption = pick_caption_;
  if (caption.empty() && section_line_ && section_first_) caption = "A placed · click A′";
  if (!caption.empty()) {
    const float cw = p.text_width(TextRole::body, caption) + 20.0F;
    p.fill(w - cw - 8.0F, bottom - 34.0F, cw, 26.0F, 6.0F, alpha_of(t.surface0, 0.85F));
    p.text_centered_y(TextRole::body, w - cw + 2.0F, bottom - 21.0F, caption, t.text_primary);
  }
}

std::string View3dView::status_line() const {
  if (!renderer_error_.empty()) return renderer_error_;
  MeshStatus status;
  {
    const std::lock_guard lock(shared_->mutex);
    status.progress = shared_->progress;
  }
  status.building = !worker_->idle();
  status.have_model = model_ != nullptr;
  status.rectangles = model_ ? rectangle_count(*model_) : 0;
  std::string line = mesh_status_text(status);
  if (options_.show_frame_time) line += " · frame " + timer_.text();
  return line;
}

void View3dView::paint_status(ui::Painter& p, float w, float h) {
  const ThemeTokens& t = ctx_.tokens();
  const std::string line = status_line();
  const bool building = !worker_->idle();
  status_paint_->set_accessible_name(line);
  const float tw = p.text_width(TextRole::mono_small, line);
  const float x = std::max(0.0F, w - tw - 30.0F);
  p.fill(x, 0.0F, tw + 30.0F, h, 8.0F, t.surface1);
  const Rgba dot = !renderer_error_.empty() ? t.danger : (building ? t.warning : t.success);
  p.fill(x + 10.0F, h / 2.0F - 4.0F, 8.0F, 8.0F, 4.0F, dot);
  p.text_centered_y(TextRole::mono_small, x + 24.0F, h / 2.0F, line, t.text_secondary);
}

// The x/y/z axis gizmo at the bottom left of the view (board colours).
void View3dView::paint_gizmo(ui::Painter& p, float h) {
  const view3d::WindowState* win = window();
  if (win == nullptr) return;
  constexpr float kLength = 40.0F;
  const float ox = 48.0F;
  const float oy = h - 48.0F;
  const Rgba colors[3] = {Rgba::hex(0xFF6B6B), Rgba::hex(0x5FD3A0), Rgba::hex(0x5AA9F0)};
  const std::array<AxisArrow, 3> arrows = axis_gizmo(win->view);
  for (std::size_t i = 0; i < arrows.size(); ++i) {
    const float x = ox + arrows[i].dx * kLength;
    const float y = oy + arrows[i].dy * kLength;
    p.line(ox, oy, x, y, 2.0F, colors[i]);
    const float label_x = x + (arrows[i].dx >= 0.0F ? 4.0F : -12.0F);
    p.text_centered_y(TextRole::mono_small, label_x, y, std::string(1, arrows[i].name),
                      ctx_.tokens().text_secondary);
  }
}

void View3dView::paint_row(ui::Painter& p, float w, float h, std::size_t index, bool /*hovered*/, bool /*focused*/) {
  if (index >= rows_.size()) return;
  const ThemeTokens& t = ctx_.tokens();
  const View3dLayerRow& row = rows_[index];
  const Rgba swatch = index < row_colors_.size() ? row_colors_[index] : Rgba{1.0F, 1.0F, 1.0F, 1.0F};
  const Rgba text = row.visible ? t.text_primary : t.text_tertiary;
  p.fill(2.0F, (h - 12.0F) / 2.0F, 12.0F, 12.0F, 2.0F, alpha_of(swatch, row.visible ? row.alpha : 0.25F));
  // Long names and ranges end in "…" before the row's buttons (the full
  // text is the row's tooltip and screen-reader name).
  const float room = std::max(0.0F, w - kView3dRowTextX - 4.0F);
  const auto measure = [&p](TextRole role) {
    return TextMeasure{[&p, role](std::string_view s) { return p.text_width(role, s); }};
  };
  p.push_clip(0.0F, 0.0F, w, h);
  p.text_centered_y(TextRole::body, kView3dRowTextX, row.range_um.empty() ? h / 2.0F : h * 0.32F,
                    ellipsize(row.name, room, measure(TextRole::body)), text);
  if (!row.range_um.empty()) {
    p.text_centered_y(TextRole::mono_small, kView3dRowTextX, h * 0.74F,
                      ellipsize(row.range_um + " \u00b5m", room, measure(TextRole::mono_small)), t.text_secondary);
  }
  p.pop_clip();
}

void View3dView::paint_level(ui::Painter& p, float w, float h) {
  const ThemeTokens& t = ctx_.tokens();
  const view3d::WindowState* win = window();
  p.fill(0.0F, 0.0F, w, h, 6.0F, t.surface1);
  const std::string text = level_text(win != nullptr ? win->level : 1);
  p.text_centered_y(TextRole::body, (w - p.text_width(TextRole::body, text)) / 2.0F, h / 2.0F, text, t.text_primary);
}

// The board's "Cross-section A–A′" panel: each layer's intervals from h to
// h + t in µm along the line, in the display style's colour.
void View3dView::paint_section(ui::Painter& p, float w, float h) {
  const ThemeTokens& t = ctx_.tokens();
  const view3d::LayerStack* stack = drawn_stack_.get();
  const view3d::WindowState* win = window();
  if (!section_line_ || stack == nullptr || win == nullptr) return;
  constexpr float kTop = 40.0F;
  constexpr float kStrip = 64.0F;
  p.fill(0.0F, 0.0F, w, h, 8.0F, t.surface2);
  p.text(TextRole::body_strong, 12.0F, 8.0F, "Cross-section A–A′", t.text_primary);
  const SectionPanel panel =
      section_panel(section_, *stack, section_.length_um, w - 24.0F, kStrip,
                    view3d::hidden_layers(ctx_.editor(), *stack, *win, magic_compat()));
  section_paint_->set_accessible_name(panel.accessible_name);
  p.text(TextRole::meta, 12.0F, 24.0F, panel.summary, t.text_secondary);
  p.fill(12.0F, kTop, w - 24.0F, kStrip, 2.0F, t.surface0);
  const display::StyleSet* styles = ctx_.editor().styles ? &*ctx_.editor().styles : nullptr;
  const std::vector<render::LayerPaint> paints = render::layer_paints(*stack, styles);
  for (const SectionBar& bar : panel.bars) {
    // In the style colour at the layer's transparency (ghosted wells stay faint).
    const float alpha = bar.layer < options_.alpha.size() ? options_.alpha[bar.layer] : 1.0F;
    const Rgba color = bar.layer < paints.size() ? alpha_of(rgb_of(paints[bar.layer].rgb), alpha) : t.text_secondary;
    p.fill(12.0F + bar.x, kTop + bar.y, std::max(1.0F, bar.w), std::max(1.0F, bar.h), 0.0F, color);
  }
  std::string foot = panel.bars.empty() ? "No layer crosses the line" : "A at left, A′ at right; height in µm";
  if (!panel.no_height.empty()) foot = "No height: " + panel.no_height.front() + (panel.no_height.size() > 1 ? " …" : "");
  p.text_centered_y(TextRole::meta, 12.0F, kTop + kStrip + 14.0F, foot, t.text_secondary);
}

// --- P2 actions (task 11.5) ----------------------------------------------------------

void View3dView::toggle_section() {
  section_first_.reset();
  if (section_line_) {
    section_line_.reset();
  } else if (const view3d::WindowState* w = window()) {
    if (const std::optional<db::Rect> area = section_area(ctx_.editor(), *w)) section_line_ = box_cut_line(*area);
  }
  section_paint_->set_visible(section_line_.has_value());
  update_section(true);
  dirty_ = true;  // the clipping plane
  repaint();
}

void View3dView::update_section(bool report) {
  section_ = {};
  const view3d::WindowState* w = window();
  if (section_line_ && model_ && drawn_stack_ && w != nullptr) {
    section_ = view3d::section(*model_, *drawn_stack_, segment_in_mesh(*section_line_, model_->units_per_root),
                               view3d::hidden_layers(ctx_.editor(), *drawn_stack_, *w, magic_compat()));
  }
  // A new line's diagnostics (a refused line, layers without a height) go
  // to the console once; a rebuilt model recomputes silently.
  if (report) {
    for (const diag::Diagnostic& d : section_.diagnostics) ctx_.core.dispatcher().report(d);
  }
  section_paint_->invalidate();
}

bool View3dView::section_click(float x, float y) {
  const view3d::WindowState* w = window();
  if (!section_line_ || w == nullptr) return false;
  const float ratio = shown_ratio();
  const render::DrawParams params = draw_params(FrameInputs{w, nullptr, nullptr, nullptr, &options_});
  const std::optional<db::Point> ground =
      view3d::ground_point(w->view, params.projection, pixels_.first, pixels_.second, x * ratio, y * ratio);
  if (!ground) return false;  // the plane is seen edge on (front, side): the click picks
  const db::Point point = ground_to_root(*ground, static_cast<float>(mesh_units()));
  if (!section_first_) {
    section_first_ = point;
  } else {
    section_line_ = axis_aligned_line(*section_first_, point);
    section_first_.reset();
    update_section(true);
    dirty_ = true;
  }
  repaint();
  return true;
}

bool View3dView::export_registered() const {
  const cmd::CommandRegistry& registry = ctx_.core.registry();
  return registry.find("export3d", view3d::kWind3dClient) != nullptr || registry.find("export3d") != nullptr;
}

void View3dView::export_model() {
  std::vector<ui::MenuItem> items;
  items.reserve(kView3dExportFormats.size());
  for (const View3dExportFormat format : kView3dExportFormats) items.push_back({export_format_label(format)});
  export_menu_.set_items("Export 3D model", items, [this](std::size_t index) {
    if (index < kView3dExportFormats.size()) export_as(kView3dExportFormats[index]);
  });
  const ui::Painted* button = control_widget(View3dControl::export_model);
  // Without a window to open the menu in, the default format (glTF).
  if (button == nullptr || !export_menu_.open_below(*button)) export_as(View3dExportFormat::gltf);
}

void View3dView::export_as(View3dExportFormat format) {
  const view3d::WindowState* w = window();
  platform::FilePicker* picker = ctx_.editor().file_picker;
  if (w == nullptr || picker == nullptr) return;
  const std::optional<platform::Path> file = picker->pick(export_request(*w, format));
  if (!file) return;
  const bool on_window = ctx_.core.registry().find("export3d", view3d::kWind3dClient) != nullptr;
  run(export_command(path(), file->string(), on_window, format));
}

ui::Painted* View3dView::control_widget(View3dControl id) const {
  for (const Control& c : controls_) {
    if (c.id == id) return c.widget.get();
  }
  return nullptr;
}

// --- separate window (round 6) -------------------------------------------------------

void View3dView::connect_viewport(ui::Viewport& viewport, bool separate) {
  viewport.on_resize([this, separate](int width, int height) {
    // Only the viewport that shows the frames sizes the target.
    if (separate != (separate_window_ != nullptr)) return;
    pixels_ = {width, height};
    if (target_) target_->resize(width, height);
    dirty_ = true;
  });
  viewport.on_input([this](const CanvasInput& input) { return handle_input(input); });
}

ui::Viewport& View3dView::shown_viewport() const {
  return separate_viewport_ ? *separate_viewport_ : *viewport_;
}

float View3dView::shown_ratio() const {
  if (separate_window_) return separate_window_->scale();
  return pixel_ratio_ ? pixel_ratio_() : 1.0F;
}

void View3dView::toggle_separate_window() {
  if (separate_window_) {
    close_separate_window();
  } else {
    open_separate_window();
  }
}

void View3dView::open_separate_window() {
  const view3d::WindowState* w = window();
  if (w == nullptr || separate_window_) return;
  separate_viewport_ = ui::Viewport::create();
  separate_viewport_->set_accessible_name("3D view");
  separate_viewport_->set_tooltip(kViewportTooltip);
  separate_window_ = std::make_unique<ui::Window>(view3d_header(*w).title, kSeparateWidth, kSeparateHeight);
  separate_window_->set_min_size(320, 240);
  separate_window_->set_clear_color(ctx_.tokens().canvas);
  separate_window_->on_close_request([this] { separate_close_requested_ = true; });
  connect_viewport(*separate_viewport_, true);
  separate_window_->set_content(*separate_viewport_);
  viewport_->show_vulkan_image(0, 0, 0);  // the tab shows a note meanwhile
  separate_viewport_->focus();
  dirty_ = true;
  repaint();
}

void View3dView::close_separate_window() {
  separate_close_requested_ = false;
  if (!separate_window_) return;
  separate_viewport_->show_vulkan_image(0, 0, 0);
  separate_window_->destroy();  // returns once Raven no longer renders it
  separate_window_.reset();
  separate_viewport_.reset();
  pixels_ = viewport_->pixel_size();
  if (target_ && pixels_.first > 0 && pixels_.second > 0) target_->resize(pixels_.first, pixels_.second);
  dirty_ = true;
  if (!controls_.empty()) repaint();
}

}  // namespace hefesto::shell
