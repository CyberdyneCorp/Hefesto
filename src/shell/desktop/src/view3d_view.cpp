// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The desktop's 3D view (view3d_view.hpp). Owner: SHELL.

#include "view3d_view.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/render/mesh3d_shading.hpp"
#include "hefesto/script/binding/window_commands.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/view3d/camera/view_state.hpp"
#include "hefesto/view3d/client/commands.hpp"
#include "hefesto/view3d/client/scene.hpp"
#include "screen_paint.hpp"
#include "side_panels.hpp"

namespace hefesto::shell {

namespace {

constexpr float kSideWidth = 272.0F;
constexpr float kRowHeight = 36.0F;
constexpr float kRowButtonWidth = 44.0F;
constexpr float kControlHeight = 28.0F;
constexpr float kPad = 12.0F;

Rgba rgb_of(const std::array<float, 3>& c) {
  return Rgba{c[0], c[1], c[2], 1.0F};
}

Rgba alpha_of(Rgba c, float a) {
  c.a *= a;
  return c;
}

std::string toggle_name(const std::string& label, bool on) {
  return label + (on ? ", on" : ", off");
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
  viewport_->set_accessible_name("3D view");
  viewport_->set_tooltip(
      "3D view: drag orbits; Option-drag, right-drag or two-finger scroll pans; wheel or pinch zooms; click picks; "
      "keys run the 3D window's bindings (L/l level, arrows scroll, Z/z zoom, space defaults)");
  stage_.add(*viewport_);
  stage_.add(*stage_paint_);
  stage_.set_accessible_name("3D view");
  viewport_->on_resize([this](int width, int height) {
    pixels_ = {width, height};
    if (target_) target_->resize(width, height);
    dirty_ = true;
  });
  viewport_->on_input([this](const CanvasInput& input) { return handle_input(input); });
  root_.add(stage_, 1.0F);
}

void View3dView::build_side() {
  auto edge = make_separator(ctx_, true);
  root_.add(*edge);
  painted_.push_back(std::move(edge));
  side_panel_.set_fill(ctx_.tokens().surface1);
  side_panel_.set_size(kSideWidth, -1.0F);
  side_panel_.set_accessible_name("3D view controls");
  side_.set_padding(kPad, kPad, kPad, kPad);
  side_.set_spacing(6.0F);
  build_layer_section(side_);
  build_view_section(side_);
  build_parity_section(side_);
  build_inspect_section(side_);
  side_scroll_.add(side_);
  side_panel_.set_child(side_scroll_);
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

ui::Painted& View3dView::add_button(ui::Box& box, Control control, std::function<void()> click,
                                    std::string_view tooltip, float width) {
  const std::function<std::string()> label = control.label;
  const std::function<bool()> on = control.on;
  const std::function<bool()> enabled = control.enabled;
  control.widget = make_interactive(
      ctx_, [label, on, enabled](ui::Painter& p, float w, float h, const ThemeTokens& t, bool hovered, bool focused) {
        const bool active = on && on();
        const bool usable = !enabled || enabled();
        paint::button(p, w, h, label(), "", active ? paint::ButtonStyle::primary : paint::ButtonStyle::secondary,
                      paint::WidgetState{hovered, focused, usable, active}, t);
      });
  ui::Painted& widget = *control.widget;
  widget.set_size(width > 0.0F ? width : paint::button_width(ctx_.runtime, label(), false), kControlHeight);
  widget.set_role(on ? ui::Role::toggle_button : ui::Role::button);
  widget.set_accessible_name(on ? toggle_name(label(), on()) : label());
  widget.set_tooltip(tooltip);
  widget.on_click([click, enabled](float, float) {
    if (!enabled || enabled()) click();
  });
  box.add(widget);
  controls_.push_back(std::move(control));
  return widget;
}

void View3dView::build_layer_section(ui::Box& box) {
  add_section(box, "Layer stack");
  rows_box_.set_spacing(2.0F);
  rows_box_.set_accessible_name("Layer stack, top layer first");
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
  add_button(line, {nullptr, [] { return std::string{"Show all layers"}; }, nullptr,
                    [this] { return window() != nullptr && visibility_editable(*window()); }},
             [this] { run(show_all_command(path())); }, "Show every layer: see *");
  box.add(line);
}

void View3dView::build_view_section(ui::Box& box) {
  add_section(box, "View");
  ui::Box& projection = boxes_.emplace_back(ui::Box::horizontal());
  projection.set_spacing(4.0F);
  add_button(projection, {nullptr, [] { return std::string{"Orthographic"}; }, [this] { return !options_.perspective; }, {}},
             [this] {
               options_.perspective = false;
               dirty_ = true;
               repaint();
             },
             "Orthographic projection (Magic's)");
  add_button(projection, {nullptr, [] { return std::string{"Perspective"}; }, [this] { return options_.perspective; }, {}},
             [this] {
               options_.perspective = true;
               dirty_ = true;
               repaint();
             },
             "Perspective projection (a Hefesto view option)");
  box.add(projection);
  ui::Box& presets = boxes_.emplace_back(ui::Box::horizontal());
  presets.set_spacing(4.0F);
  const std::pair<const char*, view3d::Preset> kPresets[] = {{"Isometric", view3d::Preset::isometric},
                                                             {"Top", view3d::Preset::top},
                                                             {"Front", view3d::Preset::front},
                                                             {"Side", view3d::Preset::side}};
  for (const auto& entry : kPresets) {
    const std::string label = entry.first;
    const view3d::Preset which = entry.second;
    add_button(presets, {nullptr, [label] { return label; }, nullptr, [this] { return window() != nullptr; }},
               [this, which] { run(preset_command(path(), which)); },
               "Camera preset " + label + ": view with the preset's angles");
  }
  box.add(presets);
}

void View3dView::build_parity_section(ui::Box& box) {
  add_section(box, "Magic 3D");
  ui::Box& mode = boxes_.emplace_back(ui::Box::horizontal());
  mode.set_spacing(4.0F);
  const auto is_mask = [this] { return window() != nullptr && window()->mode == view3d::Mode::mask_layer; };
  add_button(mode, {nullptr, [] { return std::string{"Mask layers"}; }, is_mask, {}},
             [this, is_mask] {
               if (!is_mask()) run(mode_command(path()));
             },
             "Mask-layer mode (cif); mask layers arrive in phase 2");
  add_button(mode, {nullptr, [] { return std::string{"Layout layers"}; }, [is_mask] { return !is_mask(); }, {}},
             [this, is_mask] {
               if (is_mask()) run(mode_command(path()));
             },
             "Layout-layer mode (cif)");
  box.add(mode);
  ui::Box& level = boxes_.emplace_back(ui::Box::horizontal());
  level.set_spacing(4.0F);
  add_button(level, {nullptr, [] { return std::string{"−"}; }, nullptr, {}}, [this] { run(level_command(path(), -1)); },
             "Level down (level down)", 32.0F)
      .set_accessible_name("Level down");
  add_button(level,
             {nullptr, [this] { return "Level " + std::to_string(window() != nullptr ? window()->level : 1); }, nullptr,
              {}},
             [] {}, "Rendering level (level): MSAA at level 1 and above", 88.0F);
  add_button(level, {nullptr, [] { return std::string{"+"}; }, nullptr, {}}, [this] { run(level_command(path(), 1)); },
             "Level up (level up)", 32.0F)
      .set_accessible_name("Level up");
  box.add(level);
  ui::Box& cut = boxes_.emplace_back(ui::Box::horizontal());
  add_button(cut,
             {nullptr, [] { return std::string{"Cut box"}; },
              [this] { return window() != nullptr && window()->cut_box.has_value(); }, {}},
             [this] { run(cutbox_command(path(), !(window() != nullptr && window()->cut_box.has_value()))); },
             "Clip to the layout box: cutbox box / cutbox none");
  box.add(cut);
}

void View3dView::build_inspect_section(ui::Box& box) {
  add_section(box, "Inspect");
  ui::Box& net = boxes_.emplace_back(ui::Box::horizontal());
  add_button(net,
             {nullptr, [] { return std::string{"Isolate selected net"}; }, [this] { return picking_.net_isolated(); },
              [this] { return picking_.picked().has_value() || picking_.net_isolated(); }},
             [this] {
               if (picking_.toggle_isolate_net(model_.get(), ctx_.editor())) {
                 highlight_dirty_ = true;
                 dirty_ = true;
                 repaint();
               }
             },
             "Highlight the picked target's net and dim the rest");
  box.add(net);
  ui::Box& span = boxes_.emplace_back(ui::Box::horizontal());
  add_button(span,
             {nullptr, [] { return std::string{"Span connected layers"}; },
              [this] { return options_.span_connected_layers; }, {}},
             [this] {
               options_.span_connected_layers = !options_.span_connected_layers;
               sync_pending_ = true;  // a full build with the J1 option
               repaint();
             },
             "Off: contacts and vias use their tech height and thickness, as Magic. On: each spans from the lower "
             "layer's top to the upper layer's bottom.");
  box.add(span);
  ui::Box& tools = boxes_.emplace_back(ui::Box::horizontal());
  tools.set_spacing(4.0F);
  add_button(tools, {nullptr, [] { return std::string{"Frame time"}; }, [this] { return options_.show_frame_time; }, {}},
             [this] {
               options_.show_frame_time = !options_.show_frame_time;
               dirty_ = true;
               repaint();
             },
             "Show the 3D frame's draw time and frame rate");
  add_button(tools, {nullptr, [] { return std::string{"Close 3D view"}; }, nullptr, [this] { return window() != nullptr; }},
             [this] { run(close_command(path())); }, "Close the 3D window: closewindow");
  box.add(tools);
}

View3dView::Row& View3dView::row_widgets(std::size_t index) {
  while (rows_widgets_.size() <= index) {
    const std::size_t i = rows_widgets_.size();
    auto row = std::make_unique<Row>();
    row->box.set_spacing(2.0F);
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
      w->set_size(kRowButtonWidth, kRowHeight);
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
  if (!open) return;
  if (sync_pending_) sync();
  if (dirty_) draw_frame();
}

void View3dView::reset_window_state() {
  worker_->cancel();
  model_.reset();
  drawn_stack_.reset();
  build_stack_.reset();
  have_key_ = false;
  picking_.reset();
  highlight_ = nullptr;
  pick_caption_.clear();
  rows_.clear();
  refresh_rows();
  stage_paint_->invalidate();
}

void View3dView::sync() {
  sync_pending_ = false;
  const view3d::WindowState* w = window();
  if (w == nullptr || !ctx_.editor().technology) return;
  view3d::LayerStack stack = view3d::layout_stack(ctx_.editor(), magic_compat());
  const BuildKey key = build_key(ctx_.editor(), *w, stack, options_.span_connected_layers);
  if (!have_key_ || !(key == key_)) start_build(key, stack);
  refresh_rows();
  repaint();
  dirty_ = true;
  viewport_->invalidate();
}

void View3dView::start_build(const BuildKey& key, const view3d::LayerStack& stack) {
  key_ = key;
  have_key_ = true;
  build_stack_ = std::make_shared<const view3d::LayerStack>(stack);
  const display::StyleSet* styles = ctx_.editor().styles ? &*ctx_.editor().styles : nullptr;
  drawable_.clear();
  for (const render::LayerPaint& paint : render::layer_paints(*build_stack_, styles)) drawable_.push_back(paint.drawable);
  if (key.cell == nullptr) {
    worker_->cancel();
    return;
  }
  held_tech_.push_back(ctx_.editor().technology);
  {
    const std::lock_guard lock(shared_->mutex);
    shared_->progress = 0.0;
  }
  worker_->build(build_inputs(ctx_.editor(), *window(), build_stack_, options_.span_connected_layers));
}

void View3dView::take_model(const view3d::MeshHandle& model) {
  if (worker_->idle() && held_tech_.size() > 1) held_tech_.erase(held_tech_.begin(), held_tech_.end() - 1);
  if (!model || !build_stack_ || !window_open()) return;
  // A model of an earlier stack (a build finishing as a new one started):
  // its layer ids do not match the current stack; the next one replaces it.
  if (!model->layer_heights.empty() && model->layer_heights.size() != build_stack_->size()) return;
  model_ = model;
  drawn_stack_ = build_stack_;
  highlight_dirty_ = true;
  dirty_ = true;
  refresh_rows();
  stage_paint_->invalidate();
  viewport_->invalidate();
}

void View3dView::refresh_rows() {
  const view3d::WindowState* w = window();
  const view3d::LayerStack* stack = drawn_stack_ ? drawn_stack_.get() : build_stack_.get();
  if (w != nullptr && stack != nullptr) {
    rows_ = layer_rows(*stack, *w, options_.alpha, drawable_);
    std::reverse(rows_.begin(), rows_.end());  // the panel lists the top layer first
    const display::StyleSet* styles = ctx_.editor().styles ? &*ctx_.editor().styles : nullptr;
    const std::vector<render::LayerPaint> paints = render::layer_paints(*stack, styles);
    row_colors_.clear();
    for (const View3dLayerRow& r : rows_) {
      const bool known = !r.layers.empty() && r.layers.front() < paints.size();
      row_colors_.push_back(known ? rgb_of(paints[r.layers.front()].rgb) : Rgba{1.0F, 1.0F, 1.0F, 1.0F});
    }
  }
  for (std::size_t i = 0; i < std::max(rows_.size(), rows_widgets_.size()); ++i) {
    Row& row = row_widgets(i);
    const bool shown = i < rows_.size();
    row.box.set_visible(shown);
    if (!shown) continue;
    const View3dLayerRow& r = rows_[i];
    row.label->set_accessible_name(r.name + (r.range_um.empty() ? "" : ", " + r.range_um + " µm") +
                                   (r.visible ? ", shown" : ", hidden"));
    row.hide->set_accessible_name((r.visible ? "Hide " : "Show ") + r.name);
    row.isolate->set_accessible_name("Isolate " + r.name);
    row.alpha->set_accessible_name("Transparency of " + r.name + ", " +
                                   std::to_string(static_cast<int>(r.alpha * 100.0F + 0.5F)) + " percent");
    const bool editable = visibility_editable(*w);
    row.hide->set_enabled(editable);
    row.isolate->set_enabled(editable);
    row.label->invalidate();
    row.hide->invalidate();
    row.alpha->invalidate();
  }
}

void View3dView::repaint() {
  for (Control& c : controls_) {
    if (c.on) c.widget->set_accessible_name(toggle_name(c.label(), c.on()));
    c.widget->invalidate();
  }
  stage_paint_->invalidate();
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
  if (!active_ || w == nullptr || stack == nullptr || pixels_.first <= 0 || !ensure_renderer()) return;
  if (model_ && model_ != uploaded_) {
    renderer_->upload(*model_);
    uploaded_ = model_;
  }
  if (highlight_dirty_) {
    highlight_ = picking_.highlight(model_.get(), ctx_.editor());
    highlight_dirty_ = false;
  }
  const display::StyleSet* styles = ctx_.editor().styles ? &*ctx_.editor().styles : nullptr;
  const render::DrawParams params = draw_params(FrameInputs{w, stack, styles, highlight_, &options_});
  const auto start = std::chrono::steady_clock::now();
  try {
    renderer_->draw(w->view, params, *target_);
  } catch (const std::exception& e) {
    renderer_error_ = std::string{"3D frame failed: "} + e.what();
    return;
  }
  timer_.add(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
  const render::ShownImage shown = target_->shown();
  viewport_->show_vulkan_image(shown.view, shown.width, shown.height);
  dirty_ = false;
  stage_paint_->invalidate();
}

// --- input ---------------------------------------------------------------------------

bool View3dView::handle_input(const CanvasInput& input) {
  if (!window_open()) return false;
  const ui::Rect r = viewport_->rect();
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
    viewport_->focus();
    pointer_.press(x, y, input.button, input.modifiers);
    return true;
  }
  if (input.kind == InputKind::pointer_move) {
    const ui::Rect r = viewport_->rect();
    if (auto g = pointer_.move(x, y, static_cast<int>(r.width), static_cast<int>(r.height))) apply_gesture(*g);
    return pointer_.pressed();
  }
  if (auto click = pointer_.release(x, y)) pick_at(click->first, click->second);
  return true;
}

void View3dView::apply_gesture(const view3d::Gesture& gesture) {
  std::optional<view3d::WindowState>& w = ctx_.editor().view3d.window;
  if (!w) return;
  view3d::apply(w->view, gesture);  // as the matching `view`/`scroll`/`zoom` would
  dirty_ = true;
  viewport_->invalidate();
}

void View3dView::pick_at(float x, float y) {
  const view3d::LayerStack* stack = drawn_stack_.get();
  if (!renderer_ || stack == nullptr) return;
  const float ratio = pixel_ratio_ ? pixel_ratio_() : 1.0F;
  const view3d::PickId id = renderer_->read_pick(static_cast<int>(x * ratio), static_cast<int>(y * ratio));
  const std::optional<std::string> caption = picking_.click(model_.get(), id, ctx_.editor(), *stack);
  pick_caption_ = caption.value_or(std::string{});
  highlight_dirty_ = true;
  dirty_ = true;
  repaint();
  if (caption && selection_changed_) selection_changed_();
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
  p.fill(8.0F, 8.0F, std::max(p.text_width(TextRole::cell_subtitle, header.subtitle),
                              p.text_width(TextRole::cell_title, header.title)) + 20.0F,
         42.0F, 6.0F, alpha_of(t.surface0, 0.85F));
  p.text(TextRole::cell_title, 18.0F, 13.0F, header.title, t.text_primary);
  p.text(TextRole::cell_subtitle, 18.0F, 31.0F, header.subtitle, t.text_secondary);
  if (const auto notice = mask_mode_notice(*win)) {
    const float nw = p.text_width(TextRole::body_strong, *notice) + 24.0F;
    p.fill((w - nw) / 2.0F, 12.0F, nw, 30.0F, 6.0F, t.surface0);
    p.fill((w - nw) / 2.0F, 12.0F, nw, 30.0F, 6.0F, alpha_of(t.warning, 0.2F));
    p.text_centered_y(TextRole::body_strong, (w - nw) / 2.0F + 12.0F, 27.0F, *notice, t.warning_text);
  }
  MeshStatus status;
  {
    const std::lock_guard lock(shared_->mutex);
    status.progress = shared_->progress;
  }
  status.building = !worker_->idle();
  status.have_model = model_ != nullptr;
  status.rectangles = model_ ? rectangle_count(*model_) : 0;
  std::string line = mesh_status_text(status);
  if (options_.show_frame_time) line += "   ·   frame " + timer_.text();
  if (!renderer_error_.empty()) line = renderer_error_;
  p.fill(8.0F, h - 34.0F, p.text_width(TextRole::mono_small, line) + 20.0F, 26.0F, 6.0F, alpha_of(t.surface0, 0.85F));
  p.text_centered_y(TextRole::mono_small, 18.0F, h - 21.0F, line, t.text_primary);
  if (!pick_caption_.empty()) {
    const float cw = p.text_width(TextRole::body, pick_caption_) + 20.0F;
    p.fill(w - cw - 8.0F, h - 34.0F, cw, 26.0F, 6.0F, alpha_of(t.surface0, 0.85F));
    p.text_centered_y(TextRole::body, w - cw + 2.0F, h - 21.0F, pick_caption_, t.text_primary);
  }
}

void View3dView::paint_row(ui::Painter& p, float w, float h, std::size_t index, bool /*hovered*/, bool /*focused*/) {
  if (index >= rows_.size()) return;
  const ThemeTokens& t = ctx_.tokens();
  const View3dLayerRow& row = rows_[index];
  const Rgba swatch = index < row_colors_.size() ? row_colors_[index] : Rgba{1.0F, 1.0F, 1.0F, 1.0F};
  const Rgba text = row.visible ? t.text_primary : t.text_tertiary;
  p.fill(2.0F, (h - 12.0F) / 2.0F, 12.0F, 12.0F, 2.0F, alpha_of(swatch, row.visible ? row.alpha : 0.25F));
  p.push_clip(0.0F, 0.0F, w, h);
  p.text_centered_y(TextRole::body, 22.0F, row.range_um.empty() ? h / 2.0F : h * 0.32F, row.name, text);
  if (!row.range_um.empty()) {
    p.text_centered_y(TextRole::mono_small, 22.0F, h * 0.74F, row.range_um + " \u00b5m", t.text_secondary);
  }
  p.pop_clip();
}

}  // namespace hefesto::shell
