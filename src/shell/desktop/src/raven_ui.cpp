// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The only translation unit of Hefesto that includes Raven. Each Raven entry
// point used here is listed in docs/decisions/raven-go-no-go.md ("API
// stability risk"); keep that list in sync when adding a call.
//
// Raven's convenience macros (R_TRUE, R_INVALID_ID) are C casts, which
// Hefesto's warning set rejects, so plain values are used instead.

#include "raven_ui.hpp"

#include "hefesto/shell/input_router.hpp"
#include "hefesto/shell/drag_gesture.hpp"
#include "hefesto/shell/press_hold.hpp"
#include "hefesto/shell/text_key_filter.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <utility>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#endif
#include <Raven.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace hefesto::shell::ui {

namespace {

constexpr int kTrue = 1;
constexpr int kFalse = 0;

std::string str(std::string_view s) {
  return std::string(s);
}

void on_raven_error(RError code, RSeverity severity, const char* detail) {
  std::fprintf(stderr, "hefesto: raven %s (%s): %s\n", Rerror_to_string(code), Rseverity_to_string(severity),
               detail != nullptr ? detail : "");
}

void set_color(RColorRole role, const Rgba& c) {
  Rtheme_set_color(role, c.r, c.g, c.b, c.a);
}

Rgba with_alpha(const Rgba& c, float a) {
  return Rgba{c.r, c.g, c.b, a};
}

/// Maps the forge tokens (and the mockup-derived shades) onto every Raven
/// color role, so no Raven default color is left visible.
void apply_tokens(const ThemeTokens& t) {
  set_color(R_COLOR_WINDOW, t.surface0);
  set_color(R_COLOR_PANEL, t.surface1);
  set_color(R_COLOR_BORDER, t.border);
  set_color(R_COLOR_TEXT, t.text_primary);
  set_color(R_COLOR_TEXT_DISABLED, t.text_tertiary);
  set_color(R_COLOR_TEXT_PLACEHOLDER, t.text_tertiary);
  set_color(R_COLOR_ACCENT, t.accent);
  set_color(R_COLOR_ACCENT_HOVERED, t.accent_hover);
  set_color(R_COLOR_ACCENT_PRESSED, t.accent_pressed);
  set_color(R_COLOR_ACCENT_TEXT, t.accent_text);
  set_color(R_COLOR_CONTROL, t.surface2);
  set_color(R_COLOR_CONTROL_HOVERED, t.row_selected);
  set_color(R_COLOR_CONTROL_PRESSED, t.row_selected);
  set_color(R_COLOR_CONTROL_BORDER, t.control_border);
  set_color(R_COLOR_INPUT, t.input);
  set_color(R_COLOR_FOCUS, t.accent);
  set_color(R_COLOR_SELECTION, t.row_selected);
  set_color(R_COLOR_ROW_HOVERED, with_alpha(t.row_selected, 0.6F));
  set_color(R_COLOR_TRACK, t.surface2);
  set_color(R_COLOR_HANDLE, t.control_border);
  set_color(R_COLOR_HANDLE_HOVERED, t.text_tertiary);
  set_color(R_COLOR_SEPARATOR, t.border);
  set_color(R_COLOR_HEADER, t.bar);
  set_color(R_COLOR_HEADER_HOVERED, t.surface2);
  set_color(R_COLOR_TAB_ACTIVE, t.surface1);
  set_color(R_COLOR_POPUP, t.surface2);
  set_color(R_COLOR_POPUP_BORDER, t.control_border);
  set_color(R_COLOR_SHADOW, Rgba{0.0F, 0.0F, 0.0F, kPopoverShadowAlpha});
  set_color(R_COLOR_TOOLTIP, t.surface2);
  set_color(R_COLOR_TOOLTIP_TEXT, t.text_primary);
  set_color(R_COLOR_SWITCH, t.accent);
  set_color(R_COLOR_SUCCESS, t.success);
  set_color(R_COLOR_WARNING, t.warning);
  set_color(R_COLOR_DANGER, t.danger);
  set_color(R_COLOR_LINK, t.accent);
  set_color(R_COLOR_CODE, t.input);
}

/// Desktop density and radius of "Spacing, radius, elevation, motion and density".
void apply_metrics() {
  Rtheme_set_metric(R_METRIC_FONT_SIZE, text_spec(TextRole::body).size_px);
  Rtheme_set_metric(R_METRIC_CORNER_RADIUS, static_cast<float>(kRadiusControl));
  Rtheme_set_metric(R_METRIC_POPUP_RADIUS, static_cast<float>(kRadiusPanel));
  Rtheme_set_metric(R_METRIC_BORDER_WIDTH, static_cast<float>(kPanelBorderPx));
  Rtheme_set_metric(R_METRIC_CONTROL_HEIGHT, static_cast<float>(kInputHeight));
  Rtheme_set_metric(R_METRIC_ROW_HEIGHT, 24.0F);
  Rtheme_set_metric(R_METRIC_PADDING_X, static_cast<float>(kSpacing[1]));
  Rtheme_set_metric(R_METRIC_PADDING_Y, static_cast<float>(kSpacing[0]));
  Rtheme_set_metric(R_METRIC_SPACING, static_cast<float>(kSpacing[1]));
  Rtheme_set_metric(R_METRIC_INDENT, static_cast<float>(kSpacing[3]));
  Rtheme_set_metric(R_METRIC_SPLITTER_SIZE, static_cast<float>(kPanelBorderPx));
  Rtheme_set_metric(R_METRIC_SCROLLBAR_SIZE, static_cast<float>(kSpacing[1]));
}

int face_key(FontFace face) {
  return (face.family == FontFamily::mono ? 1000 : 0) + face.weight;
}

std::optional<Rid> load_font_file(const std::filesystem::path& path) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec)) {
    return std::nullopt;
  }
  const Rid font = Rfont_create_from_file(path.string().c_str());
  if (Rfont_is_valid(font) == kFalse) {
    return std::nullopt;
  }
  return font;
}

std::string read_text_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

RAccessibleRole to_role(Role role) {
  switch (role) {
    case Role::none: return R_ROLE_NONE;
    case Role::group: return R_ROLE_GROUP;
    case Role::text: return R_ROLE_TEXT;
    case Role::heading: return R_ROLE_HEADING;
    case Role::button: return R_ROLE_BUTTON;
    case Role::toggle_button: return R_ROLE_TOGGLE_BUTTON;
    case Role::checkbox: return R_ROLE_CHECKBOX;
    case Role::list: return R_ROLE_LIST;
    case Role::list_item: return R_ROLE_LIST_ITEM;
    case Role::tree: return R_ROLE_TREE;
    case Role::tree_item: return R_ROLE_TREE_ITEM;
    case Role::tab_list: return R_ROLE_TAB_LIST;
    case Role::tab: return R_ROLE_TAB;
    case Role::toolbar: return R_ROLE_TOOLBAR;
    case Role::status_bar: return R_ROLE_STATUS_BAR;
    case Role::canvas: return R_ROLE_CANVAS;
    case Role::image: return R_ROLE_IMAGE;
    case Role::pane: return R_ROLE_PANE;
  }
  return R_ROLE_AUTO;
}

std::string upper(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return out;
}

PointerButton to_button(RMouseButton b) {
  switch (b) {
    case R_MOUSE_LEFT: return PointerButton::left;
    case R_MOUSE_RIGHT: return PointerButton::right;
    case R_MOUSE_MIDDLE: return PointerButton::middle;
    default: return PointerButton::other;
  }
}

// The shell's copies of Raven's key and modifier values (input_router.hpp).
static_assert(R_KEY_SPACE == raven_key::kSpace && R_KEY_ESCAPE == raven_key::kEscape &&
              R_KEY_ENTER == raven_key::kEnter && R_KEY_RIGHT == raven_key::kRight && R_KEY_UP == raven_key::kUp &&
              R_KEY_PAGE_UP == raven_key::kPageUp && R_KEY_HOME == raven_key::kHome && R_KEY_PAUSE == raven_key::kPause &&
              R_KEY_F1 == raven_key::kF1 && R_KEY_F12 == raven_key::kF12 && R_KEY_KP_0 == raven_key::kKp0 &&
              R_KEY_KP_9 == raven_key::kKp9 && R_KEY_KP_ENTER == raven_key::kKpEnter &&
              R_KEY_LEFT_SHIFT == raven_key::kLeftShift && R_KEY_RIGHT_SUPER == raven_key::kRightSuper &&
              R_KEY_MENU == raven_key::kMenu);
static_assert(static_cast<int>(R_MOD_SHIFT) == kModShift && static_cast<int>(R_MOD_CTRL) == kModCtrl &&
              static_cast<int>(R_MOD_ALT) == kModAlt && static_cast<int>(R_MOD_SUPER) == kModSuper);

InputKind to_kind(REventType type) {
  switch (type) {
    case R_EVENT_MOUSE_DOWN: return InputKind::pointer_down;
    case R_EVENT_MOUSE_UP: return InputKind::pointer_up;
    case R_EVENT_MOUSE_MOVE: return InputKind::pointer_move;
    case R_EVENT_MOUSE_WHEEL: return InputKind::wheel;
    case R_EVENT_KEY_DOWN: return InputKind::key_down;
    default: return InputKind::other;
  }
}

CanvasInput to_canvas_input(const REvent& e) {
  CanvasInput in;
  in.kind = to_kind(e.type);
  in.x = static_cast<double>(e.local_x);
  in.y = static_cast<double>(e.local_y);
  in.dx = static_cast<double>(e.delta_x);
  in.dy = static_cast<double>(e.delta_y);
  in.wheel_x = static_cast<double>(e.wheel_x);
  in.wheel_y = static_cast<double>(e.wheel_y);
  in.precise_wheel = e.wheel_precise != kFalse;
  in.button = to_button(e.button);
  in.modifiers = static_cast<std::uint8_t>(e.modifiers & 0x0FU);
  in.native_key = static_cast<int>(e.key);
  return in;
}

GesturePhase to_phase(RGesturePhase phase) {
  switch (phase) {
    case R_GESTURE_BEGIN: return GesturePhase::begin;
    case R_GESTURE_END: return GesturePhase::end;
    default: return GesturePhase::update;
  }
}

/// A pinch of two fingers on a touch screen or a trackpad (macOS:
/// magnifyWithEvent), in the viewport's logical coordinates.
CanvasInput to_canvas_input(const RGestureEvent& e) {
  CanvasInput in;
  in.kind = InputKind::pinch;
  in.x = static_cast<double>(e.local_x);
  in.y = static_cast<double>(e.local_y);
  in.dx = static_cast<double>(e.pan_x);
  in.dy = static_cast<double>(e.pan_y);
  in.scale = e.scale_delta > 0.0F ? static_cast<double>(e.scale_delta) : 1.0;
  in.phase = to_phase(e.phase);
  return in;
}

}  // namespace

// --- Runtime ---------------------------------------------------------------

struct Runtime::Callbacks {
  std::function<void()> queue_lock;
  std::function<void()> queue_unlock;
  std::function<void(double)> frame;

  static void lock(void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (cb->queue_lock) cb->queue_lock();
  }
  static void unlock(void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (cb->queue_unlock) cb->queue_unlock();
  }
  static void on_frame(double delta, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (cb->frame) cb->frame(delta);
  }
};

std::unique_ptr<Runtime> Runtime::start(RuntimeOptions options, std::string& error) {
  std::unique_ptr<Runtime> rt(new Runtime());
  rt->callbacks_ = std::make_unique<Callbacks>();
  rt->callbacks_->queue_lock = std::move(options.queue_lock);
  rt->callbacks_->queue_unlock = std::move(options.queue_unlock);
  Rerror_set_callback(on_raven_error);
  // The lock must be known before Raven starts its render thread.
  Rvulkan_set_queue_lock_callbacks(&Callbacks::lock, &Callbacks::unlock, rt->callbacks_.get());
  RInitDesc desc{};
  desc.backend = R_BACKEND_VULKAN;
  desc.app_name = options.app_name.c_str();
  desc.disable_vsync = options.vsync ? kFalse : kTrue;
  desc.enable_validation = options.validation ? kTrue : kFalse;
  if (Rinit_ex(&desc) == kFalse) {
    const char* detail = nullptr;
    RSeverity severity = R_SEVERITY_ERROR;
    const RError code = Rerror_get_last(&detail, &severity);
    error = std::string("Raven failed to start: ") + Rerror_to_string(code) + (detail != nullptr ? ": " : "") +
            (detail != nullptr ? detail : "");
    return nullptr;
  }
  return rt;
}

Runtime::~Runtime() {
  Rset_frame_callback(nullptr, nullptr);
  Rshutdown();
}

bool Runtime::wait_frame(double timeout_seconds) {
  return Rwait_frame(timeout_seconds) != kFalse;
}

void Runtime::quit() {
  Rquit(0);
}

void Runtime::set_frame_callback(std::function<void(double)> callback) {
  callbacks_->frame = std::move(callback);
  Rset_frame_callback(&Callbacks::on_frame, callbacks_.get());
}

render::VulkanDevice Runtime::vulkan_device() const {
  return render::VulkanDevice{Rvulkan_get_instance(), Rvulkan_get_physical_device(), Rvulkan_get_device(),
                              Rvulkan_get_graphics_queue(), Rvulkan_get_graphics_queue_family()};
}

std::string Runtime::version() const {
  return Rget_version();
}

bool Runtime::load_assets(const std::filesystem::path& asset_dir) {
  bool all = true;
  const FontFace faces[] = {{FontFamily::ui, 400}, {FontFamily::ui, 500}, {FontFamily::ui, 600},
                            {FontFamily::ui, 700}, {FontFamily::mono, 400}, {FontFamily::mono, 500}};
  for (const FontFace face : faces) {
    const auto font = load_font_file(asset_dir / font_file(face));
    if (font) {
      faces_[face_key(face)] = *font;
    } else {
      all = false;
    }
  }
  // The shapes and arrows Inter lacks (▾ ▸) come from JetBrains Mono.
  const WidgetId mono = font(FontFace{FontFamily::mono, 400});
  for (const auto& [key, id] : faces_) {
    if (key < 1000) Rfont_set_fallback(id, mono);
  }
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(asset_dir / "icons", ec)) {
    if (entry.path().extension() != ".svg") continue;
    const std::string svg = read_text_file(entry.path());
    // Rasterized at 3x so that the 18 px icons stay sharp on Retina displays.
    const Rid tex = Rtexture_create_from_svg(svg.data(), svg.size(), 3.0F);
    if (Rtexture_is_valid(tex) != kFalse) icons_[entry.path().stem().string()] = tex;
  }
  fonts_loaded_ = all;
  return all;
}

void Runtime::apply_theme(ThemeMode mode) {
  Rlook_set(mode == ThemeMode::light ? R_LOOK_RAVEN_LIGHT : R_LOOK_RAVEN_DARK);
  Rtheme_set_dark(mode == ThemeMode::dark ? kTrue : kFalse);
  Rtheme_set_material(R_MATERIAL_SOLID);
  apply_tokens(theme_tokens(mode));
  Rtheme_set_accent(theme_tokens(mode).accent.r, theme_tokens(mode).accent.g, theme_tokens(mode).accent.b);
  // Rtheme_set_accent derives focus and selection from the accent; the
  // forge selection is the neutral selected-row shade, focus stays ember.
  set_color(R_COLOR_SELECTION, theme_tokens(mode).row_selected);
  set_color(R_COLOR_FOCUS, theme_tokens(mode).accent);
  apply_metrics();
  const WidgetId ui = font(FontFace{FontFamily::ui, 400});
  Rtheme_set_font(ui);
  RAccessibilitySettings system{};
  Raccessibility_get_system_settings(&system);
  Raccessibility_set_reduce_motion(system.reduce_motion);
}

WidgetId Runtime::font(FontFace face) const {
  const auto exact = faces_.find(face_key(face));
  if (exact != faces_.end()) return exact->second;
  const auto regular = faces_.find(face_key(FontFace{face.family, 400}));
  if (regular != faces_.end()) {
    return face.weight >= 600 ? Rfont_get_variant(regular->second, R_FONT_BOLD) : regular->second;
  }
  return face.family == FontFamily::mono ? Rfont_get_monospace() : Rfont_get_default();
}

float Runtime::text_width(TextRole role, std::string_view text) const {
  const TextSpec spec = text_spec(role);
  float w = 0.0F;
  float h = 0.0F;
  Rfont_measure_text(font(spec.face), spec.size_px, str(text).c_str(), &w, &h);
  return w;
}

WidgetId Runtime::icon(std::string_view name) const {
  const auto it = icons_.find(name);
  return it != icons_.end() ? it->second : kNoWidget;
}

// --- Widgets ---------------------------------------------------------------

void announce(std::string_view text, bool assertive) {
  Raccessibility_announce(str(text).c_str(), assertive ? kTrue : kFalse);
}

Rect Widget::rect() const {
  Rect r;
  Rwidget_get_rect(id_, &r.x, &r.y, &r.width, &r.height);
  return r;
}

bool Widget::has_focus() const {
  return Rwidget_is_focused(id_) != kFalse;
}

void Widget::set_accessible_name(std::string_view name) {
  Rwidget_set_accessible_name(id_, str(name).c_str());
}

void Widget::set_tooltip(std::string_view text) {
  Rwidget_set_tooltip(id_, str(text).c_str());
}

void Widget::set_font(WidgetId font) {
  if (font != kNoWidget) {
    Rwidget_set_font(id_, font);
  }
}

void Widget::focus() {
  Rwidget_focus(id_);
}

void Widget::set_size(float width, float height) {
  Rwidget_set_size(id_, width, height);
}

void Widget::set_min_size(float width, float height) {
  Rwidget_set_min_size(id_, width, height);
}

void Widget::set_visible(bool visible) {
  Rwidget_set_visibility(id_, visible ? R_VISIBLE : R_COLLAPSED);
}

void Widget::set_enabled(bool enabled) {
  Rwidget_set_enabled(id_, enabled ? kTrue : kFalse);
}

void Widget::set_hit_testable(bool hit_testable) {
  Rwidget_set_hit_testable(id_, hit_testable ? kTrue : kFalse);
}

void Widget::set_font_size(float size) {
  Rwidget_set_metric(id_, R_METRIC_FONT_SIZE, size);
}

Box Box::vertical() {
  return Box(Rwidget_create(R_WIDGET_VBOX));
}

Box Box::horizontal() {
  return Box(Rwidget_create(R_WIDGET_HBOX));
}

void Box::add(const Widget& child, float fill) {
  Rwidget_add_child(id_, child.id());
  if (fill > 0.0F) {
    Rslot_set_fill(child.id(), fill);
  }
}

void Box::insert(const Widget& child, int index, float fill) {
  Rwidget_insert_child(id_, child.id(), index);  // moves it when it is already a child
  if (fill > 0.0F) {
    Rslot_set_fill(child.id(), fill);
  } else {
    Rslot_set_auto(child.id());  // a fill of 0 would be a zero-wide fill slot
  }
}

void Box::add_centered(const Widget& child) {
  Rwidget_add_child(id_, child.id());
  Rslot_set_valign(child.id(), R_VALIGN_CENTER);
}

void Box::clear() {
  Rwidget_clear_children(id_);
}

void Box::set_padding(float left, float top, float right, float bottom) {
  Rslot_set_padding(id_, left, top, right, bottom);
}

void Box::set_spacing(float spacing) {
  Rpanel_set_spacing(id_, spacing);
}

ListPanel ListPanel::create() {
  return ListPanel(Rwidget_create(R_WIDGET_LIST_VIEW));
}

void ListPanel::add_item(std::string_view text) {
  Rlist_view_add_item(id_, str(text).c_str());
}

void ListPanel::clear() {
  Rlist_view_clear_items(id_);
}

struct ListPanel::Select {
  std::function<void(int)> callback;
  bool quiet = false;  // clear_selection(): not a pick
  static void on_change(Rid, int index, void* user) {
    auto* self = static_cast<Select*>(user);
    if (!self->quiet && index >= 0 && self->callback) self->callback(index);
  }
};

void ListPanel::on_select(std::function<void(int)> callback) {
  if (!select_) select_ = std::make_shared<Select>();
  select_->callback = std::move(callback);
  Rlist_view_set_selection_changed_callback(id_, &Select::on_change, select_.get());
}

void ListPanel::clear_selection() {
  if (select_) select_->quiet = true;
  Rlist_view_set_selected(id_, -1);
  if (select_) select_->quiet = false;
}

void ListPanel::scroll_to_end() {
  const int count = Rlist_view_get_item_count(id_);
  if (count > 0) {
    Rlist_view_scroll_to_item(id_, count - 1);
  }
}

// --- Containers --------------------------------------------------------------

Panel Panel::create(const Rgba& background) {
  Panel panel(Rwidget_create(R_WIDGET_BORDER));
  Rborder_set_background(panel.id_, background.r, background.g, background.b, background.a);
  Rborder_set_outline(panel.id_, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F);
  Rborder_set_corner_radius(panel.id_, 0.0F, 0.0F, 0.0F, 0.0F);
  Rborder_set_padding(panel.id_, 0.0F, 0.0F, 0.0F, 0.0F);
  return panel;
}

void Panel::set_child(const Widget& child) {
  Rwidget_clear_children(id_);
  Rwidget_add_child(id_, child.id());
}

void Panel::set_fill(const Rgba& c) {
  Rborder_set_background(id_, c.r, c.g, c.b, c.a);
}

void Panel::set_padding(float left, float top, float right, float bottom) {
  Rborder_set_padding(id_, left, top, right, bottom);
}

Overlay Overlay::create() {
  return Overlay(Rwidget_create(R_WIDGET_OVERLAY));
}

void Overlay::add(const Widget& child) {
  Rwidget_add_child(id_, child.id());
}

ScrollBox ScrollBox::create() {
  ScrollBox box(Rwidget_create(R_WIDGET_SCROLL_BOX));
  Rscroll_box_set_orientation(box.id_, R_VERTICAL);
  return box;
}

void ScrollBox::add(const Widget& child) {
  Rwidget_add_child(id_, child.id());
}

void ScrollBox::clear() {
  Rwidget_clear_children(id_);
}

Tabs Tabs::create() {
  return Tabs(Rwidget_create(R_WIDGET_TAB_VIEW));
}

void Tabs::add(const Widget& page, std::string_view title) {
  Rtab_view_add_tab(id_, str(title).c_str(), page.id());
}

void Tabs::show(int index) {
  Rtab_view_set_active(id_, index);
}

// --- TextInput -----------------------------------------------------------------

struct TextInput::Callbacks {
  std::function<void(std::string)> change;
  std::function<void(std::string)> commit;
  static void on_change(Rid /*input*/, const char* text, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (cb->change) cb->change(text != nullptr ? text : "");
  }
  static void on_commit(Rid /*input*/, const char* text, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (cb->commit) cb->commit(text != nullptr ? text : "");
  }
};

TextInput::TextInput(WidgetId id) : Widget(id), callbacks_(std::make_unique<Callbacks>()) {}
TextInput::~TextInput() = default;

std::unique_ptr<TextInput> TextInput::create(std::string_view placeholder) {
  std::unique_ptr<TextInput> input(new TextInput(Rwidget_create(R_WIDGET_TEXT_INPUT)));
  Rtext_input_set_placeholder(input->id_, str(placeholder).c_str());
  Rtext_input_set_changed_callback(input->id_, &Callbacks::on_change, input->callbacks_.get());
  Rtext_input_set_committed_callback(input->id_, &Callbacks::on_commit, input->callbacks_.get());
  return input;
}

void TextInput::on_commit(std::function<void(std::string)> callback) {
  callbacks_->commit = std::move(callback);
}

void TextInput::on_change(std::function<void(std::string)> callback) {
  callbacks_->change = std::move(callback);
}

// --- Painter -------------------------------------------------------------------

namespace {
RPainter* raven(void* p) {
  return static_cast<RPainter*>(p);
}

/// Byte length of the UTF-8 sequence starting with `lead`.
std::size_t utf8_length(unsigned char lead) {
  if (lead < 0x80U) return 1;
  if ((lead >> 5U) == 0x6U) return 2;
  if ((lead >> 4U) == 0xEU) return 3;
  return 4;
}
}  // namespace

void Painter::fill(float x, float y, float w, float h, float radius, const Rgba& c) {
  Rdraw_rect(raven(p_), x, y, w, h, radius, c.r, c.g, c.b, c.a);
}

void Painter::stroke(float x, float y, float w, float h, float radius, float thickness, const Rgba& c) {
  Rdraw_rect_outline(raven(p_), x, y, w, h, radius, thickness, c.r, c.g, c.b, c.a);
}

void Painter::line(float x0, float y0, float x1, float y1, float thickness, const Rgba& c) {
  Rdraw_line(raven(p_), x0, y0, x1, y1, thickness, c.r, c.g, c.b, c.a);
}

void Painter::dashed_rect(float x, float y, float w, float h, float thickness, float dash, float gap, const Rgba& c) {
  const auto dashes = [&](float x0, float y0, float dx, float dy, float length) {
    for (float t = 0.0F; t < length; t += dash + gap) {
      const float e = std::min(t + dash, length);
      Rdraw_rect(raven(p_), x0 + dx * t, y0 + dy * t, std::max(dx * (e - t), thickness),
                 std::max(dy * (e - t), thickness), 0.0F, c.r, c.g, c.b, c.a);
    }
  };
  dashes(x, y, 1.0F, 0.0F, w);
  dashes(x, y + h - thickness, 1.0F, 0.0F, w);
  dashes(x, y, 0.0F, 1.0F, h);
  dashes(x + w - thickness, y, 0.0F, 1.0F, h);
}

void Painter::text(TextRole role, float x, float y, std::string_view text, const Rgba& c) {
  const TextSpec spec = text_spec(role);
  const Rid font = runtime_.font(spec.face);
  if (!spec.uppercase) {
    Rdraw_text(raven(p_), font, spec.size_px, x, y, str(text).c_str(), c.r, c.g, c.b, c.a);
    return;
  }
  // Raven has no letter spacing: draw the caps glyph by glyph, tracked.
  const std::string caps = upper(text);
  const float tracking = 0.08F * spec.size_px;
  for (std::size_t i = 0; i < caps.size();) {
    const std::size_t n = std::min(utf8_length(static_cast<unsigned char>(caps[i])), caps.size() - i);
    const std::string glyph = caps.substr(i, n);
    Rdraw_text(raven(p_), font, spec.size_px, x, y, glyph.c_str(), c.r, c.g, c.b, c.a);
    float w = 0.0F;
    float h = 0.0F;
    Rfont_measure_text(font, spec.size_px, glyph.c_str(), &w, &h);
    x += w + tracking;
    i += n;
  }
}

void Painter::text_centered_y(TextRole role, float x, float cy, std::string_view text, const Rgba& c) {
  this->text(role, x, cy - line_height(role) / 2.0F, text, c);
}

float Painter::text_width(TextRole role, std::string_view text) const {
  const TextSpec spec = text_spec(role);
  float w = 0.0F;
  float h = 0.0F;
  const std::string s = spec.uppercase ? upper(text) : std::string(text);
  Rfont_measure_text(runtime_.font(spec.face), spec.size_px, s.c_str(), &w, &h);
  if (spec.uppercase) {
    std::size_t glyphs = 0;
    for (std::size_t i = 0; i < s.size(); i += utf8_length(static_cast<unsigned char>(s[i]))) ++glyphs;
    w += 0.08F * spec.size_px * static_cast<float>(glyphs);
  }
  return w;
}

void Painter::text_px(TextRole role, float size_px, float x, float y, std::string_view text, const Rgba& c) {
  const TextSpec spec = text_spec(role);
  Rdraw_text(raven(p_), runtime_.font(spec.face), size_px, x, y, str(text).c_str(), c.r, c.g, c.b, c.a);
}

std::pair<float, float> Painter::measure_px(TextRole role, float size_px, std::string_view text) const {
  const TextSpec spec = text_spec(role);
  float w = 0.0F;
  float h = 0.0F;
  Rfont_measure_text(runtime_.font(spec.face), size_px, str(text).c_str(), &w, &h);
  return {w, h};
}

float Painter::line_height(TextRole role) const {
  const TextSpec spec = text_spec(role);
  float w = 0.0F;
  float h = 0.0F;
  Rfont_measure_text(runtime_.font(spec.face), spec.size_px, "Hg", &w, &h);
  return h;
}

void Painter::icon(std::string_view name, float x, float y, float size, const Rgba& c) {
  const WidgetId tex = runtime_.icon(name);
  if (tex != kNoWidget) {
    Rdraw_image(raven(p_), tex, x, y, size, size, c.r, c.g, c.b, c.a);
  }
}

void Painter::push_clip(float x, float y, float w, float h) {
  Rdraw_push_clip_rect(raven(p_), x, y, w, h);
}

void Painter::pop_clip() {
  Rdraw_pop_clip_rect(raven(p_));
}

// --- Painted -------------------------------------------------------------------

struct Painted::Callbacks {
  const Runtime* runtime = nullptr;
  const Painted* widget = nullptr;
  StatefulPaintFn paint;
  std::function<void(float, float)> click;
  std::function<void(float, float)> context;
  std::function<void(float, float)> hold;
  std::function<std::optional<DragData>(float, float)> drag;
  DragGesture gesture;  // on_drag: the left press and whether it became a drag
  float gesture_local_x = 0.0F;
  float gesture_local_y = 0.0F;
  // on_hold: the press waiting for kHoldDelay (a one-shot timer) and where.
  PressHold press;
  Rid timer = kNoWidget;
  Rid window = kNoWidget;
  float press_x = 0.0F;  // window point under the widget's bottom-left corner
  float press_y = 0.0F;
  float local_x = 0.0F;
  float local_y = 0.0F;

  Callbacks() = default;
  Callbacks(const Callbacks&) = delete;
  Callbacks& operator=(const Callbacks&) = delete;
  ~Callbacks() { stop_timer(); }

  void stop_timer() {
    if (timer != kNoWidget && Rtimer_is_valid(timer) != kFalse) Rtimer_destroy(timer);
    timer = kNoWidget;
  }

  static void on_hold_timer(Rid /*timer*/, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    cb->timer = kNoWidget;  // a one-shot timer deletes itself
    const bool down = cb->window != kNoWidget && Rwindow_is_mouse_button_down(cb->window, R_MOUSE_LEFT) != kFalse;
    if (cb->press.elapse(down) && cb->hold) cb->hold(cb->press_x, cb->press_y);
  }

  /// on_hold's left button: a click on release before the delay, the hold
  /// action once the delay passed with the button down.
  bool hold_event(Rid id, const REvent& e) {
    if (e.type == R_EVENT_MOUSE_DOWN && e.button == R_MOUSE_LEFT) {
      float x = 0.0F;
      float y = 0.0F;
      float w = 0.0F;
      float h = 0.0F;
      Rwidget_get_rect(id, &x, &y, &w, &h);
      press_x = x;
      press_y = y + h;
      local_x = e.local_x;
      local_y = e.local_y;
      window = e.window;
      press.press();
      stop_timer();
      timer = Rtimer_create(std::chrono::duration<double>(kHoldDelay).count(), kFalse, &on_hold_timer, this);
      return true;
    }
    if (e.type == R_EVENT_MOUSE_UP && e.button == R_MOUSE_LEFT) {
      stop_timer();
      if (press.release()) click(local_x, local_y);
      return true;
    }
    return false;
  }

  /// on_drag's left button: the press, the move that starts Raven's drag
  /// (once), and a click on release when no drag started.
  bool drag_event(Rid id, const REvent& e) {
    if (e.type == R_EVENT_MOUSE_DOWN && e.button == R_MOUSE_LEFT) {
      gesture.press(e.x, e.y);
      gesture_local_x = e.local_x;
      gesture_local_y = e.local_y;
      return true;
    }
    // A Raven custom widget does not capture the mouse: a move that leaves
    // it with the button down starts the drag too.
    const bool left = e.type == R_EVENT_MOUSE_LEAVE && Rwindow_is_mouse_button_down(e.window, R_MOUSE_LEFT) != kFalse;
    if ((e.type == R_EVENT_MOUSE_MOVE && gesture.move(e.x, e.y)) || (left && gesture.leave())) {
      if (const std::optional<DragData> data = drag(gesture.press_x(), gesture.press_y())) {
        (void)Rdrag_begin(id, data->type.c_str(), data->data.c_str(), data->label.c_str());
      }
      return e.type != R_EVENT_MOUSE_LEAVE;
    }
    if (e.type == R_EVENT_MOUSE_UP && e.button == R_MOUSE_LEFT) {
      if (gesture.release()) click(gesture_local_x, gesture_local_y);
      return true;
    }
    return false;
  }

  static bool context_event(const REvent& e) {
    if (e.type == R_EVENT_MOUSE_DOWN) return e.button == R_MOUSE_RIGHT;
    return e.type == R_EVENT_KEY_DOWN &&
           (e.key == R_KEY_MENU || (e.key == R_KEY_F10 && (e.modifiers & static_cast<Ruint>(R_MOD_SHIFT)) != 0U));
  }

  static void on_paint(Rid /*widget*/, RPainter* painter, float width, float height, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (!cb->paint) return;
    Painter p(painter, *cb->runtime);
    cb->paint(p, width, height, *cb->widget);
  }
  static int on_event(Rid widget, const REvent* e, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (e->type == R_EVENT_MOUSE_ENTER || e->type == R_EVENT_MOUSE_LEAVE) {
      Rwidget_invalidate(widget);
      if (cb->drag && cb->click) (void)cb->drag_event(widget, *e);
      return kFalse;
    }
    if (cb->context && context_event(*e)) {
      float x = e->x;
      float y = e->y;
      if (e->type == R_EVENT_KEY_DOWN) {  // under the widget, from the keyboard
        float w = 0.0F;
        float h = 0.0F;
        Rwidget_get_rect(widget, &x, &y, &w, &h);
        y += h;
      }
      cb->context(x, y);
      return kTrue;
    }
    if (!cb->click || Rwidget_is_enabled(widget) == kFalse) return kFalse;
    if (cb->hold && cb->hold_event(widget, *e)) return kTrue;
    if (cb->drag && cb->drag_event(widget, *e)) return kTrue;
    if (e->type == R_EVENT_MOUSE_DOWN && e->button == R_MOUSE_LEFT) {
      cb->click(e->local_x, e->local_y);
      return kTrue;
    }
    const bool activate = e->key == R_KEY_ENTER || e->key == R_KEY_KP_ENTER || e->key == R_KEY_SPACE;
    if (e->type == R_EVENT_KEY_DOWN && activate) {
      cb->click(-1.0F, -1.0F);
      return kTrue;
    }
    return kFalse;
  }
};

Painted::Painted(WidgetId id) : Widget(id), callbacks_(std::make_unique<Callbacks>()) {}
Painted::~Painted() = default;

std::unique_ptr<Painted> Painted::create(const Runtime& runtime, PaintFn paint) {
  if (!paint) return create_stateful(runtime, nullptr);
  return create_stateful(runtime, [paint = std::move(paint)](Painter& p, float w, float h, const Painted&) {
    paint(p, w, h);
  });
}

std::unique_ptr<Painted> Painted::create_stateful(const Runtime& runtime, StatefulPaintFn paint) {
  std::unique_ptr<Painted> w(new Painted(Rwidget_create(R_WIDGET_CUSTOM)));
  w->callbacks_->runtime = &runtime;
  w->callbacks_->widget = w.get();
  w->callbacks_->paint = std::move(paint);
  Rcustom_set_paint_callback(w->id_, &Callbacks::on_paint, w->callbacks_.get());
  Rwidget_set_event_callback(w->id_, &Callbacks::on_event, w->callbacks_.get());
  return w;
}

void Painted::on_click(std::function<void(float, float)> callback) {
  callbacks_->click = std::move(callback);
  Rwidget_set_focusable(id_, kTrue);
  Rwidget_set_cursor(id_, R_CURSOR_HAND);
}

void Painted::on_context(std::function<void(float, float)> callback) {
  callbacks_->context = std::move(callback);
}

void Painted::on_hold(std::function<void(float, float)> callback) {
  callbacks_->hold = std::move(callback);
}

void Painted::on_drag(std::function<std::optional<DragData>(float, float)> callback) {
  callbacks_->drag = std::move(callback);
}

void Painted::set_role(Role role) {
  Rwidget_set_accessible_role(id_, to_role(role));
}

void Painted::set_accessible_description(std::string_view text) {
  Rwidget_set_accessible_description(id_, str(text).c_str());
}

bool Painted::hovered() const {
  return Rwidget_is_hovered(id_) != kFalse;
}

bool Painted::focused() const {
  return Rwidget_is_focused(id_) != kFalse;
}

void Painted::invalidate() {
  Rwidget_invalidate(id_);
}

// --- DropZone ---------------------------------------------------------------

struct DropZone::State {
  Rid widget = kNoWidget;
  Callback callback;

  static int on_drop(Rid /*target*/, const RDropEvent* e, void* self) {
    auto* state = static_cast<State*>(self);
    if (!state->callback || e->drag_type == nullptr) return kFalse;  // another application's drop
    Phase phase = Phase::move;
    switch (e->phase) {
      case R_DROP_ENTER: phase = Phase::enter; break;
      case R_DROP_LEAVE: phase = Phase::leave; break;
      case R_DROP_DROPPED: phase = Phase::drop; break;
      default: break;
    }
    const std::string_view data = e->text != nullptr ? std::string_view(e->text) : std::string_view();
    return state->callback(phase, e->x, e->y, e->drag_type, data) ? kTrue : kFalse;
  }
};

DropZone::DropZone(const Widget& widget, Callback callback) : state_(std::make_unique<State>()) {
  state_->widget = widget.id();
  state_->callback = std::move(callback);
  Rwidget_set_drop_callback(state_->widget, &State::on_drop, state_.get());
}

DropZone::~DropZone() {
  if (Rwidget_is_valid(state_->widget) != kFalse) Rwidget_set_drop_callback(state_->widget, nullptr, nullptr);
}

// --- CommandBar ------------------------------------------------------------

struct CommandBar::Callbacks {
  std::function<void(std::string)> commit;
  std::function<std::string(bool)> history;
  std::function<std::optional<std::string>(std::string)> complete;

  static void on_commit(Rid /*input*/, const char* text, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (cb->commit) cb->commit(text != nullptr ? text : "");
  }
  static int on_event(Rid input, const REvent* e, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (e->type != R_EVENT_KEY_DOWN) return kFalse;
    if (e->key == R_KEY_TAB && e->modifiers == 0 && cb->complete) {
      const char* text = Rtext_input_get_text(input);
      if (const auto done = cb->complete(text != nullptr ? text : "")) Rtext_input_set_text(input, done->c_str());
      return kTrue;  // Tab completes; it never moves the focus out of the command bar
    }
    if (!cb->history || (e->key != R_KEY_UP && e->key != R_KEY_DOWN)) return kFalse;
    Rtext_input_set_text(input, cb->history(e->key == R_KEY_UP).c_str());
    return kTrue;
  }
};

CommandBar::CommandBar(WidgetId id) : Widget(id), callbacks_(std::make_unique<Callbacks>()) {}
CommandBar::~CommandBar() = default;

std::unique_ptr<CommandBar> CommandBar::create(std::string_view placeholder) {
  std::unique_ptr<CommandBar> bar(new CommandBar(Rwidget_create(R_WIDGET_TEXT_INPUT)));
  Rtext_input_set_placeholder(bar->id_, str(placeholder).c_str());
  Rtext_input_set_committed_callback(bar->id_, &Callbacks::on_commit, bar->callbacks_.get());
  Rwidget_set_event_callback(bar->id_, &Callbacks::on_event, bar->callbacks_.get());
  return bar;
}

void CommandBar::on_commit(std::function<void(std::string)> callback) {
  callbacks_->commit = std::move(callback);
}

void CommandBar::on_history(std::function<std::string(bool)> callback) {
  callbacks_->history = std::move(callback);
}

void CommandBar::set_text(std::string_view text) {
  Rtext_input_set_text(id_, str(text).c_str());
}

void CommandBar::on_complete(std::function<std::optional<std::string>(std::string)> callback) {
  callbacks_->complete = std::move(callback);
}

// --- Viewport --------------------------------------------------------------

struct Viewport::Callbacks {
  std::function<void(const ViewportDraw&)> draw;
  std::function<bool(const CanvasInput&)> input;
  std::function<void(int, int)> resize;

  static void on_draw(const RViewportDrawInfo* info, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (!cb->draw || info->backend != R_BACKEND_VULKAN) {
      return;
    }
    cb->draw(ViewportDraw{info->command_buffer, info->render_pass, info->x, info->y, info->width, info->height,
                          info->frame_index});
  }
  static int on_event(Rid viewport, const REvent* e, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (e->type == R_EVENT_MOUSE_DOWN) {
      Rwidget_focus(viewport);  // so that the keys reach the canvas
    }
    return cb->input && cb->input(to_canvas_input(*e)) ? kTrue : kFalse;
  }
  // Pinches only: a swipe of three fingers keeps navigating back / forward.
  static int on_gesture(Rid /*viewport*/, const RGestureEvent* e, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (e->kind != R_GESTURE_PINCH || e->cancelled != kFalse) return kFalse;
    return cb->input && cb->input(to_canvas_input(*e)) ? kTrue : kFalse;
  }
  static void on_resize(Rid /*viewport*/, int width, int height, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (cb->resize) cb->resize(width, height);
  }
};

Viewport::Viewport(WidgetId id) : Widget(id), callbacks_(std::make_unique<Callbacks>()) {}

// The owner destroys the window first (Window::destroy), which stops the
// draw callback; the widget handle is then already gone.
Viewport::~Viewport() = default;

std::unique_ptr<Viewport> Viewport::create() {
  std::unique_ptr<Viewport> vp(new Viewport(Rwidget_create(R_WIDGET_VIEWPORT)));
  Rwidget_set_focusable(vp->id_, kTrue);
  Rviewport_set_draw_callback(vp->id_, &Callbacks::on_draw, vp->callbacks_.get());
  Rwidget_set_event_callback(vp->id_, &Callbacks::on_event, vp->callbacks_.get());
  // The canvas zooms itself: it takes the pinch (trackpad or touch screen)
  // instead of letting Raven zoom or scroll a parent with it.
  Rwidget_set_gesture_callback(vp->id_, &Callbacks::on_gesture, vp->callbacks_.get());
  Rviewport_set_resize_callback(vp->id_, &Callbacks::on_resize, vp->callbacks_.get());
  return vp;
}

void Viewport::on_draw(std::function<void(const ViewportDraw&)> callback) {
  callbacks_->draw = std::move(callback);
}

void Viewport::on_input(std::function<bool(const CanvasInput&)> callback) {
  callbacks_->input = std::move(callback);
}

void Viewport::on_resize(std::function<void(int, int)> callback) {
  callbacks_->resize = std::move(callback);
}

void Viewport::set_realtime(bool realtime) {
  Rviewport_set_realtime(id_, realtime ? kTrue : kFalse);
}

void Viewport::invalidate() {
  Rwidget_invalidate(id_);
}

void Viewport::set_cursor(CursorShape shape) {
  RCursor cursor = R_CURSOR_ARROW;
  switch (shape) {
    case CursorShape::arrow: cursor = R_CURSOR_ARROW; break;
    case CursorShape::crosshair: cursor = R_CURSOR_CROSSHAIR; break;
    case CursorShape::ibeam: cursor = R_CURSOR_IBEAM; break;
    case CursorShape::move: cursor = R_CURSOR_MOVE; break;
    case CursorShape::resize_nesw: cursor = R_CURSOR_RESIZE_NESW; break;
    case CursorShape::resize_nwse: cursor = R_CURSOR_RESIZE_NWSE; break;
  }
  Rwidget_set_cursor(id_, cursor);
}

// --- PopupMenu ---------------------------------------------------------------

struct PopupMenu::State {
  struct Slot {
    State* state = nullptr;
    std::size_t index = 0;
  };
  Rid menu = kNoWidget;
  std::vector<std::unique_ptr<Slot>> slots;
  std::function<void(std::size_t)> on_choose;
  static void clicked(Rid, void* user) {
    const auto* slot = static_cast<Slot*>(user);
    if (slot->state->on_choose) slot->state->on_choose(slot->index);
  }
};

PopupMenu::PopupMenu() : state_(std::make_unique<State>()) {
  state_->menu = Rwidget_create(R_WIDGET_MENU);
}

PopupMenu::~PopupMenu() {
  if (state_->menu != kNoWidget) Rwidget_destroy(state_->menu);
}

void PopupMenu::set_items(std::string_view title, const std::vector<MenuItem>& items,
                          std::function<void(std::size_t)> on_choose) {
  Rwidget_clear_children(state_->menu);
  state_->slots.clear();
  state_->on_choose = std::move(on_choose);
  Rmenu_set_title(state_->menu, str(title).c_str());
  Rwidget_set_accessible_name(state_->menu, str(title).c_str());
  for (std::size_t i = 0; i < items.size(); ++i) {
    const MenuItem& item = items[i];
    if (item.separator_before && i > 0) Rmenu_add_separator(state_->menu);
    const Rid r = Rmenu_add_item(state_->menu, item.label.c_str());
    if (item.checked) {
      Rmenu_item_set_checkable(r, kTrue);
      Rmenu_item_set_checked(r, kTrue);
    }
    Rwidget_set_enabled(r, item.enabled ? kTrue : kFalse);
    state_->slots.push_back(std::make_unique<State::Slot>(State::Slot{state_.get(), i}));
    Rmenu_item_set_clicked_callback(r, &State::clicked, state_->slots.back().get());
  }
}

void PopupMenu::open(const Window& window, float x, float y) {
  if (state_->slots.empty()) return;
  Rmenu_open(state_->menu, window.id(), x, y);
}

std::size_t PopupMenu::size() const noexcept {
  return state_->slots.size();
}

// --- Window ----------------------------------------------------------------

/// The window's event hook (Window::install_key_hook).
struct Window::KeyHook {
  TextKeyFilter filter;
  std::function<bool(int, std::uint8_t)> shortcut;
  std::function<void(float, float)> drag_release;

  explicit KeyHook(bool repeat_text) : filter(repeat_text) {}

  static bool text_focus(Rid window) {
    const Rid focus = Rwindow_get_focused_widget(window);
    return focus != kNoWidget && Rwidget_get_type(focus) == R_WIDGET_TEXT_INPUT;
  }

  static int on_event(Rid window, const REvent* e, void* user) {
    auto* self = static_cast<KeyHook*>(user);
    switch (e->type) {
      case R_EVENT_TEXT:
        return self->filter.text(e->codepoint) ? kFalse : kTrue;
      case R_EVENT_KEY_UP:
        self->filter.key_up(static_cast<int>(e->key));
        return kFalse;
      case R_EVENT_MOUSE_UP:
        if (e->button == R_MOUSE_LEFT && self->drag_release && Rdrag_is_active(window) != kFalse) {
          self->drag_release(e->x, e->y);
        }
        return kFalse;
      case R_EVENT_KEY_DOWN: {
        const std::string again = self->filter.key_down(static_cast<int>(e->key), e->repeat != 0, text_focus(window));
        if (!again.empty()) Rwindow_inject_text(window, again.c_str());
        const bool used = self->shortcut && e->repeat == 0 &&
                          self->shortcut(static_cast<int>(e->key), static_cast<std::uint8_t>(e->modifiers));
        return used ? kTrue : kFalse;
      }
      default:
        return kFalse;
    }
  }
};

void Window::install_key_hook(std::function<bool(int, std::uint8_t)> shortcut) {
  key_hook_ = std::make_unique<KeyHook>(window_system() == "cocoa");
  key_hook_->shortcut = std::move(shortcut);
  Rwindow_set_event_hook(id_, &KeyHook::on_event, key_hook_.get());
}

void Window::on_drag_release(std::function<void(float, float)> release) {
  if (key_hook_) key_hook_->drag_release = std::move(release);
}

void Window::close_popups() {
  Rwindow_close_popups(id_);
}


struct Window::CloseHook {
  std::function<void()> callback;
  static int on_close(Rid /*window*/, void* self) {
    auto* hook = static_cast<CloseHook*>(self);
    if (hook->callback) hook->callback();
    return kFalse;  // the owner destroys the window
  }
};

void Window::set_topmost(bool topmost) {
  if (id_ != kNoWidget) (void)Rwindow_set_topmost(id_, topmost ? kTrue : kFalse);
}

void Window::on_close_request(std::function<void()> callback) {
  if (!close_hook_) close_hook_ = std::make_unique<CloseHook>();
  close_hook_->callback = std::move(callback);
  if (id_ != kNoWidget) Rwindow_set_close_callback(id_, &CloseHook::on_close, close_hook_.get());
}

Window::Window(std::string_view title, int width, int height)
    : id_(Rwindow_create(str(title).c_str(), width, height, R_WINDOW_DEFAULT)) {}

Window::~Window() {
  destroy();
}

void Window::destroy() {
  // Deleting the window waits for its last frames: no draw callback runs after.
  if (id_ != kNoWidget && valid()) {
    Rwindow_set_event_hook(id_, nullptr, nullptr);
    if (close_hook_) Rwindow_set_close_callback(id_, nullptr, nullptr);
    Rwindow_destroy(id_);
  }
  id_ = kNoWidget;
}

void Window::set_content(const Widget& content) {
  Rwindow_set_content(id_, content.id());
}

bool Window::valid() const {
  return id_ != kNoWidget && Rwindow_is_valid(id_) != kFalse;
}

void Window::request_redraw() {
  Rwindow_request_redraw(id_);
}

float Window::scale() const {
  return Rwindow_get_scale(id_);
}

int Window::logical_width() const {
  int width = 0;
  int height = 0;
  Rwindow_get_size(id_, &width, &height);
  return width;
}

FrameStats Window::frame_stats() const {
  RFrameStats s{};
  Rwindow_get_frame_stats(id_, &s);
  return FrameStats{s.layout_ms, s.paint_ms, s.render_ms, s.widget_count, s.draw_call_count, s.frame_index};
}

std::string Window::window_system() const {
  RNativeWindow native{};
  if (id_ == kNoWidget || Rwindow_get_native_handle(id_, &native) == kFalse) {
    return "none";
  }
  switch (native.system) {
    case R_WINDOW_SYSTEM_X11: return "x11";
    case R_WINDOW_SYSTEM_WAYLAND: return "wayland";
    case R_WINDOW_SYSTEM_COCOA: return "cocoa";
    case R_WINDOW_SYSTEM_WIN32: return "win32";
    default: return "none";
  }
}

void Window::set_title(std::string_view title) {
  Rwindow_set_title(id_, str(title).c_str());
}

void Window::set_min_size(int width, int height) {
  Rwindow_set_min_size(id_, width, height);
}

void Window::set_clear_color(const Rgba& c) {
  Rwindow_set_clear_color(id_, c.r, c.g, c.b, c.a);
}

namespace {

void close_dialog_with(Rid button, void* answer) {
  Rdialog_close(button, static_cast<int>(reinterpret_cast<std::intptr_t>(answer)));
}

// A label color left to the theme.
constexpr Rgba kThemeColor{0.0F, 0.0F, 0.0F, 0.0F};

void set_label_color(Rid label, const Rgba& c) {
  Rlabel_set_color(label, c.r, c.g, c.b, c.a);
}

Rid dialog_label(const std::string& text, const Rgba& color, WidgetId font, float size) {
  const Rid label = Rlabel_create(text.c_str());
  if (color.a > 0.0F) set_label_color(label, color);
  if (font != kNoWidget) Rlabel_set_font(label, font);
  if (size > 0.0F) Rlabel_set_text_size(label, size);
  return label;
}

// The icon of an M4 alert in a 32 px bordered box (board M4-Script-Consent note 1).
Rid dialog_icon(const DialogLook& look) {
  const Rid box = Rwidget_create(R_WIDGET_BORDER);
  Rborder_set_padding(box, 7.0F, 7.0F, 7.0F, 7.0F);
  Rborder_set_background(box, look.inset.r, look.inset.g, look.inset.b, look.inset.a);
  Rborder_set_outline(box, 1.0F, look.border.r, look.border.g, look.border.b, look.border.a);
  Rborder_set_corner_radius(box, 8.0F, 8.0F, 8.0F, 8.0F);
  const Rid image = Rwidget_create(R_WIDGET_IMAGE);
  Rimage_set_texture(image, look.icon);
  Rimage_set_tint_color(image, look.icon_color.r, look.icon_color.g, look.icon_color.b, look.icon_color.a);
  Rwidget_set_size(image, 18.0F, 18.0F);
  Rwidget_add_child(box, image);
  Rwidget_set_size(box, 32.0F, 32.0F);
  return box;
}

Rid dialog_chip(const DialogDetail& detail, const DialogLook& look) {
  const bool ok = detail.tone == DialogDetail::Tone::ok;
  const bool warning = detail.tone == DialogDetail::Tone::warning;
  const Rgba fill = ok ? look.ok : warning ? look.warning : look.border;
  const Rgba text = ok ? look.ok_text : warning ? look.warning_text : look.muted;
  const Rid chip = Rwidget_create(R_WIDGET_BORDER);
  Rborder_set_padding(chip, 6.0F, 1.0F, 6.0F, 1.0F);
  Rborder_set_background(chip, fill.r, fill.g, fill.b, 0.14F);
  Rborder_set_corner_radius(chip, 9.0F, 9.0F, 9.0F, 9.0F);
  Rwidget_add_child(chip, dialog_label(detail.chip, text, kNoWidget, 10.0F));
  return chip;
}

// One row of the detail block: key | value, chip, note.
void add_dialog_detail(Rid grid, int row, const DialogDetail& detail, const DialogLook& look) {
  const Rid key = dialog_label(detail.key, look.muted, kNoWidget, 12.0F);
  Rwidget_add_child(grid, key);
  Rslot_set_grid(key, 0, row);
  Rslot_set_valign(key, R_VALIGN_CENTER);
  const Rid cell = Rwidget_create(R_WIDGET_HBOX);
  Rpanel_set_spacing(cell, 6.0F);
  if (!detail.value.empty()) Rwidget_add_child(cell, dialog_label(detail.value, kThemeColor, look.mono, 12.0F));
  if (!detail.chip.empty()) Rwidget_add_child(cell, dialog_chip(detail, look));
  if (!detail.note.empty()) Rwidget_add_child(cell, dialog_label(detail.note, look.muted, look.mono, 12.0F));
  if (!detail.tooltip.empty()) Rwidget_set_tooltip(cell, detail.tooltip.c_str());
  Rwidget_add_child(grid, cell);
  Rslot_set_grid(cell, 1, row);
  Rslot_set_valign(cell, R_VALIGN_CENTER);
}

Rid dialog_details(const std::vector<DialogDetail>& details, const DialogLook& look) {
  const Rid block = Rwidget_create(R_WIDGET_BORDER);
  Rborder_set_padding(block, 12.0F, 10.0F, 12.0F, 10.0F);
  Rborder_set_background(block, look.inset.r, look.inset.g, look.inset.b, look.inset.a);
  Rborder_set_outline(block, 1.0F, look.border.r, look.border.g, look.border.b, look.border.a);
  Rborder_set_corner_radius(block, 8.0F, 8.0F, 8.0F, 8.0F);
  const Rid grid = Rwidget_create(R_WIDGET_GRID);
  Rpanel_set_spacing(grid, 8.0F);
  Rgrid_set_column_fill(grid, 1, 1.0F);
  for (std::size_t i = 0; i < details.size(); ++i) add_dialog_detail(grid, static_cast<int>(i), details[i], look);
  Rwidget_add_child(block, grid);
  return block;
}

// The answer buttons: `leading` of them on the left, the others after a gap.
Rid dialog_buttons(const PromptDialog& dialog, Rid& primary) {
  const Rid row = Rwidget_create(R_WIDGET_HBOX);
  Rpanel_set_spacing(row, 8.0F);
  const std::size_t leading = std::min(dialog.leading_buttons, dialog.buttons.size());
  for (std::size_t i = 0; i < dialog.buttons.size(); ++i) {
    if (i == leading) {
      const Rid spacer = Rwidget_create(R_WIDGET_SPACER);
      Rwidget_add_child(row, spacer);
      Rslot_set_fill(spacer, 1.0F);
    }
    const DialogButton& b = dialog.buttons[i];
    const Rid button = Rbutton_create(b.label.c_str());
    Rbutton_set_style(button, b.primary ? R_BUTTON_PRIMARY : R_BUTTON_DEFAULT);
    Rwidget_set_accessible_name(button, b.label.c_str());
    Rbutton_set_clicked_callback(button, &close_dialog_with,
                                 reinterpret_cast<void*>(static_cast<std::intptr_t>(b.answer + 1)));
    Rwidget_add_child(row, button);
    if (b.primary) primary = button;
  }
  return row;
}

// Title, text, details and buttons in one column.
Rid dialog_column(const PromptDialog& dialog, const DialogLook& look, Rid& primary) {
  const Rid column = Rwidget_create(R_WIDGET_VBOX);
  Rpanel_set_spacing(column, dialog.details.empty() ? 12.0F : 10.0F);
  Rwidget_add_child(column, dialog_label(dialog.title, kThemeColor, look.title_font, 15.0F));
  if (dialog.text != dialog.title) {
    const Rid text = dialog_label(dialog.text, look.body, kNoWidget, 0.0F);
    Rlabel_set_wrap(text, kTrue);
    Rwidget_add_child(column, text);
  }
  if (!dialog.details.empty()) Rwidget_add_child(column, dialog_details(dialog.details, look));
  const Rid buttons = dialog_buttons(dialog, primary);
  Rwidget_add_child(column, buttons);
  if (!dialog.details.empty()) Rslot_set_padding(buttons, 0.0F, 4.0F, 0.0F, 0.0F);
  return column;
}

}  // namespace

int Window::run_dialog(const PromptDialog& dialog, bool wait, const DialogLook& look) const {
  Rid primary = kNoWidget;
  const Rid column = dialog_column(dialog, look, primary);
  Rid content = column;
  if (!dialog.icon.empty() && look.icon != kNoWidget) {
    content = Rwidget_create(R_WIDGET_HBOX);
    Rpanel_set_spacing(content, 14.0F);
    const Rid icon = dialog_icon(look);
    Rwidget_add_child(content, icon);
    Rslot_set_valign(icon, R_VALIGN_TOP);
    Rwidget_add_child(content, column);
    Rslot_set_fill(column, 1.0F);
  }
  if (dialog.width > 0.0F) {
    Rwidget_set_size(content, dialog.width, -1.0F);
  } else {
    Rwidget_set_min_size(content, 420.0F, 0.0F);
  }
  Rwidget_set_accessible_name(content, dialog.title.c_str());
  if (!wait) {
    Rdialog_open(id_, content, nullptr, nullptr);
    if (primary != kNoWidget) Rwidget_focus(primary);
    return 0;
  }
  if (primary != kNoWidget) Rwidget_focus(primary);
  return Rdialog_run(id_, content);
}

std::optional<std::string> Window::open_file_dialog(std::string_view title, std::string_view filter_name,
                                                    std::string_view patterns) const {
  const std::string t = str(title);
  const std::string name = str(filter_name);
  const std::string pat = str(patterns);
  const RFileFilter filters[] = {{name.c_str(), pat.c_str()}, {"All files", "*"}};
  RFileDialogDesc desc{};
  desc.parent = id_;
  desc.title = t.c_str();
  desc.filters = filters;
  desc.filter_count = 2;
  const char* path = Rfile_dialog_open(&desc);
  if (path == nullptr) {
    return std::nullopt;
  }
  return std::string(path);
}

void Window::inject_trackpad_pinch(float x, float y, double factor, int steps) {
  if (!(factor > 0.0) || steps < 1) return;
  const auto step = static_cast<float>(std::pow(factor, 1.0 / steps));
  Rwindow_inject_trackpad_gesture(id_, R_GESTURE_PINCH, R_GESTURE_BEGIN, x, y, 1.0F, 0.0F, 0.0F, 0.0F, 2);
  for (int i = 0; i < steps; ++i) {
    Rwindow_inject_trackpad_gesture(id_, R_GESTURE_PINCH, R_GESTURE_UPDATE, x, y, step, 0.0F, 0.0F, 0.0F, 2);
  }
  Rwindow_inject_trackpad_gesture(id_, R_GESTURE_PINCH, R_GESTURE_END, x, y, 1.0F, 0.0F, 0.0F, 0.0F, 2);
}

void Window::inject_left_drag(float x0, float y0, float x1, float y1, int steps) {
  if (steps < 1) return;
  Rwindow_inject_mouse_move(id_, x0, y0);
  Rwindow_inject_mouse_button(id_, R_MOUSE_LEFT, kTrue);
  for (int i = 1; i <= steps; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(steps);
    Rwindow_inject_mouse_move(id_, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
  }
  Rwindow_inject_mouse_button(id_, R_MOUSE_LEFT, kFalse);
}

bool Window::drag_active() const {
  return Rdrag_is_active(id_) != kFalse;
}

void Window::inject_input(const InputStep& step) {
  const auto button = static_cast<RMouseButton>(step.button);
  switch (step.kind) {
    case ScriptKind::move:
      Rwindow_inject_mouse_move(id_, step.x, step.y);
      break;
    case ScriptKind::down:
    case ScriptKind::up:
      Rwindow_inject_mouse_move(id_, step.x, step.y);
      Rwindow_inject_mouse_button(id_, button, step.kind == ScriptKind::down ? kTrue : kFalse);
      break;
    case ScriptKind::key:
      Rwindow_inject_key(id_, static_cast<RKey>(step.key), kTrue, step.modifiers);
      Rwindow_inject_key(id_, static_cast<RKey>(step.key), kFalse, step.modifiers);
      break;
    case ScriptKind::text:
      Rwindow_inject_text(id_, step.text.c_str());
      break;
    case ScriptKind::wait:
      break;
  }
}

void Window::inject_left_press(float x, float y) {
  Rwindow_inject_mouse_move(id_, x, y);
  Rwindow_inject_mouse_button(id_, R_MOUSE_LEFT, kTrue);
}

bool Window::capture_ppm(const std::string& path) const {
  int width = 0;
  int height = 0;
  std::uint8_t* pixels = Rwindow_capture(id_, &width, &height);
  if (pixels == nullptr) {
    return false;
  }
  std::FILE* file = std::fopen(path.c_str(), "wb");
  if (file != nullptr) {
    std::fprintf(file, "P6\n%d %d\n255\n", width, height);
    const auto count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    for (std::size_t i = 0; i < count; ++i) {
      std::fwrite(pixels + i * 4, 1, 3, file);  // RGBA -> RGB
    }
    std::fclose(file);
  }
  Rpixels_free(pixels);
  return file != nullptr;
}

// --- M3: tasks, tables, toasts, Dock progress ------------------------------------

namespace {

void run_posted(void* user) {
  std::unique_ptr<std::function<void()>> task(static_cast<std::function<void()>*>(user));
  if (*task) (*task)();
}

RToastKind toast_kind(StatusKind kind) {
  switch (kind) {
    case StatusKind::success: return R_TOAST_SUCCESS;
    case StatusKind::warning: return R_TOAST_WARNING;
    case StatusKind::error: return R_TOAST_ERROR;
    case StatusKind::info: return R_TOAST_INFO;
  }
  return R_TOAST_INFO;
}

}  // namespace

void post_task(std::function<void()> task) {
  Rtask_post(&run_posted, new std::function<void()>(std::move(task)));
}

struct Table::Callbacks {
  std::function<std::string(int, int)> text;
  std::function<void(int)> select;
  std::function<void(int)> activate;
  std::function<void(int, bool)> sort;
  std::function<bool(int, std::uint8_t)> key;
  std::string buffer;  // the last cell text handed to Raven (valid until the next call)
  bool quiet = false;  // set_selected(): not a pick

  static const char* cell(Rid, int row, int column, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    cb->buffer = cb->text ? cb->text(row, column) : std::string();
    return cb->buffer.c_str();
  }
  static void selected(Rid, int row, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (!cb->quiet && row >= 0 && cb->select) cb->select(row);
  }
  static void activated(Rid, int row, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (row >= 0 && cb->activate) cb->activate(row);
  }
  static void sorted(Rid, int column, int descending, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (cb->sort) cb->sort(column, descending != kFalse);
  }
  static int event(Rid, const REvent* e, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (e->type != R_EVENT_KEY_DOWN || !cb->key) return kFalse;
    return cb->key(static_cast<int>(e->key), static_cast<std::uint8_t>(e->modifiers)) ? kTrue : kFalse;
  }
};

Table::Table(WidgetId id) : Widget(id), callbacks_(std::make_unique<Callbacks>()) {}
Table::~Table() = default;

std::unique_ptr<Table> Table::create() {
  std::unique_ptr<Table> table(new Table(Rwidget_create(R_WIDGET_TABLE)));
  Callbacks* cb = table->callbacks_.get();
  Rtable_set_selection_changed_callback(table->id_, &Callbacks::selected, cb);
  Rtable_set_activated_callback(table->id_, &Callbacks::activated, cb);
  Rtable_set_sort_callback(table->id_, &Callbacks::sorted, cb);
  Rwidget_set_event_callback(table->id_, &Callbacks::event, cb);
  Rtable_set_striped(table->id_, kFalse);
  Rtable_set_grid_lines(table->id_, kFalse);
  return table;
}

void Table::add_column(std::string_view header, float width, float fill, bool sortable) {
  const int column = Rtable_add_column(id_, str(header).c_str(), width);
  Rtable_set_column_fill(id_, column, fill);
  Rtable_set_column_sortable(id_, column, sortable ? kTrue : kFalse);
  if (fill <= 0.0F) Rtable_set_column_align(id_, column, 1.0F);
}

void Table::set_rows(int count, std::function<std::string(int, int)> text) {
  callbacks_->text = std::move(text);
  Rtable_set_virtual_rows(id_, count, &Callbacks::cell, callbacks_.get());
}

void Table::set_selected(int row) {
  callbacks_->quiet = true;
  Rtable_set_selected(id_, row);
  callbacks_->quiet = false;
}

int Table::selected() const {
  return Rtable_get_selected(id_);
}

void Table::scroll_to_row(int row) {
  Rtable_scroll_to_row(id_, row);
}

void Table::show_sort(int column, bool descending) {
  Rtable_sort(id_, column, descending ? kTrue : kFalse);
}

void Table::set_header_visible(bool visible) {
  Rtable_set_header_visible(id_, visible ? kTrue : kFalse);
}

void Table::on_select(std::function<void(int)> callback) {
  callbacks_->select = std::move(callback);
}

void Table::on_activate(std::function<void(int)> callback) {
  callbacks_->activate = std::move(callback);
}

void Table::on_sort(std::function<void(int, bool)> callback) {
  callbacks_->sort = std::move(callback);
}

void Table::on_key(std::function<bool(int, std::uint8_t)> callback) {
  callbacks_->key = std::move(callback);
}

// --- M4 SHELL-B: console input, file dialogs, frames -----------------------------

struct TextEditor::Callbacks {
  std::function<void(std::string)> change;
  bool quiet = false;  // set_text(): not an edit

  static void on_change(Rid editor, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (cb->quiet || !cb->change) return;
    const char* t = Rtext_editor_get_text(editor);
    cb->change(t != nullptr ? std::string(t) : std::string());
  }
};

TextEditor::TextEditor(WidgetId id) : Widget(id), callbacks_(std::make_unique<Callbacks>()) {}

TextEditor::~TextEditor() = default;

std::unique_ptr<TextEditor> TextEditor::create(std::string_view placeholder) {
  std::unique_ptr<TextEditor> editor(new TextEditor(Rtext_editor_create("")));
  Rtext_editor_set_placeholder(editor->id_, str(placeholder).c_str());
  Rtext_editor_set_line_numbers(editor->id_, kFalse);
  Rtext_editor_set_word_wrap(editor->id_, kTrue);
  Rtext_editor_set_changed_callback(editor->id_, &Callbacks::on_change, editor->callbacks_.get());
  return editor;
}

void TextEditor::set_text(std::string_view text) {
  callbacks_->quiet = true;
  Rtext_editor_set_text(id_, str(text).c_str());
  const int last = std::max(Rtext_editor_get_line_count(id_) - 1, 0);
  const auto newline = text.rfind('\n');
  const std::size_t column = newline == std::string_view::npos ? text.size() : text.size() - newline - 1;
  Rtext_editor_set_cursor(id_, last, static_cast<int>(column));
  callbacks_->quiet = false;
}

std::string TextEditor::text() const {
  const char* t = Rtext_editor_get_text(id_);
  return t != nullptr ? std::string(t) : std::string();
}

int TextEditor::cursor_line() const {
  int line = 0;
  int column = 0;
  Rtext_editor_get_cursor(id_, &line, &column);
  return line;
}

std::pair<int, int> TextEditor::cursor() const {
  int line = 0;
  int column = 0;
  Rtext_editor_get_cursor(id_, &line, &column);
  return {line, column};
}

void TextEditor::set_cursor(int line, int column) {
  Rtext_editor_set_cursor(id_, line, column);
}

int TextEditor::line_count() const {
  return Rtext_editor_get_line_count(id_);
}

void TextEditor::set_font(WidgetId font, float size) {
  Rtext_editor_set_font(id_, font, size);
}

void TextEditor::on_change(std::function<void(std::string)> callback) {
  callbacks_->change = std::move(callback);
}

std::optional<std::string> file_dialog(WidgetId parent, const FileDialogOptions& options) {
  std::vector<RFileFilter> filters;
  filters.reserve(options.filters.size());
  for (const FileDialogFilter& f : options.filters) filters.push_back(RFileFilter{f.name.c_str(), f.patterns.c_str()});
  RFileDialogDesc desc{};
  desc.parent = parent;  // kNoWidget is Raven's invalid id: no parent window
  desc.title = options.title.empty() ? nullptr : options.title.c_str();
  desc.directory = options.directory.empty() ? nullptr : options.directory.c_str();
  desc.file_name = options.file_name.empty() ? nullptr : options.file_name.c_str();
  desc.filters = filters.empty() ? nullptr : filters.data();
  desc.filter_count = static_cast<int>(filters.size());
  desc.default_extension = options.default_extension.empty() ? nullptr : options.default_extension.c_str();
  const char* path = options.save ? Rfile_dialog_save(&desc) : Rfile_dialog_open(&desc);
  if (path == nullptr) return std::nullopt;
  return std::string(path);
}

void pump_frame() {
  (void)Rdraw_frame();
}

Toast Toast::create() {
  return Toast(Rwidget_create(R_WIDGET_TOAST));
}

void Toast::show(std::string_view text, StatusKind kind, double seconds) {
  Rtoast_show_kind(id_, str(text).c_str(), toast_kind(kind), seconds);
}

void Widget::place_top_right(float right, float top) {
  Rslot_set_halign(id_, R_HALIGN_RIGHT);
  Rslot_set_valign(id_, R_VALIGN_TOP);
  Rslot_set_padding(id_, 0.0F, top, right, 0.0F);
}

void Widget::place_fill() {
  Rslot_set_halign(id_, R_HALIGN_FILL);
  Rslot_set_valign(id_, R_VALIGN_FILL);
  Rslot_set_padding(id_, 0.0F, 0.0F, 0.0F, 0.0F);
}

void Widget::place_top_center(float top) {
  Rslot_set_halign(id_, R_HALIGN_CENTER);
  Rslot_set_valign(id_, R_VALIGN_TOP);
  Rslot_set_padding(id_, 0.0F, top, 0.0F, 0.0F);
}

void Toast::place_bottom_center() {
  Rslot_set_halign(id_, R_HALIGN_CENTER);
  Rslot_set_valign(id_, R_VALIGN_BOTTOM);
  Rslot_set_padding(id_, 16.0F, 16.0F, 16.0F, 16.0F);
}

void TextInput::set_text(std::string_view text) {
  Rtext_input_set_text(id_, str(text).c_str());
}

std::string TextInput::text() const {
  const char* t = Rtext_input_get_text(id_);
  return t != nullptr ? std::string(t) : std::string();
}

void TextInput::set_outline(const std::optional<Rgba>& color) {
  for (const RColorRole role : {R_COLOR_BORDER, R_COLOR_FOCUS}) {
    if (color) {
      Rwidget_set_color(id_, role, color->r, color->g, color->b, color->a);
    } else {
      Rwidget_clear_color(id_, role);
    }
  }
}

bool Window::active() const {
  return id_ != kNoWidget && Rwindow_is_active(id_) != kFalse;
}

bool Window::set_busy_progress(bool busy) const {
  if (id_ == kNoWidget) return false;
  return Rwindow_set_taskbar_progress(id_, busy ? R_TASKBAR_PROGRESS_INDETERMINATE : R_TASKBAR_PROGRESS_NONE, 0.0F) !=
         kFalse;
}

void Window::request_attention() const {
  if (id_ != kNoWidget) (void)Rwindow_request_attention(id_, kFalse);
}

// --- M4 widgets -------------------------------------------------------------------------

struct MenuBar::State {
  struct Slot {
    State* state = nullptr;
    std::size_t menu = 0;
    std::size_t entry = 0;
  };
  std::vector<std::unique_ptr<Slot>> slots;
  std::function<void(std::size_t, std::size_t)> on_choose;
  static void clicked(Rid, void* user) {
    const auto* slot = static_cast<Slot*>(user);
    if (slot->state->on_choose) slot->state->on_choose(slot->menu, slot->entry);
  }
};

MenuBar::MenuBar(WidgetId id) : Widget(id), state_(std::make_unique<State>()) {}
MenuBar::~MenuBar() = default;

std::unique_ptr<MenuBar> MenuBar::create() {
  std::unique_ptr<MenuBar> bar(new MenuBar(Rwidget_create(R_WIDGET_MENU_BAR)));
  Rwidget_set_accessible_name(bar->id_, "Menu bar");
  return bar;
}

namespace {

// One command or check entry of a menu (not a separator).
Rid add_menu_entry(Rid menu, const MenuBarEntry& entry) {
  const Rid item = Rmenu_add_item(menu, entry.label.c_str());
  if (entry.checkable) {
    Rmenu_item_set_checkable(item, kTrue);
    Rmenu_item_set_checked(item, entry.checked ? kTrue : kFalse);
  }
  Rwidget_set_enabled(item, entry.enabled ? kTrue : kFalse);
  if (!entry.description.empty()) Rwidget_set_accessible_description(item, entry.description.c_str());
  return item;
}

}  // namespace

void MenuBar::set_menus(const std::vector<MenuBarMenuSpec>& menus,
                        std::function<void(std::size_t, std::size_t)> on_choose) {
  Rwidget_clear_children(id_);
  state_->slots.clear();
  state_->on_choose = std::move(on_choose);
  for (std::size_t m = 0; m < menus.size(); ++m) {
    const Rid menu = Rmenu_add_menu(id_, menus[m].title.c_str());
    for (std::size_t e = 0; e < menus[m].entries.size(); ++e) {
      const MenuBarEntry& entry = menus[m].entries[e];
      if (entry.separator) {
        (void)Rmenu_add_separator(menu);
        continue;
      }
      const Rid item = add_menu_entry(menu, entry);
      state_->slots.push_back(std::make_unique<State::Slot>(State::Slot{state_.get(), m, e}));
      Rmenu_item_set_clicked_callback(item, &State::clicked, state_->slots.back().get());
    }
  }
}

bool MenuBar::native() const {
  return Rmenu_bar_is_native(id_) != kFalse;
}

void MenuBar::set_native(bool native) {
  Rmenu_bar_set_native(id_, native ? kTrue : kFalse);
}

Label Label::create(std::string_view text) {
  return Label(Rlabel_create(str(text).c_str()));
}

void Label::set_text(std::string_view text) {
  Rlabel_set_text(id_, str(text).c_str());
}

void Label::set_color(const Rgba& color) {
  Rlabel_set_color(id_, color.r, color.g, color.b, color.a);
}

void Label::set_wrap(bool wrap) {
  Rlabel_set_wrap(id_, wrap ? kTrue : kFalse);
}

void Label::set_selectable(bool selectable) {
  Rlabel_set_selectable(id_, selectable ? kTrue : kFalse);
}

struct Button::Callbacks {
  std::function<void()> click;
  static void clicked(Rid, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (cb->click) cb->click();
  }
};

Button::Button(WidgetId id) : Widget(id), callbacks_(std::make_unique<Callbacks>()) {}
Button::~Button() = default;

std::unique_ptr<Button> Button::create(std::string_view text, bool primary) {
  std::unique_ptr<Button> button(new Button(Rbutton_create(str(text).c_str())));
  Rbutton_set_style(button->id_, primary ? R_BUTTON_PRIMARY : R_BUTTON_DEFAULT);
  Rwidget_set_accessible_name(button->id_, str(text).c_str());
  Rbutton_set_clicked_callback(button->id_, &Callbacks::clicked, button->callbacks_.get());
  return button;
}

void Button::on_click(std::function<void()> callback) {
  callbacks_->click = std::move(callback);
}

struct CheckBox::Callbacks {
  std::function<void(bool)> toggle;
  static void changed(Rid, int checked, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (cb->toggle) cb->toggle(checked != kFalse);
  }
};

CheckBox::CheckBox(WidgetId id) : Widget(id), callbacks_(std::make_unique<Callbacks>()) {}
CheckBox::~CheckBox() = default;

std::unique_ptr<CheckBox> CheckBox::create(std::string_view text) {
  std::unique_ptr<CheckBox> box(new CheckBox(Rcheckbox_create(str(text).c_str())));
  Rcheckbox_set_changed_callback(box->id_, &Callbacks::changed, box->callbacks_.get());
  return box;
}

void CheckBox::set_checked(bool checked) {
  Rcheckbox_set_checked(id_, checked ? kTrue : kFalse);
}

void CheckBox::on_toggle(std::function<void(bool)> callback) {
  callbacks_->toggle = std::move(callback);
}

struct ComboBox::Callbacks {
  std::function<void(int)> select;
  static void changed(Rid, int index, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (cb->select) cb->select(index);
  }
};

ComboBox::ComboBox(WidgetId id) : Widget(id), callbacks_(std::make_unique<Callbacks>()) {}
ComboBox::~ComboBox() = default;

std::unique_ptr<ComboBox> ComboBox::create(const std::vector<std::string>& items) {
  std::unique_ptr<ComboBox> box(new ComboBox(Rwidget_create(R_WIDGET_COMBO_BOX)));
  for (const std::string& item : items) (void)Rcombo_box_add_item(box->id_, item.c_str());
  Rcombo_box_set_changed_callback(box->id_, &Callbacks::changed, box->callbacks_.get());
  return box;
}

void ComboBox::set_selected(int index) {
  Rcombo_box_set_selected(id_, index);
}

void ComboBox::on_select(std::function<void(int)> callback) {
  callbacks_->select = std::move(callback);
}

}  // namespace hefesto::shell::ui
