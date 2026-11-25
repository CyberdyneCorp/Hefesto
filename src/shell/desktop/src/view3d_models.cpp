// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The 3D view's view models (hefesto/shell/view3d_models.hpp). Owner: SHELL.

#include "hefesto/shell/view3d_models.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <map>
#include <numeric>
#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/script/binding/tags.hpp"
#include "hefesto/shell/input_router.hpp"
#include "hefesto/ui/input.hpp"
#include "hefesto/view/window.hpp"
#include "hefesto/view3d/camera/input.hpp"
#include "hefesto/view3d/client/scene.hpp"

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

// Every digit a `float` needs to read back as the same value.
std::string exact_float(float v) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.9g", static_cast<double>(v));
  return buf;
}

int percent(float alpha) {
  return static_cast<int>(alpha * 100.0F + 0.5F);
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
                                       std::span<const float> alpha, const std::vector<bool>& drawable,
                                       const std::vector<bool>& ghosted) {
  std::vector<View3dLayerRow> rows;
  std::map<std::string, std::size_t, std::less<>> by_name;
  for (view3d::LayerId id = 0; id < stack.size(); ++id) {
    if (!entry_drawn(drawable, id)) continue;
    const auto [it, added] = by_name.try_emplace(stack.layers()[id].name, rows.size());
    if (added) {
      rows.push_back(new_row(stack, id, window));
      rows.back().alpha = id < alpha.size() ? alpha[id] : 1.0F;
      rows.back().ghosted = id < ghosted.size() && ghosted[id];
    }
    rows[it->second].layers.push_back(id);
  }
  std::stable_sort(rows.begin(), rows.end(),
                   [](const View3dLayerRow& a, const View3dLayerRow& b) { return a.bottom < b.bottom; });
  return rows;
}

std::string row_accessible_name(const View3dLayerRow& row) {
  std::string name = row.name;
  if (!row.range_um.empty()) name += ", " + row.range_um + " µm";
  name += row.visible ? ", shown" : ", hidden";
  if (row.alpha < 1.0F) {
    name += (row.ghosted ? ", ghosted, " : ", ") + std::to_string(percent(row.alpha)) + " percent opaque";
  }
  return name;
}

std::string alpha_button_name(const View3dLayerRow& row) {
  return "Transparency of " + row.name + ", " + std::to_string(percent(row.alpha)) + " percent" +
         (row.ghosted ? ", ghosted by default" : "");
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

std::vector<std::string> camera_commands(std::string_view path, const view3d::ViewState& camera) {
  return {window_command(path, "view " + exact_float(camera.view_x) + " " + exact_float(camera.view_y) + " " +
                                   exact_float(camera.view_z)),
          window_command(path, "scroll " + exact_float(camera.trans_x) + " " + exact_float(camera.trans_y) + " " +
                                   exact_float(camera.trans_z)),
          window_command(path, "zoom " + exact_float(camera.scale_xy) + " " + exact_float(camera.scale_z))};
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
  options.alpha_override.insert_or_assign(row.name, alpha);
}

void apply_default_alpha(View3dOptions& options, const view3d::LayerStack& stack, const std::vector<bool>& ghosted) {
  options.alpha.assign(stack.size(), 1.0F);
  for (std::size_t i = 0; i < stack.size(); ++i) {
    const auto user = options.alpha_override.find(stack.layers()[i].name);
    if (user != options.alpha_override.end()) {
      options.alpha[i] = user->second;
    } else if (i < ghosted.size() && ghosted[i]) {
      options.alpha[i] = kGhostAlpha;
    }
  }
}

void GhostDefaults::building(const view3d::LayerStack& stack, std::vector<bool> ghosted, bool drawing,
                             View3dOptions& options) {
  pending_ = std::move(ghosted);
  if (!drawing) {
    ghosted_ = pending_;
    apply_default_alpha(options, stack, ghosted_);
  }
}

void GhostDefaults::drawn(const view3d::LayerStack& stack, View3dOptions& options) {
  ghosted_ = pending_;
  apply_default_alpha(options, stack, ghosted_);  // the user's per-row values win
}

void GhostDefaults::clear() noexcept {
  pending_.clear();
  ghosted_.clear();
}

// --- the board look --------------------------------------------------------------

std::array<float, 3> view3d_background(ThemeMode mode) noexcept {
  const Rgba& c = theme_tokens(mode).canvas;
  return {c.r, c.g, c.b};
}

bool DefaultCameraLatch::take(bool open) noexcept {
  if (!open) {
    applied_ = false;
    return false;
  }
  if (applied_) return false;
  applied_ = true;
  return true;
}

view3d::Extent3 stack_extent(const db::Rect& area, const view3d::LayerStack& stack,
                             const std::vector<bool>& drawable) {
  view3d::Extent3 box{static_cast<double>(area.x0), static_cast<double>(area.y0), static_cast<double>(area.x1),
                      static_cast<double>(area.y1), 0.0, 0.0};
  bool any = false;
  for (view3d::LayerId id = 0; id < stack.size(); ++id) {
    if (!entry_drawn(drawable, id)) continue;
    const view3d::DrawHeight h = stack.draw_height(id);
    const auto bottom = static_cast<double>(h.height);
    const double top = bottom + static_cast<double>(h.thickness);
    box.z0 = any ? std::min(box.z0, bottom) : bottom;
    box.z1 = any ? std::max(box.z1, top) : top;
    any = true;
  }
  return box;
}

std::vector<bool> layers_present(const view3d::MeshModel& mesh, std::size_t stack_size) {
  std::vector<bool> present(stack_size, false);
  const auto mark = [&present](view3d::LayerId id) {
    if (id < present.size()) present[id] = true;
  };
  for (const view3d::Chunk& chunk : mesh.chunks) {
    for (const view3d::RectInstance& r : chunk.rects) mark(r.layer);
    for (const view3d::Triangle& t : chunk.triangles) mark(t.layer);
  }
  return present;
}

bool frame_ready(const CameraFrame& frame) noexcept {
  return frame.window != nullptr && frame.mesh != nullptr && frame.stack != nullptr && frame.width_px > 0 &&
         frame.height_px > 0;
}

std::vector<std::string> preset_camera_commands(const CameraFrame& frame, view3d::Preset which,
                                                std::optional<float> scale_z) {
  if (frame.window == nullptr) return {};
  const std::string& path = frame.window->toolkit_path;
  if (!frame_ready(frame)) return {preset_command(path, which)};
  view3d::ViewState camera = frame.window->view;
  const view3d::Angles angles = view3d::preset(which);
  camera.view_x = angles.x;
  camera.view_y = angles.y;
  camera.view_z = angles.z;
  if (scale_z) camera.scale_z = *scale_z;
  // The layers the mesh draws (not the whole technology), in its region.
  std::vector<bool> framed = layers_present(*frame.mesh, frame.stack->size());
  for (std::size_t i = 0; i < framed.size() && i < frame.drawable.size(); ++i) {
    framed[i] = framed[i] && frame.drawable[i];
  }
  // The mesh's own units (mask units in mask-layer mode), as the renderer draws them.
  const view3d::Extent3 box = stack_extent(frame.mesh->region, *frame.stack, framed);
  return camera_commands(path, view3d::frame_view(camera, box, frame.width_px, frame.height_px));
}

bool desktop_opened(const view3d::WindowState& window) noexcept {
  return window.toolkit_path == view3d::kRender3dToolkitPath;
}

float board_scale_z(const view3d::WindowState& window) noexcept {
  return window.mode == view3d::Mode::mask_layer ? kBoardScaleZ * window.applied_factor : kBoardScaleZ;
}

std::vector<std::string> default_camera_commands(const CameraFrame& frame) {
  if (!frame_ready(frame) || !desktop_opened(*frame.window)) return {};
  return preset_camera_commands(frame, view3d::Preset::isometric, board_scale_z(*frame.window));
}

std::array<AxisArrow, 3> axis_gizmo(const view3d::ViewState& view) {
  view3d::ViewState rotation;
  rotation.view_x = view.view_x;
  rotation.view_y = view.view_y;
  rotation.view_z = view.view_z;
  const view3d::Matrix4 m = view3d::matrix(rotation, 1, 1, view3d::ProjectionParams{});
  // Column-major: the image of axis j of the displayed scene is column j;
  // the screen's y is the negated clip y (the renderer draws clip y up). The
  // scene is the model mirrored in z, so its +z points to the higher layers.
  const auto arrow = [&m](char name, int column, float sign) {
    return AxisArrow{name, sign * m[static_cast<std::size_t>(column * 4)],
                     -sign * m[static_cast<std::size_t>(column * 4 + 1)]};
  };
  return {arrow('x', 0, 1.0F), arrow('y', 1, 1.0F), arrow('z', 2, 1.0F)};
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
  // The board look in both magic-compat modes (Q-MISC-84).
  if (in.background) params.background = in.background;
  params.lighting = render::Lighting::board;
  if (in.window != nullptr) {
    params.level = in.window->level;
    if (in.hidden != nullptr) {
      params.hidden = *in.hidden;
    } else if (in.stack != nullptr) {
      params.hidden = hidden_flags(*in.stack, *in.window);
    }
  }
  return params;
}

// --- mesh builds -----------------------------------------------------------------

BuildKey build_key(const view3d::BuildInputs& in, bool span_connected_layers) {
  BuildKey key;
  key.cell = in.cell;
  if (key.cell != nullptr) {
    key.clip = in.clip.area;
    key.clipped = in.clip.cut_box;
  }
  key.span_connected_layers = span_connected_layers;
  key.technology = in.technology;
  // A new output style (even with the same layers) is a new mask source: a full build.
  key.mask_style = in.mask ? std::shared_ptr<const void>{in.mask->style} : nullptr;
  if (in.stack != nullptr) {
    key.mode = in.stack->mode();
    key.layers.assign(in.stack->layers().begin(), in.stack->layers().end());
    key.magic_compat = in.stack->magic_compat();
    key.microns_per_unit = in.stack->microns_per_unit();
  }
  return key;
}

bool model_is_current(const view3d::MeshModel& model, std::uint64_t build_id) noexcept {
  return model.build_id == build_id;
}

view3d::BuildInputs build_inputs(editor::EditorState& editor, view3d::BuildInputs window_inputs,
                                 bool span_connected_layers) {
  view3d::BuildInputs in = std::move(window_inputs);
  in.span_connected_layers = span_connected_layers;
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

bool PickResolver::select(editor::EditorState& editor, const view3d::PickTarget& target) {
  return view3d::link_to_2d(editor, target).has_value();
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
  selected_in_2d_ = false;
  if (mesh == nullptr || id == view3d::kNoPick) return std::nullopt;
  picked_ = resolver_.resolve(*mesh, id);
  if (!picked_) return std::nullopt;
  selected_in_2d_ = resolver_.select(editor, *picked_);
  return pick_caption(*picked_, stack) + (selected_in_2d_ ? " · selected in 2D" : "");
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
  selected_in_2d_ = false;
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

// --- controls and their screen-reader names ----------------------------------------

namespace {

View3dControlView plain(std::string label, bool enabled, std::string tooltip) {
  View3dControlView v;
  v.accessible_name = label;
  v.label = std::move(label);
  v.enabled = enabled;
  v.tooltip = std::move(tooltip);
  return v;
}

View3dControlView toggle(std::string label, bool on, bool enabled, std::string tooltip) {
  View3dControlView v;
  v.accessible_name = view3d_toggle_name(label, on);
  v.label = std::move(label);
  v.on = on;
  v.enabled = enabled;
  v.tooltip = std::move(tooltip);
  return v;
}

View3dControlView preset_view(const char* name, bool open) {
  return plain(name, open,
               std::string{"Camera preset "} + name + ": view with the preset's angles, then scroll and zoom to frame the cell");
}

// The P2 actions and the side panel's View and Inspect controls.
std::optional<View3dControlView> panel_control(View3dControl control, const View3dControlState& s) {
  switch (control) {
    case View3dControl::cross_section:
      return toggle("Cross-section", s.section_on, s.window_open,
                    "Cut A–A′ along the layout box's long axis (or click two points in the 3D view) and show the "
                    "section in microns");
    case View3dControl::export_model:
      return plain("Export glTF…", s.window_open && s.export_available,
                   "Export the 3D model: export3d <file> (.gltf, .glb or .stl)");
    case View3dControl::orthographic:
      return toggle("Orthographic", !s.perspective, true, "Orthographic projection (Magic's)");
    case View3dControl::perspective:
      return toggle("Perspective", s.perspective, true, "Perspective projection (a Hefesto view option)");
    case View3dControl::isometric: return preset_view("Isometric", s.window_open);
    case View3dControl::top: return preset_view("Top", s.window_open);
    case View3dControl::front: return preset_view("Front", s.window_open);
    case View3dControl::side: return preset_view("Side", s.window_open);
    case View3dControl::substrate:
      return toggle("Substrate", s.substrate_on && s.substrate_available, s.substrate_available,
                    "A ghosted substrate slab under the stack (board look)");
    case View3dControl::isolate_net:
      return toggle("Isolate selected net", s.net_isolated, s.picked || s.net_isolated,
                    "Highlight the picked target's net and dim the rest");
    case View3dControl::span_connected_layers:
      return toggle("Span connected layers", s.span_connected_layers, true,
                    "Off: contacts and vias use their tech height and thickness, as Magic. On: each spans from the "
                    "lower layer's top to the upper layer's bottom.");
    case View3dControl::frame_time:
      return toggle("Frame time", s.frame_time, true, "Show the 3D frame's draw time and frame rate");
    case View3dControl::close_view: return plain("Close 3D view", s.window_open, "Close the 3D window: closewindow");
    case View3dControl::show_all_layers:
      return plain("Show all layers", s.window_open && s.visibility_editable, "Show every layer: see *");
    default: return std::nullopt;
  }
}

// The floating toolbar's controls.
View3dControlView toolbar_control(View3dControl control, const View3dControlState& s) {
  switch (control) {
    case View3dControl::mask_layers:
      return toggle("Mask layers", s.mask_mode, s.window_open, "Mask-layer mode (cif): the mask layers of the current output style");
    case View3dControl::layout_layers:
      return toggle("Layout layers", !s.mask_mode, s.window_open, "Layout-layer mode (cif)");
    case View3dControl::level_down: {
      View3dControlView v = plain("−", s.window_open, "Level down (level down)");
      v.accessible_name = "Level down";
      return v;
    }
    case View3dControl::level_up: {
      View3dControlView v = plain("+", s.window_open, "Level up (level up)");
      v.accessible_name = "Level up";
      return v;
    }
    default:
      return toggle("Cut box", s.cut_box, s.window_open, "Clip to the layout box: cutbox box / cutbox none");
  }
}

}  // namespace

std::string view3d_toggle_name(std::string_view label, bool on) {
  return std::string{label} + (on ? ", on" : ", off");
}

View3dControlView control_view(View3dControl control, const View3dControlState& state) {
  if (std::optional<View3dControlView> v = panel_control(control, state)) return std::move(*v);
  return toolbar_control(control, state);
}

std::string level_text(int level) {
  return "Level " + std::to_string(level);
}

// --- layout ------------------------------------------------------------------------

float grid_cell_width(float width, int columns, float spacing) noexcept {
  if (columns <= 0) return width;
  const float cell = (width - spacing * static_cast<float>(columns - 1)) / static_cast<float>(columns);
  return std::max(0.0F, std::floor(cell));
}

std::vector<std::size_t> flow_rows(std::span<const float> widths, float available, float spacing) {
  std::vector<std::size_t> rows;
  rows.reserve(widths.size());
  std::size_t row = 0;
  float used = 0.0F;
  for (const float w : widths) {
    const bool first = rows.empty() || used == 0.0F;
    if (!first && used + spacing + w > available) {
      ++row;
      used = 0.0F;
    }
    used += (used == 0.0F ? 0.0F : spacing) + w;
    rows.push_back(row);
  }
  return rows;
}

float flow_width(std::span<const float> widths, const std::vector<std::size_t>& rows, float spacing) {
  std::map<std::size_t, float> row_width;
  for (std::size_t i = 0; i < widths.size() && i < rows.size(); ++i) {
    auto [it, fresh] = row_width.try_emplace(rows[i], 0.0F);
    it->second += (fresh ? 0.0F : spacing) + widths[i];
  }
  float widest = 0.0F;
  for (const auto& [row, w] : row_width) widest = std::max(widest, w);
  return widest;
}

// --- export ------------------------------------------------------------------------

platform::FilePickerRequest export_request(const view3d::WindowState& window) {
  platform::FilePickerRequest request;
  request.mode = platform::FilePickerRequest::Mode::save;
  request.title = "Export 3D model";
  request.initial_file = (window.cell.empty() ? std::string{"model"} : window.cell) + ".gltf";
  request.file_types = {{"glTF", "*.gltf"}, {"glTF binary", "*.glb"}, {"STL", "*.stl"}};
  return request;
}

std::string export_command(std::string_view path, std::string_view file, bool on_window_table) {
  const std::string command = "export3d " + script::quote_list_element(file);
  return on_window_table ? window_command(path, command) : command;
}

// --- cross-section -----------------------------------------------------------------

view3d::Segment box_cut_line(const db::Rect& area) {
  if (area.x1 - area.x0 >= area.y1 - area.y0) {
    const db::Coord y = area.y0 + (area.y1 - area.y0) / 2;
    return view3d::Segment{db::Point{area.x0, y}, db::Point{area.x1, y}};
  }
  const db::Coord x = area.x0 + (area.x1 - area.x0) / 2;
  return view3d::Segment{db::Point{x, area.y0}, db::Point{x, area.y1}};
}

view3d::Segment axis_aligned_line(db::Point a, db::Point b) {
  const auto dx = std::llabs(static_cast<long long>(b.x) - a.x);
  const auto dy = std::llabs(static_cast<long long>(b.y) - a.y);
  if (dx >= dy) return view3d::Segment{a, db::Point{b.x, a.y}};
  return view3d::Segment{a, db::Point{a.x, b.y}};
}

std::optional<db::Rect> section_area(const editor::EditorState& editor, const view3d::WindowState& window) {
  const db::CellDef* cell = editor.cells.find(window.cell);
  if (cell == nullptr) return std::nullopt;
  // Magic's box root: the box's own cell, else the edit cell (cutbox box reads it the same way).
  const db::CellDef* box_root = editor.box_root != nullptr ? editor.box_root : editor.edit_cell;
  if (editor.box && !editor.box->is_null() && box_root == cell) return *editor.box;
  if (window.cut_box && !window.cut_box->is_null()) return *window.cut_box;
  return cell->bbox();
}

render::SectionPlane section_clip_plane(const view3d::Segment& line) noexcept {
  render::SectionPlane plane;
  if (line.a.y == line.b.y) {
    plane.axis = render::SectionPlane::Axis::y;
    plane.position = static_cast<double>(line.a.y);
  } else {
    plane.axis = render::SectionPlane::Axis::x;
    plane.position = static_cast<double>(line.a.x);
  }
  plane.keep_greater = true;
  return plane;
}

db::Point ground_to_root(db::Point ground, float xy_factor) noexcept {
  if (!std::isfinite(xy_factor) || xy_factor <= 0.0F || xy_factor == 1.0F) return ground;
  const double f = static_cast<double>(xy_factor);
  return db::Point{static_cast<db::Coord>(std::lround(static_cast<double>(ground.x) / f)),
                   static_cast<db::Coord>(std::lround(static_cast<double>(ground.y) / f))};
}

view3d::Segment segment_in_mesh(const view3d::Segment& line, std::int32_t units_per_root) noexcept {
  if (units_per_root <= 1) return line;
  const auto scale = [units_per_root](db::Point p) {
    const auto clamp = [](std::int64_t v) {
      return static_cast<db::Coord>(std::clamp<std::int64_t>(v, db::kPlaneMin, db::kPlaneMax));
    };
    return db::Point{clamp(std::int64_t{p.x} * units_per_root), clamp(std::int64_t{p.y} * units_per_root)};
  };
  return view3d::Segment{scale(line.a), scale(line.b)};
}

namespace {

std::string two_decimals(double v) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.2f", v);
  return buf;
}

bool layer_hidden(const std::vector<bool>& hidden, view3d::LayerId layer) {
  return layer < hidden.size() && hidden[layer];
}

std::string section_text(const SectionPanel& panel, const view3d::CrossSection& section,
                         const view3d::LayerStack& stack, const std::vector<bool>& hidden) {
  std::map<view3d::LayerId, std::size_t> intervals;
  for (const view3d::CrossSection::Interval& i : section.intervals) {
    if (!layer_hidden(hidden, i.layer) && i.layer < stack.size()) ++intervals[i.layer];
  }
  std::string text = "Cross-section A–A′, " +
                     (panel.length_um > 0.0 ? two_decimals(panel.length_um) + " µm long" : "length unknown");
  if (intervals.empty()) text += ", no layer crosses the line";
  for (const auto& [layer, count] : intervals) {
    const std::string range = micron_range(stack.physical_height_um(layer));
    text += "; " + stack.layers()[layer].name + (range.empty() ? std::string{} : " " + range + " µm") + ", " +
            std::to_string(count) + (count == 1 ? " interval" : " intervals");
  }
  if (!panel.no_height.empty()) text += "; no height: ";
  for (std::size_t i = 0; i < panel.no_height.size(); ++i) text += (i == 0 ? "" : ", ") + panel.no_height[i];
  return text;
}

}  // namespace

SectionPanel section_panel(const view3d::CrossSection& section, const view3d::LayerStack& stack, double length_um,
                           float width, float height, const std::vector<bool>& hidden) {
  SectionPanel panel;
  panel.length_um = std::max(0.0, length_um);
  for (const view3d::CrossSection::Interval& i : section.intervals) {
    if (!layer_hidden(hidden, i.layer)) panel.top_um = std::max(panel.top_um, i.top_um);
  }
  for (const view3d::LayerId id : section.no_height) {
    if (id < stack.size()) panel.no_height.push_back(stack.layers()[id].name);
  }
  std::map<view3d::LayerId, bool> layers;
  if (panel.length_um > 0.0 && panel.top_um > 0.0) {
    const double sx = static_cast<double>(width) / panel.length_um;
    const double sy = static_cast<double>(height) / panel.top_um;
    for (const view3d::CrossSection::Interval& i : section.intervals) {
      if (layer_hidden(hidden, i.layer)) continue;
      layers[i.layer] = true;
      const double x0 = std::clamp(std::min(i.from_um, i.to_um), 0.0, panel.length_um) * sx;
      const double x1 = std::clamp(std::max(i.from_um, i.to_um), 0.0, panel.length_um) * sx;
      const double y_top = static_cast<double>(height) - i.top_um * sy;
      panel.bars.push_back(SectionBar{i.layer, static_cast<float>(x0), static_cast<float>(y_top),
                                      static_cast<float>(x1 - x0),
                                      static_cast<float>((i.top_um - i.bottom_um) * sy)});
    }
  }
  // A technology without a µm scale has no length in µm (the window test's tech).
  const std::string length = panel.length_um > 0.0 ? two_decimals(panel.length_um) + " µm" : "no µm scale";
  panel.summary = "A–A′ " + length + " · " + std::to_string(layers.size()) + (layers.size() == 1 ? " layer" : " layers");
  panel.accessible_name = section_text(panel, section, stack, hidden);
  return panel;
}

}  // namespace hefesto::shell
