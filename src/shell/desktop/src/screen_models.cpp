// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/screen_models.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

#include "hefesto/view/color_window.hpp"

namespace hefesto::shell {

namespace {

std::string lower(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

bool contains(std::string_view haystack, std::string_view needle) {
  return needle.empty() || lower(haystack).find(lower(needle)) != std::string::npos;
}

/// A step's name as a menu shows it: its label (UndoLog::step_label), cut
/// to a menu's width (an unlabelled step is named by its newest event, which
/// names the cell and the areas).
std::string menu_step_name(const std::string& label) {
  constexpr std::size_t kMaxName = 60;  // undo::kMaxStepLabelLength
  if (label.empty()) return "(empty step)";
  if (label.size() <= kMaxName) return label;
  std::size_t cut = kMaxName - 3;  // room for "…"; never inside a UTF-8 sequence
  while (cut > 0 && (static_cast<unsigned char>(label[cut]) & 0xC0U) == 0x80U) --cut;
  return label.substr(0, cut) + "…";
}

std::string quote_macro(std::string_view text) {
  std::string out = "\"";
  for (const char c : text) {
    if (c == '"' || c == '\\' || c == '$' || c == '[' || c == ']') out += '\\';
    out += c;
  }
  return out + "\"";
}

std::string table_label(std::string_view table) {
  if (table == "box" || table == "layout") return "Layout (box tool)";
  if (table == "wiring") return "Wiring tool";
  if (table == "pick") return "Pick tool";
  if (table == "nettool") return "Netlist tool";
  if (table == "color") return "Color window";
  if (table == "windows" || table == "*window") return "Every window";
  std::string label(table);
  if (!label.empty()) label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
  return label;
}

std::string octal(int value) {
  char buf[8];
  std::snprintf(buf, sizeof buf, "%03o", static_cast<unsigned>(value));
  return buf;
}

std::string fixed3(double v) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "%.3f", v);
  return buf;
}

/// `setpoint X Y ID; pushbutton left` at a surface point of a color window.
std::string press_line(editor::EditorState& editor, const view::ColorWindow& window, db::Point point) {
  const view::ViewMapping m = view::color_window_mapping(editor, window);
  return "setpoint " + std::to_string(m.screen_x(point.x)) + " " + std::to_string(m.screen_y(point.y)) + " " +
         std::to_string(window.id) + "; pushbutton left";
}

db::Point center(const db::Rect& r) {
  return db::Point{r.x0 + (r.x1 - r.x0) / 2, r.y0 + (r.y1 - r.y0) / 2};
}

// The bar colors of the board (Magic's red, green, blue, yellow, gray and
// brown-1 display styles).
constexpr std::array<std::uint32_t, 6> kBarColors = {0xE05252U, 0x3DB46DU, 0x4F8EF7U, 0xE3C53BU, 0x8E96A3U, 0xA8743AU};

}  // namespace

// --- command bar popovers ---------------------------------------------------------

std::vector<MenuEntry> history_entries(const std::vector<std::string>& history, std::string_view filter,
                                       std::size_t max) {
  std::vector<MenuEntry> entries;
  for (std::size_t i = history.size(); i > 0 && entries.size() < max; --i) {
    const std::string& line = history[i - 1];
    if (!contains(line, filter)) continue;
    entries.push_back(MenuEntry{std::to_string(i) + "  " + line, line, true});
  }
  std::reverse(entries.begin(), entries.end());
  return entries;
}

std::string replace_last_word(std::string_view line, std::string_view word) {
  const std::size_t cut = line.find_last_of(" \t");
  const std::string_view head = cut == std::string_view::npos ? std::string_view{} : line.substr(0, cut + 1);
  return std::string(head) + std::string(word) + " ";
}

std::vector<MenuEntry> completion_entries(std::string_view line, const Completion& completion, std::size_t max) {
  std::vector<MenuEntry> entries;
  for (std::size_t i = 0; i < completion.candidates.size() && i < max; ++i) {
    const std::string& candidate = completion.candidates[i];
    entries.push_back(MenuEntry{candidate, replace_last_word(line, candidate), true});
  }
  if (completion.candidates.size() > max) {
    MenuEntry more{"… " + std::to_string(completion.candidates.size() - max) + " more (type to narrow)", ""};
    more.enabled = false;
    entries.push_back(std::move(more));
  }
  return entries;
}

// --- undo history ------------------------------------------------------------------

UndoHistory undo_history(const undo::UndoLog& log, std::size_t max_each) {
  UndoHistory h;
  h.title = std::to_string(log.step_count()) + (log.step_count() == 1 ? " step" : " steps") + " · limit " +
            std::to_string(log.limit());
  // Each step named after the user command that made it ("paint metal1",
  // UndoLog::step_label; hefesto-app-ux "Undo history"): the redo tail
  // farthest first, then the undoable steps newest first.
  bool first_undo = true;
  for (const undo::HistoryEntry& step : undo::history(log, max_each)) {
    MenuEntry e{(step.redo ? "↷ " : "↶ ") + menu_step_name(step.label), step.command};
    if (!step.redo) {
      e.separator_before = first_undo && !h.entries.empty();
      first_undo = false;
      e.enabled = !log.suspended();  // Magic refuses undo while recording is off
    }
    h.entries.push_back(std::move(e));
  }
  return h;
}

std::optional<std::string> undo_status_message(bool redo, std::string_view step, std::size_t remaining) {
  if (step.empty()) return std::nullopt;
  std::string text = std::string(redo ? "Redid " : "Undid ") + std::string(step);
  if (remaining > 0) {
    text += " · " + std::to_string(remaining) + (remaining == 1 ? " more step" : " more steps");
  }
  return text;
}

std::string next_undo_step(const undo::UndoLog& log, bool redo) {
  // undo: the newest closed step before the position; redo: the nearest undone one.
  const std::size_t position = log.position();
  if (redo ? position >= log.step_count() : position == 0) return {};
  const std::string label = log.step_label(redo ? position : position - 1);
  return label.empty() ? std::string{} : menu_step_name(label);
}

UndoMark undo_mark(const undo::UndoLog& log) noexcept { return UndoMark{log.position(), log.step_count()}; }

std::optional<std::string> undo_move_message(const UndoMark& before, const undo::UndoLog& log) {
  const std::size_t now = log.position();
  if (log.step_count() != before.steps || now == before.position) return std::nullopt;  // a new step: no move
  const bool redo = now > before.position;
  // hefesto-app-ux "Undo two steps from the history": the message names the
  // last step the log passed, i.e. the oldest step an undo undid (the one at
  // the new position) and the newest step a redo redid (the one before it).
  const std::string label = log.step_label(redo ? now - 1 : now);
  const std::string name = label.empty() ? std::string{} : menu_step_name(label);
  return undo_status_message(redo, name, redo ? log.step_count() - now : now);
}

// --- layer palette and inspector menus ------------------------------------------------

std::vector<MenuEntry> layer_menu(const LayerRow& row) {
  const std::string& n = row.name;
  std::vector<MenuEntry> m;
  MenuEntry paint_layer{"Paint layer  —  palette paint " + n, "palette paint " + n};
  paint_layer.checked = row.paint;
  m.push_back(std::move(paint_layer));
  MenuEntry paint{"Paint on box  —  paint " + n, "paint " + n};
  paint.separator_before = true;
  paint.enabled = !row.locked;
  m.push_back(std::move(paint));
  m.push_back(MenuEntry{"Erase on box  —  erase " + n, "erase " + n});
  m.push_back(MenuEntry{"Select on box  —  select area " + n, "select area " + n});
  MenuEntry see{row.visible ? "Hide  —  see no " + n : "Show  —  see " + n, row.toggle_command()};
  see.separator_before = true;
  m.push_back(std::move(see));
  m.push_back(MenuEntry{row.locked ? "Unlock  —  tech unlock " + n : "Lock  —  tech lock " + n, row.lock_command()});
  return m;
}

std::vector<MenuEntry> orient_menu(std::string_view current) {
  static constexpr std::array<std::string_view, 8> kOrientations = {"N", "E", "S", "W", "FN", "FE", "FS", "FW"};
  std::vector<MenuEntry> m;
  for (const std::string_view o : kOrientations) {
    MenuEntry e{"orient " + std::string(o), "orient " + std::string(o)};
    e.checked = o == current;
    m.push_back(std::move(e));
  }
  return m;
}

// --- key-binding sheet -------------------------------------------------------------------

std::vector<KeyBindingTable> key_binding_tables(const ui::MacroTables& macros) {
  std::vector<KeyBindingTable> tables;
  for (const std::string& name : macros.table_names()) tables.push_back({name, table_label(name)});
  return tables;
}

std::vector<KeyBindingRow> key_binding_rows(const ui::MacroTables& macros, std::string_view table,
                                            std::string_view filter) {
  std::vector<KeyBindingRow> rows;
  for (const auto& [key, macro] : macros.entries(table)) {
    if (macro == nullptr) continue;
    KeyBindingRow row;
    row.key = ui::key_display_name(key);
    row.command = macro->interactive ? "imacro: " + macro->text : macro->text;
    row.help = macro->help;
    if (!contains(row.key, filter) && !contains(row.command, filter)) continue;
    row.edit = std::string(macro->interactive ? "imacro " : "macro ") + std::string(table) + " " + row.key + " " +
               quote_macro(macro->text);
    rows.push_back(std::move(row));
  }
  return rows;
}

// --- color window screen ------------------------------------------------------------------

std::optional<ColorWindowView> color_window_view(editor::EditorState& editor) {
  view::ColorWindow* window = editor.color_map.active(std::nullopt);
  if (window == nullptr || !editor.styles) return std::nullopt;
  const auto& entries = editor.styles->colors.entries;
  if (window->color < 0 || static_cast<std::size_t>(window->color) >= entries.size()) return std::nullopt;
  const display::ColorEntry& c = entries[static_cast<std::size_t>(window->color)];
  ColorWindowView v;
  v.window = window->id;
  v.index = window->color;
  v.octal = octal(window->color);
  v.caption = "COLOR = " + v.octal;
  v.decimal = std::to_string(window->color) + " decimal";
  v.modified = editor.color_map.modified;
  v.swatch = Rgba::hex((static_cast<std::uint32_t>(c.r) << 16U) | (static_cast<std::uint32_t>(c.g) << 8U) | static_cast<std::uint32_t>(c.b));
  const view::Rgb rgb{c.r / 255.0, c.g / 255.0, c.b / 255.0};
  const view::Hsv hsv = view::rgb_to_hsv(rgb);
  const std::array<double, 6> values = {rgb.r, rgb.g, rgb.b, hsv.h, hsv.s, hsv.v};
  const auto bars = view::color_bars();
  const auto pumps = view::color_pumps();
  for (std::size_t i = 0; i < v.bars.size() && i < bars.size(); ++i) {
    ColorBarView& b = v.bars[i];
    b.name = std::string(bars[i].name);
    b.value = values[i];
    b.fill = Rgba::hex(kBarColors[i]);
    for (const view::ColorPump& pump : pumps) {
      if (pump.component != bars[i].component) continue;
      (pump.amount < 0.0 ? b.minus : b.plus) = press_line(editor, *window, center(pump.rect));
    }
  }
  char hex[8];
  std::snprintf(hex, sizeof hex, "%02X%02X%02X", static_cast<unsigned>(c.r), static_cast<unsigned>(c.g),
                static_cast<unsigned>(c.b));
  v.values = "R " + std::to_string(c.r) + " G " + std::to_string(c.g) + " B " + std::to_string(c.b) + "   H " +
             fixed3(hsv.h) + " S " + fixed3(hsv.s) + " V " + fixed3(hsv.v) + "   #" + hex;
  if (window->color > 0) v.previous = "color " + octal(window->color - 1);
  if (static_cast<std::size_t>(window->color) + 1 < entries.size()) v.next = "color " + octal(window->color + 1);
  return v;
}

std::string bar_click_line(editor::EditorState& editor, std::size_t bar, double fraction) {
  view::ColorWindow* window = editor.color_map.active(std::nullopt);
  const auto bars = view::color_bars();
  if (window == nullptr || bar >= bars.size()) return {};
  const db::Rect& r = bars[bar].bar;
  const double f = std::clamp(fraction, 0.0, 1.0);
  const auto x = static_cast<db::Coord>(std::lround(static_cast<double>(r.x0) + f * static_cast<double>(r.x1 - r.x0)));
  return press_line(editor, *window, db::Point{x, center(r).y});
}

// --- panels and focus ------------------------------------------------------------------------

int panel_edge_index(DockSide side) noexcept { return side == DockSide::left ? 1 : 0; }

FocusRegion next_focus_region(FocusRegion current, const FocusRegions& shown, bool backward) noexcept {
  constexpr std::array<FocusRegion, 6> kOrder = {FocusRegion::tools,     FocusRegion::layers,  FocusRegion::canvas,
                                                 FocusRegion::inspector, FocusRegion::console, FocusRegion::command};
  const auto visible = [&shown](FocusRegion r) {
    return (r != FocusRegion::layers || shown.layers) && (r != FocusRegion::inspector || shown.inspector) &&
           (r != FocusRegion::console || shown.console);
  };
  std::size_t i = static_cast<std::size_t>(std::find(kOrder.begin(), kOrder.end(), current) - kOrder.begin());
  for (std::size_t step = 0; step < kOrder.size(); ++step) {
    i = backward ? (i + kOrder.size() - 1) % kOrder.size() : (i + 1) % kOrder.size();
    if (visible(kOrder[i])) return kOrder[i];
  }
  return current;
}

}  // namespace hefesto::shell
