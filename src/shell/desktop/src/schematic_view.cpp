// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Schematic tab's Raven view (schematic_view.hpp). Owner: SHELL.

#include "schematic_view.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <numbers>
#include <string>
#include <utility>

#include "hefesto/diag/format.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/pcell/framework/tcl_list.hpp"
#include "hefesto/platform_posix/executable.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/shell/view_models.hpp"
#include "hefesto/ui/state.hpp"
#include "screen_paint.hpp"
#include "side_panels.hpp"

namespace hefesto::shell {

namespace {

constexpr float kStroke = 1.5F;
constexpr float kCrumbBar = 28.0F;
constexpr float kNetlistBar = 40.0F;
constexpr float kMessagesHeight = 120.0F;
constexpr float kLibraryWidth = 240.0F;
constexpr float kPropertiesWidth = 260.0F;
constexpr float kTitleHeight = 32.0F;
constexpr float kRowHeight = 24.0F;
constexpr float kPadX = 12.0F;
constexpr float kBarButtonHeight = 28.0F;
constexpr float kCrumbGap = 18.0F;  // " › " between crumbs
constexpr int kArcSteps = 24;
constexpr double kPlaceSnap = 10.0;  // xschem's default snap
constexpr std::string_view kEmptyHint = "No schematic open: schematic open <file.sch>";

// The highlight colour cycle (EDIT's kHighlightColours) in forge tokens:
// state is never color only, the highlight is also drawn thicker.
Rgba highlight_color(const ThemeTokens& t, int colour) {
  const std::array<Rgba, 8> cycle{t.accent, t.info, t.success, t.warning, t.danger, t.accent_soft, t.icon,
                                  t.text_primary};
  return cycle[static_cast<std::size_t>(std::abs(colour)) % cycle.size()];
}

Rgba source_color(const ThemeTokens& t, SceneSource source) {
  switch (source) {
    case SceneSource::wire: return t.info;
    case SceneSource::pin: return t.danger;
    case SceneSource::symbol: return t.success;
    case SceneSource::graphic: break;
  }
  return t.icon;
}

// Draws scene items through the viewport (schematic units to canvas pixels).
struct ScenePainter {
  ui::Painter& p;
  const SchematicViewport& viewport;
  std::function<Rgba(SceneSource, std::size_t)> color;

  [[nodiscard]] std::pair<float, float> at(schem::Point pt) const {
    const CanvasPoint c = viewport.to_canvas(pt);
    return {static_cast<float>(c.x), static_cast<float>(c.y)};
  }
  void segment(schem::Point a, schem::Point b, const Rgba& c, float width) const {
    const auto [x0, y0] = at(a);
    const auto [x1, y1] = at(b);
    p.line(x0, y0, x1, y1, width, c);
  }
  void polygon(const ScenePolygon& poly) const {
    const Rgba c = color(poly.source, poly.index);
    for (std::size_t i = 0; i < poly.points.size(); ++i) {
      segment(poly.points[i], poly.points[(i + 1) % poly.points.size()], c, kStroke);
    }
  }
  void arc(const SceneArc& arc) const {
    const double step = arc.sweep_angle / kArcSteps * std::numbers::pi / 180.0;
    const double start = arc.start_angle * std::numbers::pi / 180.0;
    const auto point = [&](int k) {  // y down: positive angles turn clockwise on screen, as the file's arcs
      const double a = start + step * k;
      return schem::Point{arc.centre.x + arc.radius * std::cos(a), arc.centre.y - arc.radius * std::sin(a)};
    };
    const Rgba c = color(arc.source, arc.index);
    for (int i = 0; i < kArcSteps; ++i) segment(point(i), point(i + 1), c, kStroke);
  }
};

// A message's status kind and its line with the kind's shape (state is
// never color only): "■ SCHEM-NET-004  net OUT shorted to IN".
std::pair<StatusKind, std::string> message_line(const Message& m) {
  StatusKind kind = StatusKind::info;
  std::string_view shape = "● ";
  if (m.severity == MessageSeverity::error) {
    kind = StatusKind::error;
    shape = "■ ";
  } else if (m.severity == MessageSeverity::warning) {
    kind = StatusKind::warning;
    shape = "▲ ";
  }
  return {kind, std::string{shape} + (m.code.empty() ? std::string{} : m.code + "  ") + m.text};
}

std::string_view netlist_state_name(NetlistAction::State state) {
  switch (state) {
    case NetlistAction::State::running: return "running";
    case NetlistAction::State::done: return "done";
    case NetlistAction::State::failed: return "failed";
    case NetlistAction::State::idle: break;
  }
  return "idle";
}

// "252,76": a widget's top-left corner in window units (scripted clicks).
std::string rect_origin(const ui::Rect& r) {
  return std::to_string(std::lround(r.x)) + "," + std::to_string(std::lround(r.y));
}

// The system clipboard through Raven.
class RavenClipboard final : public ClipboardHost {
 public:
  void set_text(const std::string& text) override { ui::set_clipboard_text(text); }
  [[nodiscard]] std::string text() override { return ui::clipboard_text(); }
};

std::vector<std::string> words_of(const std::string& line) {
  auto words = pcell::split_tcl_list(line);
  return words ? std::move(words.value()) : std::vector<std::string>{};
}

}  // namespace

SchematicView::SchematicView(ScreenContext& ctx)
    : ctx_(ctx),
      library_panel_(ui::Panel::create(ctx.tokens().surface1)),
      properties_panel_(ui::Panel::create(ctx.tokens().surface1)),
      input_(
#ifdef __APPLE__
          true
#else
          false
#endif
          ),
      resolver_(SchematicSymbolResolver::Options{
          std::make_shared<platform::LocalFileSystem>(), {}, [alive = std::weak_ptr<bool>(alive_), this] {
            // The worker thread: hand the result to the UI thread.
            ui::post_task([alive, this] {
              if (!alive.expired()) symbols_ready();
            });
          }}),
      clipboard_host_(std::make_unique<RavenClipboard>()),
      clipboard_(*clipboard_host_) {
  input_.keys().attach(ctx_.editor().macros);  // the core's `macro schematic ...` edits them
  const char* cad_root = std::getenv("CAD_ROOT");
  install_root_ = desktop_install_root(ctx_.core.files(), platform_posix::executable_dir(),
                                       cad_root != nullptr ? std::optional<std::string>{cad_root} : std::nullopt);
  build_library();
  build_centre();
  build_properties();
  separators_.push_back(make_separator(ctx_, true));
  separators_.push_back(make_separator(ctx_, true));
  root_.add(library_panel_);
  root_.add(*separators_[0]);
  root_.add(centre_, 1.0F);
  root_.add(*separators_[1]);
  root_.add(properties_panel_);
  root_.set_accessible_name("Schematic editor");
  breadcrumb_.set_top("No schematic open");
  ctx_.core.dispatcher().add_observer(*this);
}

SchematicView::~SchematicView() {
  ctx_.core.dispatcher().remove_observer(*this);
  alive_.reset();  // tasks posted by the worker see the view gone; resolver_ joins it
}

// --- layout -------------------------------------------------------------------------------

void SchematicView::build_library() {
  library_title_ = make_painted(ctx_, [this](ui::Painter& p, float, float h, const ThemeTokens& t) {
    paint::section_label(p, kPadX, h - 4.0F - p.line_height(TextRole::section_label), library_.title(), t);
  });
  library_title_->set_size(-1.0F, kTitleHeight);
  library_title_->set_role(ui::Role::heading);
  search_ = ui::TextInput::create(SymbolLibraryBrowser::placeholder());
  search_->set_accessible_name("Search symbols");
  search_->set_size(-1.0F, static_cast<float>(kInputHeight));
  search_->set_font(ctx_.runtime.font(TextRole::small));
  search_->set_font_size(text_spec(TextRole::small).size_px);
  search_->on_change([this](std::string text) {
    library_.set_filter(std::move(text));
    library_rows_->set_size(-1.0F, std::max(1.0F, static_cast<float>(library_.visible().size())) * kRowHeight);
    library_rows_->invalidate();
  });
  library_rows_ = make_painted(ctx_, [this](ui::Painter& p, float w, float, const ThemeTokens& t) {
    paint_library_rows(p, w, t);
  });
  library_rows_->set_role(ui::Role::list);
  library_rows_->set_accessible_name("Symbols");
  library_rows_->set_tooltip("Click a symbol to place it at the centre of the view (schematic place)");
  library_rows_->on_click([this](float, float y) { place(static_cast<std::size_t>(std::max(0.0F, y) / kRowHeight)); });
  library_rows_->set_size(-1.0F, kRowHeight);
  library_scroll_.add(*library_rows_);
  ui::Box search_row = ui::Box::vertical();
  search_row.set_padding(kPadX, 0.0F, kPadX, 8.0F);
  search_row.add(*search_);
  library_column_.add(*library_title_);
  library_column_.add(search_row);
  library_column_.add(library_scroll_, 1.0F);
  library_panel_.set_child(library_column_);
  library_panel_.set_size(kLibraryWidth, -1.0F);
  library_panel_.set_accessible_name("Symbol library");
}

void SchematicView::build_centre() {
  crumbs_ = make_painted(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t) { paint_crumbs(p, w, h, t); });
  crumbs_->set_size(-1.0F, kCrumbBar);
  crumbs_->set_accessible_name("Hierarchy path");
  crumbs_->on_click([this](float x, float) { click_crumb(x); });

  canvas_ = make_painted(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t) { paint_canvas(p, w, h, t); });
  // The painted scene never takes input: the viewport below gets every
  // event (pointer, wheel, pinch, keys once focused by a click).
  canvas_->set_hit_testable(false);
  input_viewport_ = ui::Viewport::create();
  input_viewport_->set_accessible_name("Schematic canvas");
  input_viewport_->set_tooltip(
      "Schematic canvas: click selects; wheel or pinch zooms, two-finger scroll, Option-drag or middle-drag pans; "
      "keys run the schematic bindings (w wire, m move, c copy, R rotate, F flip, e descend, Ctrl+e ascend, "
      "q properties, k highlight, u undo, U redo, Insert place, n netlist); Copy and Paste use the system clipboard");
  input_viewport_->on_input([this](const CanvasInput& input) { return canvas_input(input); });
  stage_.add(*input_viewport_);
  stage_.add(*canvas_);
  stage_.set_accessible_name("Schematic canvas");

  build_netlist_bar();
  build_messages();
  centre_.add(*crumbs_);
  centre_.add(stage_, 1.0F);
  centre_.add(*netlist_bar_);
  centre_.add(*messages_view_);
}

void SchematicView::build_netlist_bar() {
  netlist_bar_ = make_painted(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t) {
    paint_netlist_bar(p, w, h, t);
  });
  netlist_bar_->set_size(-1.0F, kNetlistBar);
  netlist_bar_->set_accessible_name("Netlist");
  netlist_bar_->set_tooltip("Netlist (n): schematic netlist; Interrupt (Ctrl-C) stops a long run");
  netlist_bar_->on_click([this](float x, float) { click_netlist_bar(x); });
}

void SchematicView::build_messages() {
  messages_view_ = make_painted(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t) {
    paint_messages(p, w, h, t);
  });
  messages_view_->set_size(-1.0F, kMessagesHeight);
  messages_view_->set_role(ui::Role::list);
  messages_view_->set_accessible_name("Messages");
  messages_view_->set_tooltip("Messages: the diagnostics of the schematic commands; click to clear");
  messages_view_->on_click([this](float, float) {
    messages_.clear();
    messages_view_->invalidate();
  });
}

void SchematicView::build_properties() {
  properties_title_ = make_painted(ctx_, [this](ui::Painter& p, float, float h, const ThemeTokens& t) {
    const std::string title = properties_.title();
    paint::section_label(p, kPadX, h - 4.0F - p.line_height(TextRole::section_label),
                         title.empty() ? std::string{"Properties"} : "Properties · " + title, t);
  });
  properties_title_->set_size(-1.0F, kTitleHeight);
  properties_title_->set_role(ui::Role::heading);
  property_rows_ = make_painted(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t) {
    paint_property_rows(p, w, h, t);
  });
  property_rows_->set_role(ui::Role::list);
  property_rows_->set_accessible_name("Properties");
  property_rows_->set_tooltip("Click a property to edit it below (schematic setprop)");
  property_rows_->on_click(
      [this](float, float y) { edit_property(static_cast<std::size_t>(std::max(0.0F, y) / kRowHeight)); });
  property_rows_->set_size(-1.0F, 3.0F * kRowHeight);
  properties_scroll_.add(*property_rows_);
  build_field_editor();
  properties_column_.add(*properties_title_);
  properties_column_.add(properties_scroll_, 1.0F);
  properties_column_.add(field_box_);
  properties_panel_.set_child(properties_column_);
  properties_panel_.set_size(kPropertiesWidth, -1.0F);
  properties_panel_.set_accessible_name("Properties");
}

// The field editor ("Field view"): the key and value of the row clicked, or a
// new key; Return in either field or Set runs the change's one setprop line.
void SchematicView::build_field_editor() {
  const auto input = [this](std::string_view placeholder) {
    auto field = ui::TextInput::create(placeholder);
    field->set_accessible_name(placeholder);
    field->set_size(-1.0F, static_cast<float>(kInputHeight));
    field->set_font(ctx_.runtime.font(TextRole::mono));
    field->set_font_size(text_spec(TextRole::small).size_px);
    field->on_commit([this](const std::string&) { commit_field(); });
    return field;
  };
  field_key_ = input("Key");
  field_value_ = input("Value");
  field_value_->set_tooltip("Return sets the value; an empty value removes the attribute (schematic setprop)");
  field_all_ = ui::CheckBox::create("Apply to all selected");
  field_all_->set_tooltip("Set the attribute on every selected instance and wire (schematic setprop selection)");
  field_all_->on_toggle([this](bool on) { field_all_selected_ = on; });
  field_set_ = ui::Button::create("Set", true);
  field_set_->set_accessible_name("Set the attribute");
  field_set_->on_click([this] { commit_field(); });
  field_box_.set_padding(kPadX, 8.0F, kPadX, 8.0F);
  field_box_.add(*field_key_);
  field_box_.add(*field_value_);
  field_box_.add(*field_all_);
  field_box_.add(*field_set_);
  field_box_.set_accessible_name("Edit a property");
  field_box_.set_visible(false);
}

// --- state --------------------------------------------------------------------------------

void SchematicView::refresh() {
  if (refreshing_) return;  // a read below runs a command
  refreshing_ = true;
  schem::Session& session = ctx_.editor().schem;
  if (session.documents.empty()) {
    show_nothing();
  } else {
    show_document(session);
  }
  refreshing_ = false;
  invalidate_all();
}

std::string SchematicView::state_line() const {
  const std::string shown = properties_.title();
  std::string line = shown.empty() ? std::string{"properties none"}
                                   : "properties " + shown + " " + std::to_string(properties_.rows().size()) + " rows";
  for (const PropertyRow& row : properties_.rows()) line += " " + row.key + "=" + row.value;
  line += ", highlights " + std::to_string(highlight_.nets().size()) + " nets " +
          std::to_string(highlight_.instances().size()) + " instances";
  std::array<char, 32> zoom{};
  (void)std::snprintf(zoom.data(), zoom.size(), "%.3g", viewport_.zoom());
  line += std::string{", zoom "} + zoom.data() + (input_.user_view() ? " user view" : " fitted");
  line += ", netlist " + std::string{netlist_state_name(netlist_.state())} + (card_seen_ ? ", card seen" : ", no card");
  return line + ", canvas at " + rect_origin(input_viewport_->rect()) + ", rows at " + rect_origin(property_rows_->rect());
}

std::string SchematicView::status_line() const {
  return breadcrumb_.text() + ", " + std::to_string(scene_.instances.size()) + " instances, " +
         std::to_string(resolver_.pending_count()) + " pending, canvas " +
         std::to_string(std::lround(viewport_.width())) + " x " + std::to_string(std::lround(viewport_.height()));
}

void SchematicView::show_nothing() {
  has_document_ = false;
  shown_id_ = 0;
  resolver_.forget();
  scene_ = {};
  properties_.clear();
  breadcrumb_.set_top("No schematic open");
  breadcrumb_.set_path(".");
}

void SchematicView::show_document(schem::Session& session) {
  const schem::OpenDocument& open = session.documents.back();  // the active one is last
  if (open.id != shown_id_) {
    shown_id_ = open.id;
    input_.reset_view();
    properties_.clear();
    session_stale_ = true;
  }
  has_document_ = true;
  const bool compat = session.compat == nullptr || session.compat->xschem_compat;
  (void)resolver_.show(open.id, open.document, session.search_path, compat);
  breadcrumb_.set_top(open.document.path.empty() ? std::string{"untitled.sch"}
                                                 : open.document.path.filename().string());
  // The reads run commands: never in the middle of the netlist run.
  if (session_stale_ && netlist_.state() != NetlistAction::State::running) read_session_state();
  reload_library(session);
  rebuild_scene();
}

void SchematicView::rebuild_scene() {
  const schem::Session& session = ctx_.editor().schem;
  if (session.documents.empty()) return;
  const schem::Document& doc = session.documents.back().document;
  SceneHighlight lit{highlight_.nets(), highlight_.instances()};
  scene_ = build_scene(doc, resolver_.lookup(), lit, &resolver_.pending_symbol());
  wire_colours_.assign(doc.wires.size(), -1);
  for (const SceneSegment& s : scene_.segments) {
    if (s.source != SceneSource::wire || !s.highlighted || s.index >= doc.wires.size()) continue;
    const auto lab = schem::get_token(doc.wires[s.index].props, "lab");
    wire_colours_[s.index] = lab ? highlight_.colour(*lab, false).value_or(0) : 0;
  }
  instance_colours_.assign(scene_.instances.size(), -1);
  for (std::size_t i = 0; i < scene_.instances.size(); ++i) {
    if (scene_.instances[i].highlighted) instance_colours_[i] = highlight_.colour(scene_.instances[i].name, true).value_or(0);
  }
}

void SchematicView::reload_library(const schem::Session& session) {
  std::vector<platform::Path> dirs;
  const platform::Path folder = session.documents.back().document.path.parent_path();
  for (const platform::Path& dir : session.search_path.directories()) {
    if (dir == platform::Path{schem::kDocumentFolderElement}) {
      if (!folder.empty()) dirs.push_back(folder);
    } else {
      dirs.push_back(dir);
    }
  }
  if (library_loaded_ && dirs == library_dirs_) return;
  library_dirs_ = dirs;
  library_loaded_ = true;
  library_.load(ctx_.core.files(), library_folders(dirs, install_root_));
  library_rows_->set_size(-1.0F, std::max(1.0F, static_cast<float>(library_.visible().size())) * kRowHeight);
}

std::optional<std::string> SchematicView::read(const std::string& line) {
  std::vector<std::string> words = words_of(line);
  if (words.empty()) return std::nullopt;
  cmd::Invocation invocation{words.front(), {words.begin() + 1, words.end()}, cmd::Origin::internal, std::nullopt};
  const cmd::CommandResult result = ctx_.core.dispatcher().execute(invocation);
  if (!result.ok()) return std::nullopt;
  return result.value.to_string();
}

// `hilight list`, `hierarchy path` and the properties of the target, read
// with origin `internal` (not replayed, no command count) after a command.
void SchematicView::read_session_state() {
  session_stale_ = false;
  highlight_.set_list(read(NetHighlight::list_command()).value_or(std::string{}));
  breadcrumb_.set_path(read(HierarchyBreadcrumb::path_command()).value_or(std::string{"."}));
  if (const auto load = properties_.load_command()) {
    const auto props = read(*load);
    properties_.set_properties(props.value_or(std::string{}));
    property_rows_->set_size(-1.0F, static_cast<float>(properties_.rows().size() + 2) * kRowHeight);
  }
  field_box_.set_visible(properties_.editable());
}

void SchematicView::symbols_ready() {
  if (!resolver_.take_finished()) return;
  CommandOutcome warnings;
  for (const diag::Diagnostic& d : resolver_.take_warnings()) warnings.diagnostics.push_back(diag::format_text(d));
  messages_.add(warnings);
  refresh();  // the resolved symbols, and a job for references added meanwhile
}

void SchematicView::on_command(const cmd::Invocation& invocation, const cmd::CommandResult& result) {
  if (invocation.origin == cmd::Origin::internal) return;  // the view's own reads
  session_stale_ = true;
  if (invocation.name != "schematic") return;
  follow_selection(invocation, result);
  (void)clipboard_.copied(invocation, result);  // Copy: the text to the system clipboard too
  note_symbol_changes(invocation, result);
  const CommandOutcome outcome = outcome_of(result);
  const bool netlist = !invocation.args.empty() && invocation.args.front() == "netlist";
  if (netlist && netlist_.state() == NetlistAction::State::running) {
    netlist_.finish(outcome, messages_);
  } else {
    messages_.add(outcome);
  }
}

// The session cache changes the canvas's resolver mirrors ("Symbol cache
// and reload"): reload_symbols starts again, reload drops the files changed
// on disk, a save forgets the file.
void SchematicView::note_symbol_changes(const cmd::Invocation& invocation, const cmd::CommandResult& result) {
  if (invocation.args.empty() || !result.ok()) return;
  const std::string& option = invocation.args.front();
  if (option == "reload_symbols") resolver_.reload_symbols();
  if (option == "reload") resolver_.forget_changed();
  const schem::Session& session = ctx_.editor().schem;
  if (option == "save" && !session.documents.empty()) resolver_.forget_file(session.documents.back().document.path);
}

// `schematic select instance <name>` from anywhere (a click, the command
// bar, a script): the properties panel shows that instance.
void SchematicView::follow_selection(const cmd::Invocation& invocation, const cmd::CommandResult& result) {
  const std::vector<std::string>& a = invocation.args;
  if (!result.ok() || a.size() != 3 || a[0] != "select" || a[1] != "instance") return;
  if (properties_.target() == PropertyTarget::instance && properties_.title() == a[2]) return;
  properties_.show_instance(a[2]);
}

void SchematicView::invalidate_all() {
  for (ui::Painted* w : {library_title_.get(), library_rows_.get(), crumbs_.get(), canvas_.get(), netlist_bar_.get(),
                         messages_view_.get(), properties_title_.get(), property_rows_.get()}) {
    w->invalidate();
  }
}

// --- actions ------------------------------------------------------------------------------

bool SchematicView::canvas_input(const CanvasInput& input) {
  if (!has_document_) return false;
  input_.set_running(netlist_.state() == NetlistAction::State::running);
  const SchematicInputResult result = input_.handle(input, viewport_, scene_);
  apply(result);
  return result.consumed;
}

void SchematicView::apply(const SchematicInputResult& result) {
  if (result.repaint) canvas_->invalidate();
  if (result.interrupt) interrupt();
  if (result.copy) copy_selection();
  if (result.paste_at) paste(*result.paste_at);
  if (!result.prompt.empty() && ctx_.prompt) ctx_.prompt(result.prompt);
  if (result.action == "place") search_->focus();
  const bool selects = result.line.starts_with("schematic select instance");
  if (selects || result.action == "properties") {
    if (!input_.selected().empty()) properties_.show_instance(input_.selected());
    session_stale_ = true;  // the properties are read in the next refresh
  }
  if (!result.line.empty()) run(result.line);
  if (result.action == "properties") refresh();
}

void SchematicView::run(const std::string& line) {
  if (line == "schematic netlist") {  // the `n` binding: the netlist action with its card
    start_netlist();
    return;
  }
  if (ctx_.run) ctx_.run(line);
}

NetlistButtonView SchematicView::netlist_button_view() const {
  return netlist_button(netlist_, !ctx_.editor().schem.documents.empty());
}

void SchematicView::start_netlist() {
  if (!refreshing_ && session_stale_) refresh();  // the top bar's button: the document may be new
  const std::string label = breadcrumb_.crumbs().front().label;
  (void)run_netlist(netlist_, has_document_, label,
                    [this](const std::string& line) {
                      netlist_bar_->invalidate();
                      if (netlist_changed_) netlist_changed_();  // running: the top bar's button is disabled
                      tick_netlist();
                      if (ctx_.run) ctx_.run(line);
                    },
                    messages_);
  netlist_bar_->invalidate();
  messages_view_->invalidate();
  if (netlist_changed_) netlist_changed_();
}

// While the netlist runs, the frames its script progress pumps repaint the
// bar, so the Interrupt card appears after kLongRunning and its time counts.
void SchematicView::tick_netlist() {
  ui::post_task([alive = std::weak_ptr<bool>(alive_), this] {
    if (alive.expired()) return;
    netlist_bar_->invalidate();
    if (netlist_.state() == NetlistAction::State::running) tick_netlist();
  });
}

void SchematicView::copy_selection() {
  run(clipboard_.copy_command());  // the observer hands the result to the system clipboard
  clipboard_.end_copy();
}

void SchematicView::paste(schem::Point at) {
  const ClipboardPaste request = clipboard_.paste_commands(at);
  if (request.lines.empty()) {
    CommandOutcome none;
    none.status = OutcomeStatus::failed;
    none.message = request.message;
    messages_.add(none);
    messages_view_->invalidate();
    return;
  }
  for (const std::string& line : request.lines) run(line);
}

void SchematicView::interrupt() {
  if (interrupt_schematic(netlist_, ctx_.editor())) netlist_bar_->invalidate();
}

void SchematicView::place(std::size_t row) {
  if (!has_document_) return;
  const schem::Point centre = viewport_.centre();
  const schem::Point at{std::round(centre.x / kPlaceSnap) * kPlaceSnap, std::round(centre.y / kPlaceSnap) * kPlaceSnap};
  if (const auto line = library_.place_command(row, at)) run(*line);
}

void SchematicView::edit_property(std::size_t row) {
  if (!properties_.editable()) return;
  if (row >= properties_.rows().size()) {  // under the rows: a new attribute
    field_key_->set_text("");
    field_value_->set_text("");
    field_key_->focus();
    return;
  }
  const PropertyRow& property = properties_.rows()[row];
  field_key_->set_text(property.key);
  field_value_->set_text(property.value);
  field_value_->focus();
}

std::optional<std::size_t> SchematicView::instance_index(const std::string& name) const {
  const schem::Session& session = ctx_.editor().schem;
  if (session.documents.empty()) return std::nullopt;
  const std::vector<schem::InstanceRecord>& instances = session.documents.back().document.instances;
  for (std::size_t i = 0; i < instances.size(); ++i) {
    if (schem::get_token(instances[i].props, "name") == name) return i;
  }
  return std::nullopt;
}

// One setprop line per change ("Field view"); a renamed instance is followed
// to the unique name setprop gave it, by its place in the document.
void SchematicView::commit_field() {
  const std::string key = field_key_->text();
  const std::string value = field_value_->text();
  const auto line = properties_.field_command(key, value, field_all_selected_);
  if (!line) {
    CommandOutcome refused;
    refused.status = OutcomeStatus::failed;
    refused.message = "setprop: a key without blanks, '=' or quotes is needed";
    messages_.add(refused);
    messages_view_->invalidate();
    return;
  }
  const auto renamed = properties_.renames(key, value, field_all_selected_) ? instance_index(properties_.title())
                                                                            : std::nullopt;
  run(*line);
  const schem::Session& session = ctx_.editor().schem;
  if (renamed && !session.documents.empty() && *renamed < session.documents.back().document.instances.size()) {
    const std::string& props = session.documents.back().document.instances[*renamed].props;
    properties_.follow_rename(schem::get_token(props, "name").value_or(std::string{}));
  }
  session_stale_ = true;
  refresh();
}

void SchematicView::click_crumb(float x) {
  for (std::size_t i = 0; i < crumb_spans_.size(); ++i) {
    if (x < crumb_spans_[i].x0 || x > crumb_spans_[i].x1) continue;
    for (const std::string& line : breadcrumb_.click_commands(i)) run(line);
    return;
  }
}

void SchematicView::click_netlist_bar(float x) {
  if (interrupt_x0_ >= 0.0F && x >= interrupt_x0_) {
    interrupt();
  } else if (x <= netlist_button_x1_ && netlist_.enabled()) {
    start_netlist();
  }
}

// --- painting -----------------------------------------------------------------------------

Rgba SchematicView::instance_color(const ThemeTokens& t, std::size_t index) const {
  if (index < instance_colours_.size() && instance_colours_[index] >= 0) {
    return highlight_color(t, instance_colours_[index]);
  }
  if (index < scene_.instances.size() && scene_.instances[index].pending) return t.text_tertiary;
  return source_color(t, SceneSource::symbol);
}

Rgba SchematicView::wire_color(const ThemeTokens& t, std::size_t index) const {
  if (index < wire_colours_.size() && wire_colours_[index] >= 0) return highlight_color(t, wire_colours_[index]);
  return source_color(t, SceneSource::wire);
}

void SchematicView::paint_canvas(ui::Painter& p, float w, float h, const ThemeTokens& t) {
  p.fill(0.0F, 0.0F, w, h, 0.0F, t.canvas);
  viewport_.set_size(static_cast<double>(w), static_cast<double>(h));
  if (!input_.user_view()) viewport_.fit(scene_.box);  // fitted until the user pans or zooms
  if (!has_document_) {
    p.text_centered_y(TextRole::body, kPadX, h / 2.0F, kEmptyHint, t.text_tertiary);
    return;
  }
  p.push_clip(0.0F, 0.0F, w, h);
  paint_items(p, t);
  p.pop_clip();
  if (const std::size_t pending = resolver_.pending_count(); pending > 0) {
    const std::string note = "Resolving " + std::to_string(pending) + (pending == 1 ? " symbol…" : " symbols…");
    p.text(TextRole::meta, kPadX, 8.0F, note, t.text_tertiary);
  }
}

void SchematicView::paint_items(ui::Painter& p, const ThemeTokens& t) const {
  const ScenePainter painter{p, viewport_, [&](SceneSource source, std::size_t index) {
                               if (source == SceneSource::wire) return wire_color(t, index);
                               if (source == SceneSource::symbol) return instance_color(t, index);
                               return source_color(t, source);
                             }};
  for (const SceneSegment& s : scene_.segments) {
    const bool lit = s.source == SceneSource::wire && s.index < wire_colours_.size() && wire_colours_[s.index] >= 0;
    painter.segment(s.a, s.b, painter.color(s.source, s.index), lit ? 2.0F * kStroke : kStroke);
  }
  for (const ScenePolygon& poly : scene_.polygons) painter.polygon(poly);
  for (const SceneArc& arc : scene_.arcs) painter.arc(arc);
  for (const SceneText& text : scene_.texts) {
    const std::optional<double> size = scene_text_px(text.size, viewport_.zoom());  // follows the zoom
    if (!size) continue;
    const auto [x, y] = painter.at(text.origin);
    p.text_px(TextRole::meta, static_cast<float>(*size), x, y, text.text, t.text_secondary);
  }
}

void SchematicView::paint_crumbs(ui::Painter& p, float w, float h, const ThemeTokens& t) {
  paint::fill_rect(p, w, h, t.bar);
  crumb_spans_.clear();
  float x = kPadX;
  const std::vector<Crumb> crumbs = breadcrumb_.crumbs();
  for (std::size_t i = 0; i < crumbs.size(); ++i) {
    const float width = p.text_width(TextRole::body, crumbs[i].label);
    const bool last = i + 1 == crumbs.size();
    p.text_centered_y(TextRole::body, x, h / 2.0F, crumbs[i].label,
                      !has_document_ ? t.text_tertiary : (last ? t.text_primary : t.accent_soft));
    crumb_spans_.push_back(CrumbSpan{x, x + width});
    x += width;
    if (!last) p.text_centered_y(TextRole::body, x + 4.0F, h / 2.0F, "›", t.text_tertiary);
    x += kCrumbGap;
  }
}

void SchematicView::paint_netlist_bar(ui::Painter& p, float w, float h, const ThemeTokens& t) {
  paint::fill_rect(p, w, h, t.surface1);
  p.fill(0.0F, 0.0F, w, 1.0F, 0.0F, t.border);
  const float bw = paint::button_width(ctx_.runtime, "Netlist", false);
  paint::button_at(p, kPadX, (h - kBarButtonHeight) / 2.0F, bw, kBarButtonHeight, "Netlist", paint::ButtonStyle::primary,
                   paint::WidgetState{false, false, has_document_ && netlist_.enabled(), false}, t);
  netlist_button_x1_ = kPadX + bw;
  float x = kPadX + bw + kPadX;
  p.text_centered_y(TextRole::small, x, h / 2.0F, "Simulate · arrives in M9", t.text_tertiary);
  x += p.text_width(TextRole::small, "Simulate · arrives in M9") + 2.0F * kPadX;
  const auto card = netlist_.card(NetlistAction::Clock::now());
  float interrupt_x0 = -1.0F;
  if (card) {
    const std::string label = netlist_.cancel_requested() ? "Stopping…" : card->action + " ⌃C";
    card_seen_ = true;
    const float cw = paint::button_width(ctx_.runtime, label, false);
    interrupt_x0 = w - kPadX - cw;
    p.text_centered_y(TextRole::body_strong, x, h / 2.0F, card->title + "  " + card->detail, t.text_primary);
    paint::button_at(p, interrupt_x0, (h - kBarButtonHeight) / 2.0F, cw, kBarButtonHeight, label,
                     paint::ButtonStyle::primary, paint::WidgetState{false, false, !netlist_.cancel_requested(), false},
                     t);
  } else if (!netlist_.status().empty()) {
    const bool failed = netlist_.state() == NetlistAction::State::failed;
    p.text_centered_y(TextRole::small, x, h / 2.0F, (failed ? "■ " : "● ") + netlist_.status(),
                      status_text_color(t, failed ? StatusKind::error : StatusKind::success));
  }
  interrupt_x0_ = interrupt_x0;
}

void SchematicView::paint_messages(ui::Painter& p, float w, float h, const ThemeTokens& t) const {
  paint::fill_rect(p, w, h, t.surface0);
  p.fill(0.0F, 0.0F, w, 1.0F, 0.0F, t.border);
  paint::section_label(p, kPadX, 6.0F, "Messages · " + messages_.summary(), t);
  const std::vector<Message>& all = messages_.messages();
  const float top = 26.0F;
  const auto fit = static_cast<std::size_t>(std::max(0.0F, (h - top) / 18.0F));
  const std::size_t first = all.size() > fit ? all.size() - fit : 0;  // newest last
  float y = top;
  for (std::size_t i = first; i < all.size(); ++i, y += 18.0F) {
    const auto [kind, line] = message_line(all[i]);
    p.text(TextRole::mono_small, kPadX, y, line, status_text_color(t, kind));
  }
}

void SchematicView::paint_library_rows(ui::Painter& p, float w, const ThemeTokens& t) const {
  const std::vector<LibraryEntry>& rows = library_.visible();
  if (rows.empty()) {
    p.text_centered_y(TextRole::small, kPadX, kRowHeight / 2.0F, "No symbols", t.text_tertiary);
    return;
  }
  const TextMeasure body = [&p](std::string_view text) { return p.text_width(TextRole::body, text); };
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const float y = static_cast<float>(i) * kRowHeight;
    const std::string& folder = library_.folders()[rows[i].folder].label;
    const float fw = p.text_width(TextRole::meta, folder);
    // A long name is shortened in the middle so it never runs under the folder (round 7 capture review).
    const std::string name = ellipsize_middle(rows[i].name, std::max(0.0F, w - 3.0F * kPadX - fw), body);
    p.text_centered_y(TextRole::body, kPadX, y + kRowHeight / 2.0F, name, t.text_primary);
    p.text_centered_y(TextRole::meta, w - kPadX - fw, y + kRowHeight / 2.0F, folder, t.text_tertiary);
  }
}

void SchematicView::paint_property_rows(ui::Painter& p, float w, float h, const ThemeTokens& t) const {
  const std::vector<PropertyRow>& rows = properties_.rows();
  if (properties_.target() == PropertyTarget::none) {
    p.text_centered_y(TextRole::small, kPadX, kRowHeight / 2.0F, "Click an instance to see its properties",
                      t.text_tertiary);
  } else if (properties_.editable()) {
    const float y = static_cast<float>(rows.size()) * kRowHeight + kRowHeight / 2.0F;
    p.text_centered_y(TextRole::small, kPadX, y, "+ Add attribute", t.accent_soft);
  }
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const float y = static_cast<float>(i) * kRowHeight + kRowHeight / 2.0F;
    p.text_centered_y(TextRole::small, kPadX, y, rows[i].key, t.text_secondary);
    p.text_centered_y(TextRole::mono, w * 0.4F, y, rows[i].has_value ? rows[i].value : "", t.text_primary);
  }
  // Board review (task 13.1): the M9 parts are shown disabled.
  p.text_centered_y(TextRole::small, kPadX, h - kRowHeight / 2.0F, "Operating point · arrives in M9", t.text_tertiary);
}

}  // namespace hefesto::shell
