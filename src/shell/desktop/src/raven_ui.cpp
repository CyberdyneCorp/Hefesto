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

#include <cstdio>
#include <filesystem>
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

Rgba mix(const Rgba& a, const Rgba& b, float t) {
  return Rgba{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, 1.0F};
}

/// Maps the forge tokens onto Raven's color roles.
void apply_tokens(const ThemeTokens& t) {
  set_color(R_COLOR_WINDOW, t.surface0);
  set_color(R_COLOR_PANEL, t.surface1);
  set_color(R_COLOR_HEADER, t.surface1);
  set_color(R_COLOR_TAB_ACTIVE, t.surface2);
  set_color(R_COLOR_POPUP, t.surface1);
  set_color(R_COLOR_INPUT, t.surface0);
  set_color(R_COLOR_CONTROL, t.surface2);
  set_color(R_COLOR_CONTROL_HOVERED, mix(t.surface2, t.border, 0.5F));
  set_color(R_COLOR_BORDER, t.border);
  set_color(R_COLOR_CONTROL_BORDER, t.border);
  set_color(R_COLOR_POPUP_BORDER, t.border);
  set_color(R_COLOR_SEPARATOR, t.border);
  set_color(R_COLOR_TEXT, t.text_primary);
  set_color(R_COLOR_TEXT_PLACEHOLDER, t.text_secondary);
  set_color(R_COLOR_TEXT_DISABLED, t.text_secondary);
  set_color(R_COLOR_ACCENT, t.accent);
  set_color(R_COLOR_FOCUS, t.accent);
  Rtheme_set_accent(t.accent.r, t.accent.g, t.accent.b);
}

std::optional<WidgetId> load_font(const std::string& dir, const char* file) {
  const std::filesystem::path path = std::filesystem::path(dir) / file;
  if (!std::filesystem::exists(path)) {
    return std::nullopt;
  }
  const Rid font = Rfont_create_from_file(path.string().c_str());
  if (Rfont_is_valid(font) == kFalse) {
    return std::nullopt;
  }
  return font;
}

PointerButton to_button(RMouseButton b) {
  switch (b) {
    case R_MOUSE_LEFT: return PointerButton::left;
    case R_MOUSE_RIGHT: return PointerButton::right;
    case R_MOUSE_MIDDLE: return PointerButton::middle;
    default: return PointerButton::other;
  }
}

CanvasKey to_key(RKey key) {
  switch (key) {
    case R_KEY_LEFT: return CanvasKey::left;
    case R_KEY_RIGHT: return CanvasKey::right;
    case R_KEY_UP: return CanvasKey::up;
    case R_KEY_DOWN: return CanvasKey::down;
    case R_KEY_EQUAL:
    case R_KEY_KP_ADD: return CanvasKey::zoom_in;
    case R_KEY_MINUS:
    case R_KEY_KP_SUBTRACT: return CanvasKey::zoom_out;
    case R_KEY_F:
    case R_KEY_HOME: return CanvasKey::fit;
    default: return CanvasKey::other;
  }
}

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
  in.key = to_key(e.key);
  return in;
}

RDockSide to_dock_side(DockSide side) {
  switch (side) {
    case DockSide::left: return R_DOCK_LEFT;
    case DockSide::right: return R_DOCK_RIGHT;
    case DockSide::top: return R_DOCK_TOP;
    case DockSide::bottom: return R_DOCK_BOTTOM;
    case DockSide::tab: return R_DOCK_TAB;
  }
  return R_DOCK_TAB;
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

bool Runtime::apply_theme(ThemeMode mode, const std::string& font_dir) {
  Rtheme_apply(mode == ThemeMode::light ? R_THEME_LIGHT : R_THEME_DARK);
  apply_tokens(theme_tokens(mode));
  if (font_dir.empty()) {
    mono_font_ = Rfont_get_monospace();
    return true;
  }
  const auto inter = load_font(font_dir, "Inter-Regular.ttf");
  const auto mono = load_font(font_dir, "JetBrainsMono-Regular.ttf");
  if (inter) {
    ui_font_ = *inter;
    Rtheme_set_font(ui_font_);
  }
  mono_font_ = mono ? *mono : Rfont_get_monospace();
  return inter.has_value() && mono.has_value();
}

// --- Widgets ---------------------------------------------------------------

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

void Box::set_padding(float left, float top, float right, float bottom) {
  Rslot_set_padding(id_, left, top, right, bottom);
}

void Box::set_spacing(float spacing) {
  Rpanel_set_spacing(id_, spacing);
}

Label Label::create(std::string_view text) {
  return Label(Rlabel_create(str(text).c_str()));
}

void Label::set_text(std::string_view text) {
  Rlabel_set_text(id_, str(text).c_str());
}

ListPanel ListPanel::create() {
  return ListPanel(Rwidget_create(R_WIDGET_LIST_VIEW));
}

void ListPanel::add_item(std::string_view text) {
  Rlist_view_add_item(id_, str(text).c_str());
}

void ListPanel::scroll_to_end() {
  const int count = Rlist_view_get_item_count(id_);
  if (count > 0) {
    Rlist_view_scroll_to_item(id_, count - 1);
  }
}

TreePanel TreePanel::create() {
  return TreePanel(Rwidget_create(R_WIDGET_TREE_VIEW));
}

WidgetId TreePanel::add_item(WidgetId parent, std::string_view text) {
  return Rtree_view_add_item(id_, parent, str(text).c_str());
}

Dock Dock::create() {
  return Dock(Rdock_area_create());
}

void Dock::add_panel(const Widget& panel, std::string_view title, DockSide side, const Widget* relative, float size) {
  Rdock_area_add_panel(id_, panel.id(), str(title).c_str(), to_dock_side(side),
                       relative != nullptr ? relative->id() : kNoWidget, size);
}

// --- CommandBar ------------------------------------------------------------

struct CommandBar::Callbacks {
  std::function<void(std::string)> commit;
  std::function<std::string(bool)> history;

  static void on_commit(Rid /*input*/, const char* text, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (cb->commit) cb->commit(text != nullptr ? text : "");
  }
  static int on_event(Rid input, const REvent* e, void* self) {
    auto* cb = static_cast<Callbacks*>(self);
    if (e->type != R_EVENT_KEY_DOWN || !cb->history || (e->key != R_KEY_UP && e->key != R_KEY_DOWN)) {
      return kFalse;
    }
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

// --- Window ----------------------------------------------------------------

Window::Window(std::string_view title, int width, int height)
    : id_(Rwindow_create(str(title).c_str(), width, height, R_WINDOW_DEFAULT)) {}

Window::~Window() {
  destroy();
}

void Window::destroy() {
  // Deleting the window waits for its last frames: no draw callback runs after.
  if (id_ != kNoWidget && valid()) {
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

}  // namespace hefesto::shell::ui
