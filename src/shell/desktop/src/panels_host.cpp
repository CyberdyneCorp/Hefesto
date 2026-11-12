// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The panel layout on screen (board M2-Panels; capability hefesto-app-ux
// "Panel layout and focus cycling"; M4 CARRY task 11.2). The layout is the
// PanelLayout model (panels_layout.hpp); this file arranges the main
// window's panels from it:
//   - each panel (its strip leader: Layers | Cells | Libraries, Console |
//     Keys) sits in a frame with a 32 px header (title, collapse and float
//     buttons; Dock on a floating one; the context menu docks it anywhere,
//     the keyboard alternative to dragging);
//   - docked left or right of the canvas, under it (bottom), or floating as
//     a card over the canvas, kept inside it;
//   - a collapsed panel keeps its header row; a side dock with no expanded
//     panel, or one the window is too narrow for (fit_to_window), shows its
//     32 px edge strip, whose entries run `panel NAME show`;
//   - the `panel` command (run_panel_command) is the only way the layout
//     changes, so every change is logged and replayable;
//   - the layout is restored at launch from panel-layout.json in the user's
//     storage folder and saved after each change and on quit (not in a
//     scripted capture, which must not touch the user's settings).
//   - dragging a header (Painted::on_drag, Raven's in-application drag with
//     the title following the pointer) over a dock, the canvas column or an
//     edge strip shows Raven's drop highlight there; the release issues the
//     PanelDrag's `panel ID dock ZONE` or `panel ID float X Y W H`, Escape
//     cancels and nothing changes.
//
// Owner: CARRY (openspec/changes/m4-scripting/design.md D2).

#include <algorithm>
#include <cstdio>
#include <utility>

#include "hefesto/cmd/context.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/platform/session_log.hpp"
#include "main_window.hpp"

namespace hefesto::shell {

namespace {

constexpr float kHeaderButton = 24.0F;  // a header button's square
constexpr float kHeaderPad = 8.0F;
constexpr float kStripRow = 16.0F;      // an edge strip's row: one letter of a title
constexpr std::array<DockZone, 3> kDocks{DockZone::left, DockZone::right, DockZone::bottom};

std::size_t zone_slot(DockZone zone) { return static_cast<std::size_t>(zone); }

/// The frames' panel words, in build order (strip leaders only).
constexpr std::array<std::string_view, 5> kFrameIds{"layers", "drc", "inspector", "techmanager", "console"};

/// The type of a header drag (Raven's drag type; its data is the panel word).
constexpr std::string_view kPanelDragType = "hefesto/panel";

LayoutRect layout_rect(const ui::Rect& r) { return LayoutRect{r.x, r.y, r.width, r.height}; }

/// Draws a header button's glyph (the icon set has no chevrons or dock glyphs).
void header_glyph(ui::Painter& p, std::string_view icon, float x, float y, const Rgba& c) {
  const float m = kHeaderButton / 2.0F;
  if (icon == "chevron-down") {
    p.line(x + m - 4.0F, y + m - 2.0F, x + m, y + m + 2.0F, 1.5F, c);
    p.line(x + m, y + m + 2.0F, x + m + 4.0F, y + m - 2.0F, 1.5F, c);
  } else if (icon == "chevron-right") {
    p.line(x + m - 2.0F, y + m - 4.0F, x + m + 2.0F, y + m, 1.5F, c);
    p.line(x + m + 2.0F, y + m, x + m - 2.0F, y + m + 4.0F, 1.5F, c);
  } else if (icon == "float") {  // a window over a window
    p.stroke(x + 5.0F, y + 9.0F, 10.0F, 10.0F, 1.0F, 1.0F, c);
    p.fill(x + 9.0F, y + 5.0F, 10.0F, 10.0F, 1.0F, c);
  } else {  // dock: a window with its dock bar
    p.stroke(x + 5.0F, y + 6.0F, 14.0F, 12.0F, 1.0F, 1.0F, c);
    p.fill(x + 5.0F, y + 6.0F, 5.0F, 12.0F, 1.0F, c);
  }
}

}  // namespace

// --- building -----------------------------------------------------------------------------

void MainWindow::build_panels() {
  // The right dock's exclusive views share one slot (update_right_dock).
  right_stack_.add(inspector_->widget(), 1.0F);
  right_stack_.add(pdk_->widget(), 1.0F);
  right_stack_.add(color_->widget(), 1.0F);
  right_stack_.add(drc_panel_->inspector_widget(), 1.0F);
  const auto content = [this](std::string_view id) -> ui::Widget* {
    if (id == "layers") return &left_dock();
    if (id == "drc") return &drc_panel_->list_widget();
    if (id == "inspector") return &right_stack_;
    if (id == "techmanager") return &managers_->tech_widget();
    return &console_tabs_;
  };
  for (const std::string_view id : kFrameIds) {
    auto frame = std::make_unique<PanelFrame>();
    frame->id = std::string(id);
    frame->content = content(id);
    frame->card.emplace(ui::Panel::create(ctx_.tokens().surface1));
    const std::string word = frame->id;
    frame->header = make_interactive(ctx_, [this, word](ui::Painter& p, float w, float h, const ThemeTokens& t,
                                                        bool hov, bool foc) {
      const PanelHeader hd = panel_header(panels_, word, frame_title(word));
      p.fill(0.0F, 0.0F, w, h, 0.0F, t.bar);
      p.fill(0.0F, h - 1.0F, w, 1.0F, 0.0F, t.border);
      if (foc) p.stroke(1.0F, 1.0F, w - 2.0F, h - 2.0F, 2.0F, 1.0F, t.accent);
      p.text_centered_y(TextRole::section_label, kHeaderPad, h / 2.0F, hd.title, hov ? t.text_primary : t.text_tertiary);
      float x = w - kHeaderPad - kHeaderButton * static_cast<float>(hd.buttons.size());
      for (const PanelHeaderButton& b : hd.buttons) {
        header_glyph(p, b.icon, x, (h - kHeaderButton) / 2.0F, t.icon);
        x += kHeaderButton;
      }
    });
    frame->header->set_size(-1.0F, kHeaderHeight);
    frame->header->set_role(ui::Role::button);
    frame->header->set_accessible_description("Enter collapses or expands; the context menu docks or floats it");
    frame->header->on_click([this, word](float x, float) { header_click(word, x); });
    frame->header->on_context([this, word](float x, float y) { open_panel_menu(word, x, y); });
    frame->header->on_drag([this, word](float x, float y) { return begin_panel_drag(word, x, y); });
    frame->box.add(*frame->header);
    frame->box.add(*frame->content, 1.0F);
    frame->card->set_child(frame->box);
    frames_.push_back(std::move(frame));
  }
  for (const DockZone zone : kDocks) {
    const std::size_t slot = zone_slot(zone);
    auto strip = make_interactive(ctx_, [this, slot](ui::Painter& p, float w, float h, const ThemeTokens& t, bool hov,
                                                     bool foc) {
      p.fill(0.0F, 0.0F, w, h, 0.0F, t.bar);
      if (foc) p.stroke(1.0F, 1.0F, w - 2.0F, h - 2.0F, 2.0F, 1.0F, t.accent);
      const float line = kStripRow;
      float y = 12.0F;
      for (const std::string& id : strip_panels_[slot]) {
        const std::string title = frame_title(id);
        for (const char c : title) {  // the title down the strip, one letter a row
          p.text(TextRole::small, (w - p.text_width(TextRole::small, std::string(1, c))) / 2.0F, y,
                 std::string(1, c), hov ? t.text_primary : t.text_secondary);
          y += line;
        }
        y += 16.0F;
      }
    });
    strip->set_role(ui::Role::button);
    strip->on_click([this, slot](float, float y) { strip_click(slot, y); });
    strip->set_visible(false);
    strips_[slot] = std::move(strip);
  }
  const auto drop = [this](ui::DropZone::Phase phase, float x, float y, std::string_view type, std::string_view) {
    return panel_drop(phase, x, y, type);
  };
  for (const ui::Widget* zone : {static_cast<const ui::Widget*>(&side_docks_[0]),
                                 static_cast<const ui::Widget*>(&side_docks_[1]), static_cast<const ui::Widget*>(&center_)}) {
    drop_zones_.push_back(std::make_unique<ui::DropZone>(*zone, drop));
  }
  for (const auto& strip : strips_) drop_zones_.push_back(std::make_unique<ui::DropZone>(*strip, drop));
  // The saved layout of the last session (first launch: the default).
  const platform::Path file = platform::default_storage_dir() / std::string(kPanelLayoutFile);
  RestoredLayout restored = restore_panel_layout(ctx_.core.files(), file);
  panels_ = std::move(restored.layout);
  if (!restored.warning.empty()) set_message(StatusKind::warning, diagnostic_line(StatusKind::warning, restored.warning));
  apply_panel_flags({});
  arrange_middle();
}

void MainWindow::register_panel_command() {
  const auto added = ctx_.core.registry().add(
      {"panel", std::string(panel_command_usage()),
       "Shows, hides (collapses) or toggles a panel of the main window, docks it left, right or at the bottom, "
       "floats it over the canvas, or restores the default layout. The layout is saved per user.",
       [this](cmd::CommandContext&, const cmd::Invocation& inv) { return panel_command(inv.args); }});
  if (added) std::fprintf(stderr, "hefesto: the panel command is already registered\n");
}

// --- the `panel` command --------------------------------------------------------------------

cmd::CommandResult MainWindow::panel_command(const std::vector<std::string>& args) {
  sync_panels_from_flags();
  const PanelCommandResult r = run_panel_command(panels_, args, panel_window_rect());
  if (!r.ok) {
    return cmd::CommandResult::failure(diag::make(diag::codes::kCmdUsage, "usage: " + r.message));
  }
  if (r.changed) {
    apply_panel_flags(args);
    update_right_dock();  // also arranges the docks
    save_panels();
  }
  return cmd::CommandResult::success(r.message);
}

void MainWindow::sync_panels_from_flags() {
  const auto set = [this](std::string_view id, bool shown) {
    if (PanelPlacement* p = panels_.find(id)) p->collapsed = !shown;
  };
  set("layers", layers_visible_);
  set("inspector", inspector_visible_);
  set("drc", drc_visible_);
  set("techmanager", tech_shown_);
  set("console", console_visible_);
  set("keys", keys_shown_);
}

void MainWindow::apply_panel_flags(const std::vector<std::string>& args) {
  const auto shown = [this](std::string_view id) {
    const PanelPlacement* p = panels_.find(id);
    return p != nullptr && !p->collapsed;
  };
  layers_visible_ = shown("layers");
  inspector_visible_ = shown("inspector");
  tech_shown_ = shown("techmanager");
  if (drc_visible_ != shown("drc")) {
    drc_visible_ = shown("drc");
    drc_rows_changed_ = true;
    dirty_ = true;  // the list is built on the next refresh
  }
  if (console_visible_ != shown("console")) toggle_console();
  keys_shown_ = shown("keys");
  // The tab a command named comes to the front.
  const std::string named = args.empty() ? std::string() : args[0];
  const bool showing = args.size() < 2 || args[1] != "hide";
  if (named == "keys" && keys_shown_) {
    refresh_keys();
    console_tabs_.show(3);
    open_key_sheet();  // the sheet over the canvas, the full list in the Keys tab
  } else if (named == "keys") {
    close_key_sheet();
    if (console_visible_) console_tabs_.show(0);
  }
  if (showing && named == "cells") left_tabs_.show(1);
  if (showing && named == "libraries") left_tabs_.show(2);
  if (showing && named == "layers") left_tabs_.show(0);
}

// --- arranging ---------------------------------------------------------------------------------

MainWindow::FrameState MainWindow::frame_state(const std::string& id) const {
  if (id == "layers") {
    // While the DRC browser is shown in the same dock it takes the Layers' place.
    if (drc_visible_ && same_dock("drc", "layers")) return FrameState::hidden;
    return layers_visible_ ? FrameState::expanded : FrameState::header;
  }
  if (id == "inspector") {
    const bool any = pdk_visible_ || color_visible_ || inspector_visible_;
    return any ? FrameState::expanded : FrameState::header;
  }
  // On-demand panels (drc, techmanager, console): shown or not there at all.
  bool shown = console_visible_;
  if (id == "drc") shown = drc_visible_;
  if (id == "techmanager") shown = tech_shown_;
  return shown ? FrameState::expanded : FrameState::hidden;
}

bool MainWindow::same_dock(std::string_view a, std::string_view b) const {
  const PanelPlacement* pa = panels_.find(a);
  const PanelPlacement* pb = panels_.find(b);
  return pa != nullptr && pb != nullptr && pa->dock == pb->dock;
}

std::string MainWindow::frame_title(const std::string& id) const {
  if (id == "layers") return "Layers";
  if (id == "cells") return "Cells";
  if (id == "libraries") return "Libraries";
  if (id == "drc") return "DRC";
  if (id == "techmanager") return "Technology";
  if (id == "console" || id == "keys") return id == "keys" ? "Keys" : "Console";
  if (pdk_visible_) return "PDKs";
  if (color_visible_) return "Color";
  return "Inspector";
}

void MainWindow::arrange_middle() {
  if (frames_.empty()) {  // build_center, before build_panels: the canvas column only
    middle_.insert(center_, 2, 1.0F);
    return;
  }
  // rail, rail edge, then: left dock (or its strip), canvas column, right dock.
  int index = 2;
  place_side(DockZone::left, index);
  middle_.insert(center_, index++, 1.0F);
  place_side(DockZone::right, index);
  // The bottom dock under the canvas (after it and the banners over it), in dock order.
  for (const std::string& id : panels_.in(DockZone::bottom)) {
    PanelFrame* f = frame(id);
    if (f == nullptr) continue;
    center_.add(*f->card);  // moves it to the end
    f->card->place_fill();  // a panel that floated keeps its overlay placement otherwise
    f->card->set_fill(ctx_.tokens().surface1);
    f->card->set_padding(0.0F, 0.0F, 0.0F, 0.0F);
    f->card->set_size(-1.0F, id == "console" ? static_cast<float>(kConsoleHeight) : panels_.bottom_height);
    const FrameState state = frame_state(id);
    f->card->set_visible(state != FrameState::hidden);
    f->content->set_visible(state == FrameState::expanded);
  }
  for (const std::string& id : panels_.in(DockZone::floating)) {
    if (PanelFrame* f = frame(id)) place_floating(*f);
  }
  const auto side = [](const PanelPlacement* p) {
    return p != nullptr && p->dock == DockZone::right ? DockSide::right : DockSide::left;
  };
  left_->set_dock_side(side(panels_.find("layers")));
  inspector_->set_dock_side(side(panels_.find("inspector")));
}

MainWindow::PanelFrame* MainWindow::frame(std::string_view id) {
  for (auto& f : frames_) {
    if (f->id == id) return f.get();
  }
  return nullptr;
}

void MainWindow::place_side(DockZone zone, int& index) {
  const std::size_t slot = zone_slot(zone);
  const bool narrow = zone == DockZone::left ? narrow_left_ : narrow_right_;
  ui::Box& dock = side_docks_[slot];
  std::vector<PanelFrame*> frames;
  for (const std::string& id : panels_.in(zone)) {
    if (PanelFrame* f = frame(id)) frames.push_back(f);
  }
  strip_panels_[slot].clear();
  bool any_expanded = false;
  for (const PanelFrame* f : frames) {
    const FrameState state = frame_state(f->id);
    any_expanded = any_expanded || state == FrameState::expanded;
    if (state != FrameState::hidden) strip_panels_[slot].push_back(f->id);
  }
  // The edge strip: the window is too narrow for the dock, or every panel in it is collapsed.
  const bool strip = !strip_panels_[slot].empty() && (narrow || !any_expanded);
  const float width = static_cast<float>(zone == DockZone::left ? kLeftPanelWidth : kInspectorWidth);
  int row = 0;
  for (PanelFrame* f : frames) {
    const FrameState state = frame_state(f->id);
    // Expanded panels share the dock's height; a collapsed one keeps its header row.
    dock.insert(*f->card, row++, state == FrameState::expanded ? 1.0F : 0.0F);
    f->card->place_fill();  // a panel that floated keeps its overlay placement otherwise
    f->card->set_fill(ctx_.tokens().surface1);
    f->card->set_padding(0.0F, 0.0F, 0.0F, 0.0F);
    f->card->set_size(state == FrameState::header ? width : -1.0F, -1.0F);
    f->card->set_visible(state != FrameState::hidden);
    f->content->set_visible(state == FrameState::expanded);
  }
  middle_.insert(dock, index++);
  dock.set_visible(!strip && !strip_panels_[slot].empty());
  ui::Painted& strip_widget = *strips_[slot];
  middle_.insert(strip_widget, index++);
  strip_widget.set_size(kEdgeStripWidth, -1.0F);
  strip_widget.set_visible(strip);
  std::string names;
  for (const std::string& id : strip_panels_[slot]) names += (names.empty() ? "" : ", ") + frame_title(id);
  strip_widget.set_accessible_name("Collapsed " + std::string(dock_zone_name(zone)) + " dock: " + names);
  strip_widget.set_tooltip("Expand " + names);
  strip_widget.invalidate();
}

void MainWindow::place_floating(PanelFrame& f) {
  const PanelPlacement* p = panels_.find(f.id);
  if (p == nullptr) return;
  const ui::Rect area = canvas_->widget().rect();
  // The card stays on the canvas, the area it floats over.
  const LayoutRect canvas{area.x, area.y, area.width, area.height};
  const LayoutRect r = area.width > 0.0F ? clamp_floating(p->floating, canvas) : p->floating;
  canvas_->add_overlay(*f.card);
  f.card->set_size(r.w, r.h);
  // The popover elevation as a 1 px outline (Raven panels draw no radius or shadow).
  f.card->set_fill(ctx_.tokens().control_border);
  f.card->set_padding(1.0F, 1.0F, 1.0F, 1.0F);
  f.card->place_top_right(std::max(0.0F, area.x + area.width - r.x - r.w), std::max(0.0F, r.y - area.y));
  // A floating panel is shown whole, or not at all while it is collapsed.
  const bool shown = frame_state(f.id) == FrameState::expanded;
  f.card->set_visible(shown);
  f.content->set_visible(shown);
}

void MainWindow::fit_panels() {
  const int width = window_.logical_width();
  if (width <= 0 || width == panels_fit_width_ || frames_.empty()) return;
  panels_fit_width_ = width;
  PanelLayout open = panels_;
  open.strip = {false, false, false};
  const PanelLayout fitted = fit_to_window(open, static_cast<float>(width));
  const bool left = fitted.strip[zone_slot(DockZone::left)];
  const bool right = fitted.strip[zone_slot(DockZone::right)];
  if (left == narrow_left_ && right == narrow_right_) return;
  narrow_left_ = left;
  narrow_right_ = right;
  arrange_middle();
}

LayoutRect MainWindow::panel_window_rect() const {
  const ui::Rect r = root_.rect();
  return LayoutRect{0.0F, 0.0F, std::max(r.width, 1024.0F), std::max(r.height, 640.0F)};
}

// --- headers and strips ------------------------------------------------------------------------

void MainWindow::header_click(const std::string& id, float x) {
  const PanelHeader hd = panel_header(panels_, id, frame_title(id));
  for (const auto& f : frames_) {
    if (f->id != id) continue;
    const float w = f->header->rect().width;
    const float first = w - kHeaderPad - kHeaderButton * static_cast<float>(hd.buttons.size());
    if (x >= first && x < w - kHeaderPad) {
      const auto i = static_cast<std::size_t>((x - first) / kHeaderButton);
      run_line(hd.buttons[std::min(i, hd.buttons.size() - 1)].command);
      return;
    }
  }
  // The title (or Enter): collapse or expand; a floating panel docks back.
  if (!hd.buttons.empty()) run_line(hd.buttons.front().command);
}

void MainWindow::open_panel_menu(const std::string& id, float x, float y) {
  const PanelPlacement* p = panels_.find(id);
  if (p == nullptr) return;
  std::vector<MenuEntry> entries;
  for (const DockZone zone : kDocks) {
    const std::string word(dock_zone_name(zone));
    MenuEntry e;
    e.label = "Dock " + word;
    e.command = "panel " + id + " dock " + word;
    e.checked = p->dock == zone;
    entries.push_back(std::move(e));
  }
  MenuEntry floating;
  floating.label = "Float";
  floating.command = "panel " + id + " float";
  floating.checked = p->dock == DockZone::floating;
  entries.push_back(std::move(floating));
  MenuEntry reset;
  reset.label = "Reset layout";
  reset.command = "panel reset";
  reset.separator_before = true;
  entries.push_back(std::move(reset));
  open_menu(frame_title(id), std::move(entries), x, y);
}

void MainWindow::strip_click(std::size_t zone, float y) {
  const std::vector<std::string>& ids = strip_panels_[zone];
  if (ids.empty()) return;
  // Entries are stacked as painted: 12 px, a row per letter, 16 px between.
  const float line = kStripRow;
  float top = 12.0F;
  for (const std::string& id : ids) {
    const float bottom = top + line * static_cast<float>(frame_title(id).size()) + 16.0F;
    if (y < bottom) {
      run_line("panel " + id + " show");
      return;
    }
    top = bottom;
  }
  run_line("panel " + ids.back() + " show");
}

// --- dragging a header ---------------------------------------------------------------------------

std::optional<ui::Painted::DragData> MainWindow::begin_panel_drag(const std::string& id, float x, float y) {
  const PanelFrame* f = frame(id);
  if (f == nullptr || panels_.find(id) == nullptr) return std::nullopt;
  panel_drag_.emplace(id, x, y, layout_rect(f->card->rect()));
  return ui::Painted::DragData{std::string(kPanelDragType), id, frame_title(id)};
}

// Raven's drop zones only report where the pointer is: they refuse the
// drop, so Raven draws no highlight of its own (it would tint the whole dock
// or canvas column) and the band is the board's; the release ends the drag
// through the window hook (end_panel_drag).
bool MainWindow::panel_drop(ui::DropZone::Phase phase, float x, float y, std::string_view type) {
  if (type != kPanelDragType || !panel_drag_) return false;
  drop_band_->invalidate();
  if (phase == ui::DropZone::Phase::leave) {  // no band until the pointer is over a zone again
    panel_drag_->cancel();
  } else {
    (void)panel_drag_->move(x, y, dock_geometry());
  }
  return false;
}

void MainWindow::end_panel_drag() {
  if (!panel_drag_) return;
  const std::optional<std::string> command = panel_drag_->drop_command();
  panel_drag_.reset();
  drop_band_->invalidate();
  // After Raven's event returns: the command rearranges the widgets.
  if (command) {
    run_when_ready(*command);
    window_.request_redraw();
  }
}

// After an --input script: "hefesto: input done", then each shown panel's
// frame in window units, for scripted checks (desktop-window tests).
void MainWindow::report_panel_rects() const {
  std::printf("hefesto: input done\n");
  for (const auto& f : frames_) {
    if (frame_state(f->id) == FrameState::hidden) continue;
    const ui::Rect r = f->card->rect();
    std::printf("hefesto: panel %s %.0f %.0f %.0f %.0f\n", f->id.c_str(), static_cast<double>(r.x),
                static_cast<double>(r.y), static_cast<double>(r.width), static_cast<double>(r.height));
  }
  std::fflush(stdout);
}

// Board M2-Panels tile 2: the target dock as a dashed ember zone with a 14 %
// ember tint, its caption and, over the dock the panel left, a note; a
// floating target as the window's dashed outline. Drawn over the whole
// window (the docks lie outside the canvas) and never hit.
void MainWindow::build_drop_band() {
  drop_band_ = ui::Painted::create(ctx_.runtime, [this](ui::Painter& p, float, float) { paint_drop_band(p); });
  drop_band_->set_hit_testable(false);
  window_root_.add(root_);
  window_root_.add(*drop_band_);
  window_.on_drag_release([this](float, float) { end_panel_drag(); });
}

void MainWindow::paint_drop_band(ui::Painter& p) const {
  if (!panel_drag_ || !panel_drag_->target() || !window_.drag_active()) return;
  const DropTarget& target = *panel_drag_->target();
  const PanelPlacement* placement = panels_.find(panel_drag_->id());
  const LayoutRect r = drop_band_rect(target, panels_);
  const ThemeTokens& t = ctx_.tokens();
  const float inset = 4.0F;
  const float x = r.x + inset;
  const float y = r.y + inset;
  const float w = r.w - 2.0F * inset;
  const float h = r.h - 2.0F * inset;
  if (target.zone == DockZone::floating || placement == nullptr) {
    p.dashed_rect(x, y, w, h, 1.0F, 6.0F, 4.0F, t.accent);
    return;
  }
  // The zone covers the dock (or the canvas edge it would take): it reads
  // as an empty dock, as on the board, with the 14 % ember tint over it.
  Rgba base = t.surface0;
  base.a = 0.92F;
  Rgba tint = t.accent;
  tint.a = 0.14F;
  p.fill(x, y, w, h, 6.0F, base);
  p.fill(x, y, w, h, 6.0F, tint);
  p.dashed_rect(x, y, w, h, 1.0F, 6.0F, 4.0F, t.accent);
  const DropBandText text = drop_band_text(target, *placement, frame_title(panel_drag_->id()));
  float cy = y + h / 2.0F - 24.0F;
  const float ix = x + (w - 16.0F) / 2.0F;  // the dock glyph of the board: a panel with its side bar
  p.stroke(ix, cy, 16.0F, 14.0F, 2.0F, 1.25F, t.accent);
  p.line(ix + 10.0F, cy, ix + 10.0F, cy + 14.0F, 1.25F, t.accent);
  cy += 24.0F;
  for (const auto& [line, color] : {std::pair{text.caption, t.accent}, std::pair{text.note, t.text_secondary}}) {
    if (line.empty()) continue;
    const float lw = p.text_width(TextRole::small, line);
    p.text(TextRole::small, x + (w - std::min(lw, w)) / 2.0F, cy, line, color);
    cy += p.line_height(TextRole::small) + 4.0F;
  }
}

DockGeometry MainWindow::dock_geometry() const {
  DockGeometry g;
  g.window = panel_window_rect();
  g.canvas = layout_rect(canvas_->widget().rect());
  // A side dock is its box, or its edge strip while it shows one (a drop on
  // the strip docks there); an empty dock is the band at the canvas edge.
  const auto dock = [this](DockZone zone) {
    const std::size_t slot = zone_slot(zone);
    if (strip_panels_[slot].empty()) return LayoutRect{};
    const LayoutRect strip = layout_rect(strips_[slot]->rect());
    return strip.w > 0.0F ? strip : layout_rect(side_docks_[slot].rect());
  };
  g.left = dock(DockZone::left);
  g.right = dock(DockZone::right);
  for (const std::string& id : panels_.in(DockZone::bottom)) {
    const auto it = std::find_if(frames_.begin(), frames_.end(), [&](const auto& f) { return f->id == id; });
    if (it != frames_.end() && frame_state(id) != FrameState::hidden) {
      const LayoutRect r = layout_rect((*it)->card->rect());
      g.bottom = g.bottom.w > 0.0F ? LayoutRect{g.bottom.x, g.bottom.y, g.bottom.w, g.bottom.h + r.h} : r;
    }
  }
  return g;
}

// --- saving ------------------------------------------------------------------------------------

void MainWindow::save_panels() {
  if (capture_mode_) return;  // a scripted capture leaves the user's layout alone
  sync_panels_from_flags();
  const platform::Path file = platform::default_storage_dir() / std::string(kPanelLayoutFile);
  if (const diag::Status saved = save_panel_layout(ctx_.core.files(), file, panels_); !saved) {
    set_message(StatusKind::warning,
                diagnostic_line(StatusKind::warning, "panel layout not saved: " + saved.error().text));
  }
}

}  // namespace hefesto::shell
