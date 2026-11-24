// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The 3D view's view models (hefesto/shell/view3d_models.hpp). Owner: SHELL.

#include "hefesto/shell/view3d_models.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>
#include <numeric>
#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/shell/input_router.hpp"
#include "hefesto/ui/input.hpp"
#include "hefesto/view/window.hpp"
#include "hefesto/view3d/camera/input.hpp"

namespace hefesto::shell {

namespace {

constexpr float kWheelZoomStep = 1.15F;
constexpr double kTrackpadLinePixels = 48.0;

bool type_hidden(const view3d::WindowState& window, const std::optional<tech::TileType>& type) {
  return window.mode == view3d::Mode::layout_layer && type && window.visibility.layout_hidden.test(*type);
}

View3dLayerRow new_row(const view3d::LayerStack& stack, view3d::LayerId id, const view3d::WindowState& window) {
  const view3d::StackLayer& layer = stack.layers()[id];
  View3dLayerRow row;
  row.name = layer.name;
  row.type = layer.type;
  row.style = layer.style;
  row.range_um = micron_range(stack.physical_height_um(id));
  row.bottom = stack.draw_height(id).height;
  row.visible = !type_hidden(window, layer.type);
  return row;
}

bool entry_drawn(const std::vector<bool>& drawable, std::size_t i) {
  return i >= drawable.size() || drawable[i];
}

std::string_view trim_left(std::string_view s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())) != 0) s.remove_prefix(1);
  return s;
}

std::string_view first_word(std::string_view line) {
  std::size_t end = 0;
  while (end < line.size() && std::isspace(static_cast<unsigned char>(line[end])) == 0 && line[end] != ';') ++end;
  return line.substr(0, end);
}

std::string format_float(float v) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%g", static_cast<double>(v));
  return buf;
}

bool pans(PointerButton button, std::uint8_t modifiers) {
  if (button == PointerButton::middle || button == PointerButton::right) return true;
  return button == PointerButton::left && (modifiers & (kModAlt | kModShift)) != 0;
}

}  // namespace

// --- layer stack panel -----------------------------------------------------------

std::string micron_range(const std::optional<view3d::PhysicalHeight>& height) {
  if (!height) return {};
  char buf[64];
  std::snprintf(buf, sizeof buf, "%.2f–%.2f", height->height_um, height->height_um + height->thickness_um);
  return buf;
}

std::vector<View3dLayerRow> layer_rows(const view3d::LayerStack& stack, const view3d::WindowState& window,
                                       std::span<const float> alpha, const std::vector<bool>& drawable) {
  std::vector<View3dLayerRow> rows;
  std::map<std::string, std::size_t, std::less<>> by_name;
  for (view3d::LayerId id = 0; id < stack.size(); ++id) {
    if (!entry_drawn(drawable, id)) continue;
    const auto [it, added] = by_name.try_emplace(stack.layers()[id].name, rows.size());
    if (added) {
      rows.push_back(new_row(stack, id, window));
      rows.back().alpha = id < alpha.size() ? alpha[id] : 1.0F;
    }
    rows[it->second].layers.push_back(id);
  }
  std::stable_sort(rows.begin(), rows.end(),
                   [](const View3dLayerRow& a, const View3dLayerRow& b) { return a.bottom < b.bottom; });
  return rows;
}

std::vector<bool> hidden_flags(const view3d::LayerStack& stack, const view3d::WindowState& window) {
  std::vector<bool> hidden(stack.size(), false);
  for (std::size_t i = 0; i < stack.size(); ++i) hidden[i] = type_hidden(window, stack.layers()[i].type);
  return hidden;
}

bool visibility_editable(const view3d::WindowState& window) noexcept {
  return window.mode == view3d::Mode::layout_layer;
}

// --- commands --------------------------------------------------------------------

std::string window_command(std::string_view path, std::string_view command) {
  std::string line{path};
  line += ' ';
  line += command;
  return line;
}

std::string see_command(std::string_view path, std::string_view layer, bool show) {
  return window_command(path, std::string{show ? "see " : "see no "} + std::string{layer});
}

std::vector<std::string> isolate_commands(std::string_view path, std::string_view layer) {
  return {window_command(path, "see no *"), see_command(path, layer, true)};
}

std::string show_all_command(std::string_view path) {
  return window_command(path, "see *");
}

std::string preset_command(std::string_view path, view3d::Preset preset) {
  const view3d::Angles a = view3d::preset(preset);
  return window_command(path, "view " + format_float(a.x) + " " + format_float(a.y) + " " + format_float(a.z));
}

std::string level_command(std::string_view path, int step) {
  return window_command(path, step > 0 ? "level up" : "level down");
}

std::string mode_command(std::string_view path) {
  return window_command(path, "cif");
}

std::string cutbox_command(std::string_view path, bool on) {
  return window_command(path, on ? "cutbox box" : "cutbox none");
}

std::string close_command(std::string_view path) {
  return window_command(path, "closewindow");
}

float next_alpha(float alpha) noexcept {
  if (alpha > 0.75F) return 0.5F;
  if (alpha > 0.35F) return 0.2F;
  return 1.0F;
}

// --- focus routing ---------------------------------------------------------------

std::string route_typed_line(std::string_view line, const std::optional<std::string>& focused_path,
                             const IsWindowCommand& is_window_command) {
  const std::string_view body = trim_left(line);
  if (!focused_path || !is_window_command) return std::string{line};
  const std::string_view word = first_word(body);
  if (word.empty() || !is_window_command(word)) return std::string{line};
  return window_command(*focused_path, body);
}

// --- view options ----------------------------------------------------------------

void set_row_alpha(View3dOptions& options, const View3dLayerRow& row, float alpha, std::size_t stack_size) {
  if (options.alpha.size() < stack_size) options.alpha.resize(stack_size, 1.0F);
  for (const view3d::LayerId id : row.layers) {
    if (id < options.alpha.size()) options.alpha[id] = alpha;
  }
}

float xy_factor(const view3d::WindowState& window) noexcept {
  return window.mode == view3d::Mode::mask_layer ? window.applied_factor : 1.0F;
}

render::DrawParams draw_params(const FrameInputs& in) {
  render::DrawParams params;
  if (in.options != nullptr) {
    params.projection.kind = in.options->perspective ? view3d::Projection::perspective : view3d::Projection::orthographic;
    params.alpha = in.options->alpha;
  }
  params.stack = in.stack;
  params.styles = in.styles;
  params.highlight = in.highlight;
  if (in.window != nullptr) {
    params.level = in.window->level;
    params.xy_scale = xy_factor(*in.window);
    if (in.stack != nullptr) params.hidden = hidden_flags(*in.stack, *in.window);
  }
  return params;
}

// --- mesh builds -----------------------------------------------------------------

view3d::ClipArea clip_area(const db::CellDef& cell, const view3d::WindowState& window) {
  if (window.cut_box) return view3d::ClipArea{*window.cut_box, true, 0};
  const db::Rect& b = cell.bbox();
  return view3d::ClipArea{db::Rect{b.x0 - 1, b.y0 - 1, b.x1 + 1, b.y1 + 1}, false, 0};
}

BuildKey build_key(const editor::EditorState& editor, const view3d::WindowState& window,
                   const view3d::LayerStack& stack, bool span_connected_layers) {
  BuildKey key;
  key.cell = editor.cells.find(window.cell);
  if (key.cell != nullptr) {
    const view3d::ClipArea clip = clip_area(*key.cell, window);
    key.clip = clip.area;
    key.clipped = clip.cut_box;
  }
  key.span_connected_layers = span_connected_layers;
  key.technology = editor.technology.get();
  key.layers.assign(stack.layers().begin(), stack.layers().end());
  key.magic_compat = stack.magic_compat();
  key.microns_per_unit = stack.microns_per_unit();
  return key;
}

view3d::BuildInputs build_inputs(editor::EditorState& editor, const view3d::WindowState& window,
                                 std::shared_ptr<const view3d::LayerStack> stack, bool span_connected_layers) {
  view3d::BuildInputs in;
  in.stack = std::move(stack);
  in.cell = editor.cells.find(window.cell);
  if (in.cell != nullptr) in.clip = clip_area(*in.cell, window);
  in.span_connected_layers = span_connected_layers;
  in.technology = editor.technology.get();
  in.lock = &editor.database_lock;
  return in;
}

std::optional<ChangeAreas> change_areas(const view::LayoutWindow& window, const db::CellDef* rendered) {
  if (rendered == nullptr || window.root() != rendered) return std::nullopt;
  const view::Redisplay& pending = window.pending_redisplay();
  if (pending.empty()) return std::nullopt;
  return ChangeAreas{pending.all, pending.areas};
}

std::string grouped(std::size_t n) {
  std::string digits = std::to_string(n);
  for (std::size_t i = digits.size(); i > 3; i -= 3) digits.insert(i - 3, ",");
  return digits;
}

std::string mesh_status_text(const MeshStatus& status) {
  if (status.building) {
    const int percent = static_cast<int>(std::lround(std::clamp(status.progress, 0.0, 1.0) * 100.0));
    return "Building mesh · " + std::to_string(percent) + " %";
  }
  if (!status.have_model) return "No mesh";
  return "Mesh ready · " + grouped(status.rectangles) + (status.rectangles == 1 ? " rectangle" : " rectangles");
}

std::size_t rectangle_count(const view3d::MeshModel& mesh) noexcept {
  return std::accumulate(mesh.chunks.begin(), mesh.chunks.end(), std::size_t{0},
                         [](std::size_t n, const view3d::Chunk& c) { return n + c.rects.size(); });
}

// --- header, notice --------------------------------------------------------------

View3dHeader view3d_header(const view3d::WindowState& window) {
  View3dHeader h;
  h.title = window.cell + " · 3D";
  h.subtitle = window.mode == view3d::Mode::mask_layer ? "mask layers" : "layout layers";
  h.subtitle += " · level " + std::to_string(window.level);
  if (window.toolkit_path == view3d::kRender3dToolkitPath) {
    h.subtitle += " · render3d opened this tab";
  } else {
    h.subtitle += " · " + window.toolkit_path;
  }
  return h;
}

std::optional<std::string> mask_mode_notice(const view3d::WindowState& window) {
  if (window.mode != view3d::Mode::mask_layer) return std::nullopt;
  return std::string{"Mask layers arrive in phase 2"};
}

// --- gestures --------------------------------------------------------------------

std::optional<view3d::Gesture> drag_gesture(PointerButton button, std::uint8_t modifiers, float dx, float dy,
                                            int width, int height) {
  if (pans(button, modifiers)) return view3d::pan_drag(dx, dy, width, height);
  if (button == PointerButton::left) return view3d::orbit_drag(dx, dy);
  return std::nullopt;
}

std::optional<view3d::Gesture> wheel_gesture(double wheel_x, double wheel_y, bool precise, std::uint8_t modifiers,
                                             int width, int height) {
  const bool zoom_modifier = (modifiers & (kModCtrl | kModSuper)) != 0;
  if (precise && !zoom_modifier) {
    return view3d::pan_drag(static_cast<float>(wheel_x * kTrackpadLinePixels),
                            static_cast<float>(wheel_y * kTrackpadLinePixels), width, height);
  }
  if (wheel_y == 0.0) return std::nullopt;
  return view3d::zoom_pinch(std::pow(kWheelZoomStep, static_cast<float>(wheel_y)), (modifiers & kModShift) != 0);
}

std::optional<view3d::Gesture> pinch_gesture(double scale, std::uint8_t modifiers) {
  if (!(scale > 0.0) || !std::isfinite(scale)) return std::nullopt;
  return view3d::zoom_pinch(static_cast<float>(scale), (modifiers & kModShift) != 0);
}

std::optional<std::string> key_macro(const ::hefesto::ui::MacroTables& macros, int native_key, std::uint8_t modifiers) {
#if defined(__APPLE__)
  constexpr bool kMacos = true;
#else
  constexpr bool kMacos = false;
#endif
  const std::optional<::hefesto::ui::KeyEvent> event = key_event_for(native_key, modifiers);
  if (!event) return std::nullopt;
  const std::optional<::hefesto::ui::Key> key = ::hefesto::ui::normalize_key_event(*event, kMacos);
  if (!key) return std::nullopt;
  const ::hefesto::ui::Macro* macro = macros.find("wind3d", *key);
  if (macro == nullptr || macro->text.empty()) return std::nullopt;
  return macro->text;
}

void PointerTracker::press(float x, float y, PointerButton button, std::uint8_t modifiers) {
  pressed_ = true;
  dragging_ = false;
  button_ = button;
  modifiers_ = modifiers;
  press_x_ = last_x_ = x;
  press_y_ = last_y_ = y;
}

std::optional<view3d::Gesture> PointerTracker::move(float x, float y, int width, int height) {
  if (!pressed_) return std::nullopt;
  if (!dragging_ && std::hypot(x - press_x_, y - press_y_) < kClickSlop) return std::nullopt;
  dragging_ = true;
  const float dx = x - last_x_;
  const float dy = y - last_y_;
  last_x_ = x;
  last_y_ = y;
  return drag_gesture(button_, modifiers_, dx, dy, width, height);
}

std::optional<std::pair<float, float>> PointerTracker::release(float x, float y) {
  const bool click = pressed_ && !dragging_ && button_ == PointerButton::left &&
                     std::hypot(x - press_x_, y - press_y_) < kClickSlop;
  pressed_ = false;
  dragging_ = false;
  if (!click) return std::nullopt;
  return std::pair{x, y};
}

// --- picking ---------------------------------------------------------------------

std::optional<view3d::PickTarget> PickResolver::resolve(const view3d::MeshModel& mesh, view3d::PickId id) {
  if (scope_.stack == nullptr) return view3d::resolve(mesh, id);
  return view3d::resolve(mesh, scope_, id);
}

diag::Status PickResolver::select(editor::EditorState& editor, const view3d::PickTarget& target) {
  return view3d::to_selection(editor, target);
}

view3d::HighlightSet PickResolver::from_selection(const view3d::MeshModel& mesh, const editor::EditorState& editor) {
  return view3d::from_selection(mesh, scope_, editor.selection);
}

view3d::HighlightSet PickResolver::net(const view3d::MeshModel& mesh, const editor::EditorState& editor,
                                       const view3d::PickTarget& seed) {
  if (scope_.stack == nullptr) return view3d::net_highlight(mesh, editor, seed);
  return view3d::net_highlight(mesh, editor, seed,
                               view3d::NetOptions{view3d::NetScope::hierarchical, scope_.magic_compat, scope_.stack});
}

std::string pick_caption(const view3d::PickTarget& target, const view3d::LayerStack& stack) {
  const std::string name = target.layer < stack.size() ? stack.layers()[target.layer].name : std::string{"layer"};
  std::string caption = name + " · picked";
  if (!target.use_path.empty()) caption += " in " + target.use_path.back();
  return caption;
}

std::optional<std::string> View3dPicking::click(const view3d::MeshModel* mesh, view3d::PickId id,
                                                editor::EditorState& editor, const view3d::LayerStack& stack) {
  picked_.reset();
  if (mesh == nullptr || id == view3d::kNoPick) return std::nullopt;
  picked_ = resolver_.resolve(*mesh, id);
  if (!picked_) return std::nullopt;
  const diag::Status selected = resolver_.select(editor, *picked_);
  return pick_caption(*picked_, stack) + (selected.ok() ? " · selected in 2D" : "");
}

bool View3dPicking::toggle_isolate_net(const view3d::MeshModel* mesh, const editor::EditorState& editor) {
  if (net_) {
    net_.reset();
    net_stale_ = false;
    return true;
  }
  if (mesh == nullptr || !picked_) return false;
  net_ = resolver_.net(*mesh, editor, *picked_);
  net_stale_ = false;
  return true;
}

const view3d::HighlightSet* View3dPicking::highlight(const view3d::MeshModel* mesh,
                                                     const editor::EditorState& editor) {
  if (net_ && net_stale_ && mesh != nullptr && picked_) {
    net_ = resolver_.net(*mesh, editor, *picked_);
    net_stale_ = false;
  }
  if (net_) return net_->empty() ? nullptr : &*net_;
  if (mesh == nullptr) return nullptr;
  shown_ = resolver_.from_selection(*mesh, editor);
  return shown_.empty() ? nullptr : &shown_;
}

void View3dPicking::reset() {
  picked_.reset();
  net_.reset();
  net_stale_ = false;
  shown_ = {};
}

// --- frame time ------------------------------------------------------------------

void FrameTimer::add(double milliseconds) {
  if (samples_.size() < kWindow) {
    samples_.push_back(milliseconds);
    return;
  }
  samples_[next_] = milliseconds;
  next_ = (next_ + 1) % kWindow;
}

double FrameTimer::average_ms() const noexcept {
  if (samples_.empty()) return 0.0;
  return std::accumulate(samples_.begin(), samples_.end(), 0.0) / static_cast<double>(samples_.size());
}

std::string FrameTimer::text() const {
  if (samples_.empty()) return "—";
  const double ms = average_ms();
  char buf[64];
  std::snprintf(buf, sizeof buf, "%.1f ms · %.0f fps", ms, ms > 0.0 ? 1000.0 / ms : 0.0);
  return buf;
}

}  // namespace hefesto::shell
