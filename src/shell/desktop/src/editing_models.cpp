// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/editing_models.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <map>

#include "hefesto/db/transform.hpp"
#include "hefesto/ui/complete.hpp"

namespace hefesto::shell {

namespace {

constexpr std::array<std::string_view, 12> kCellCommands = {"load",  "getcell", "get",     "xload",   "edit",     "dump",
                                                            "flush", "save",    "cellname", "instance", "writeall", "down"};
constexpr std::array<std::string_view, 9> kLayerCommands = {"paint", "erase", "see",     "fill",    "select",
                                                            "contact", "genkey", "spliterase", "splitpaint"};

template <std::size_t N>
bool one_of(std::string_view word, const std::array<std::string_view, N>& words) {
  return std::find(words.begin(), words.end(), word) != words.end();
}

std::vector<std::string> split_words(std::string_view line) {
  std::vector<std::string> words;
  std::string word;
  for (const char c : line) {
    if (c == ' ' || c == '\t') {
      if (!word.empty()) words.push_back(std::move(word));
      word.clear();
    } else {
      word += c;
    }
  }
  if (!word.empty()) words.push_back(std::move(word));
  return words;
}

std::optional<ui::CompletionKind> completion_kind(const std::vector<std::string>& before) {
  if (before.empty()) return ui::CompletionKind::command;
  const std::string& first = before.front();
  if (one_of(first, kCellCommands)) return ui::CompletionKind::cell;
  if (one_of(first, kLayerCommands)) return ui::CompletionKind::layer;
  if (first == "tech" && before.size() >= 2 && (before[1] == "lock" || before[1] == "unlock")) {
    return ui::CompletionKind::layer;
  }
  if (first == "wire" && before.size() >= 2 && before[1] == "type") return ui::CompletionKind::layer;
  return std::nullopt;
}

std::string common_prefix(const std::vector<std::string>& names) {
  std::string prefix = names.front();
  for (const std::string& n : names) {
    std::size_t i = 0;
    while (i < prefix.size() && i < n.size() && prefix[i] == n[i]) ++i;
    prefix.resize(i);
  }
  return prefix;
}

std::string capitalized(std::string_view word) {
  std::string out(word);
  if (!out.empty()) out.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(out.front())));
  return out;
}

std::string trimmed(std::string_view text) {
  const auto first = text.find_first_not_of(" \t\n");
  if (first == std::string_view::npos) return {};
  const auto last = text.find_last_not_of(" \t\n");
  return std::string(text.substr(first, last - first + 1));
}

/// The verb of a `yes` answer for questions whose effect is known.
std::optional<std::string> yes_verb(std::string_view question) {
  if (question.find("throw away") != std::string_view::npos || question.find("lose the changes") != std::string_view::npos) {
    return std::string("Discard changes");
  }
  if (question.find("Overwrite") != std::string_view::npos) return std::string("Overwrite");
  if (question.find("Recover") != std::string_view::npos) return std::string("Recover");
  if (question.find("quit") != std::string_view::npos || question.find("Quit") != std::string_view::npos) {
    return std::string("Quit without saving");
  }
  return std::nullopt;
}

std::string point_text(db::Coord x, db::Coord y, const UnitScale& scale) {
  return format_length(static_cast<double>(x), scale) + ", " + format_length(static_cast<double>(y), scale);
}

void add_bbox_rows(InspectorView& view, const db::Rect& r, const UnitScale& scale) {
  const std::string unit(scale.unit_name());
  view.rows.push_back({"Width", format_length(static_cast<double>(r.width()), scale) + " " + unit});
  view.rows.push_back({"Height", format_length(static_cast<double>(r.height()), scale) + " " + unit});
  view.rows.push_back({"Lower left", point_text(r.x0, r.y0, scale)});
  view.rows.push_back({"Upper right", point_text(r.x1, r.y1, scale)});
}

struct SelectionCounts {
  std::map<std::string, std::size_t> layers;  // long type name -> tiles
  std::size_t tiles = 0;
};

SelectionCounts paint_counts(const db::CellDef& def, const tech::Technology& tech) {
  SelectionCounts counts;
  tech::TypeMask all = tech::TypeMask::all();
  all.clear_type(tech::kSpace);
  for (std::size_t p = 0; p < def.plane_count(); ++p) {
    const db::Plane& plane = def.plane(static_cast<tech::PlaneId>(p));
    const db::Rect used = plane.bounding_box();
    if (used.is_null()) continue;
    (void)plane.enumerate(used, all, [&](const db::TileView& tile) {
      ++counts.layers[std::string(tech.type_name(tile.type))];
      ++counts.tiles;
      return true;
    });
  }
  return counts;
}

void paint_view(InspectorView& view, const SelectionCounts& counts) {
  view.title = counts.layers.size() == 1 ? counts.layers.begin()->first : "Paint";
  view.subtitle = std::to_string(counts.tiles) + (counts.tiles == 1 ? " tile on " : " tiles on ") +
                  std::to_string(counts.layers.size()) + (counts.layers.size() == 1 ? " layer" : " layers");
  for (const auto& [name, n] : counts.layers) view.rows.push_back({name, std::to_string(n) + (n == 1 ? " tile" : " tiles")});
}

void label_view(InspectorView& view, const db::Label& label, const tech::Technology& tech,
                std::vector<InspectorAction>* actions) {
  view.title = label.text;
  view.subtitle = label.is_port() ? "Port " + std::to_string(label.port_index) : std::string("Label");
  view.rows.push_back({"Text", label.text});
  view.rows.push_back({"Layer", std::string(tech.type_name(label.type))});
  view.rows.push_back({"Position", std::string(db::position_long_name(label.justification))});
  if (label.is_port()) view.rows.push_back({"Port index", std::to_string(label.port_index)});
  if (actions == nullptr) return;
  actions->push_back({"Rename…", "setlabel text "});
  if (label.is_port()) {
    actions->push_back({"Port index…", "port index "});
  } else {
    actions->push_back({"Make port", "port make"});
  }
  actions->push_back({"Select net", "select net"});
}

void use_view(InspectorView& view, const db::CellUse& use, std::vector<InspectorAction>* actions) {
  view.title = use.def != nullptr ? use.def->name() : std::string("instance");
  view.subtitle = "Instance " + use.id;
  view.rows.push_back({"Cell", view.title});
  view.rows.push_back({"Id", use.id});
  view.rows.push_back({"Orientation", std::string(db::orientation_name(db::orientation(use.transform)))});
  if (use.array.is_array()) {
    const auto cols = std::abs(use.array.xhi - use.array.xlo) + 1;
    const auto rows = std::abs(use.array.yhi - use.array.ylo) + 1;
    view.rows.push_back({"Array", std::to_string(cols) + " × " + std::to_string(rows)});
  }
  if (actions == nullptr) return;
  actions->push_back({"Identify…", "identify "});
  actions->push_back({"Rotate 90°", "clockwise 90"});
  actions->push_back({"Orient…", "orient", true});
  actions->push_back({"Array…", "array "});  // `array xsize ysize` or the bounds, in the command bar
  actions->push_back({"Expand", "expand selection"});
  actions->push_back({"Edit", "edit"});
}

}  // namespace

// --- tool rail --------------------------------------------------------------------

std::optional<std::string_view> active_rail_icon(std::string_view tool, EditMode mode) noexcept {
  if (tool == "box") {
    switch (mode) {
      case EditMode::paint: return "paint";
      case EditMode::label: return "label";
      case EditMode::measure: return "measure";
      case EditMode::none: return "box";
    }
  }
  if (tool == "pick") return "select";
  if (tool == "wiring") return "wire";
  return std::nullopt;
}

std::optional<RailAction> rail_action(std::string_view icon, std::string_view current_tool) {
  const std::string to_box = current_tool == "box" ? std::string() : std::string("tool box");
  if (icon == "box") return RailAction{"tool box", EditMode::none};
  if (icon == "select") return RailAction{"tool pick", EditMode::none};
  if (icon == "wire") return RailAction{"tool wiring", EditMode::none};
  if (icon == "paint") return RailAction{to_box, EditMode::paint};
  if (icon == "label") return RailAction{to_box, EditMode::label};
  if (icon == "measure") return RailAction{to_box, EditMode::measure};
  if (icon == "fit") return RailAction{"view", EditMode::none};
  return std::nullopt;
}

std::string rail_tooltip(const ToolButton& button, bool enabled) {
  if (button.icon == "paint" && !enabled) return "Paint: choose a layer to paint in the Layers panel";
  return std::string(button.tooltip);
}

// --- command bar --------------------------------------------------------------------

Completion complete_command_line(std::string_view line, const cmd::CommandRegistry& registry,
                                 const editor::EditorState& editor) {
  Completion out{std::string(line), {}};
  const bool new_word = line.empty() || line.back() == ' ' || line.back() == '\t';
  std::vector<std::string> words = split_words(line);
  const std::string prefix = new_word || words.empty() ? std::string() : words.back();
  if (!new_word && !words.empty()) words.pop_back();
  const auto kind = completion_kind(words);
  if (!kind) return out;
  const auto found = ui::complete(prefix, ui::CompletionContext{*kind, &registry, &editor});
  if (!found.ok() || found.value().empty()) return out;
  const std::vector<std::string>& names = found.value();
  const std::string stem(line.substr(0, line.size() - prefix.size()));
  if (names.size() == 1) {
    out.line = stem + names.front() + " ";
    return out;
  }
  out.line = stem + common_prefix(names);
  out.candidates = names;
  return out;
}

std::string ResultLine::text() const {
  if (more_lines == 0) return first;
  return first + " (+" + std::to_string(more_lines) + (more_lines == 1 ? " line" : " lines") + ", see Console)";
}

ResultLine result_line(std::string_view output) {
  ResultLine out;
  const auto end = output.find('\n');
  out.first = std::string(output.substr(0, end));
  if (end == std::string_view::npos) return out;
  std::string_view rest = output.substr(end + 1);
  while (!rest.empty() && rest.back() == '\n') rest.remove_suffix(1);
  if (rest.empty()) return out;
  out.more_lines = static_cast<std::size_t>(std::count(rest.begin(), rest.end(), '\n')) + 1;
  return out;
}

// --- prompt dialogs ------------------------------------------------------------------

PromptDialog prompt_dialog(std::string_view question, const std::vector<std::string>& answers,
                           std::size_t default_answer) {
  PromptDialog dialog;
  dialog.text = trimmed(question);
  // The first sentence: up to "?", a newline or a ". " (not the dot of a file name).
  std::size_t stop = dialog.text.find_first_of("?\n");
  const std::size_t period = dialog.text.find(". ");
  if (period != std::string::npos && (stop == std::string::npos || period < stop)) stop = period;
  if (stop != std::string::npos && dialog.text[stop] == '\n' && stop > 0) --stop;
  dialog.title = stop == std::string::npos ? dialog.text : trimmed(dialog.text.substr(0, stop + 1));
  const std::optional<std::string> verb = yes_verb(question);
  const bool yes_no = answers.size() == 2 &&
                      std::find(answers.begin(), answers.end(), "yes") != answers.end() &&
                      std::find(answers.begin(), answers.end(), "no") != answers.end();
  for (std::size_t i = 0; i < answers.size(); ++i) {
    DialogButton b{capitalized(answers[i]), i, i == default_answer};
    if (yes_no && verb) b.label = answers[i] == "yes" ? *verb : std::string("Cancel");
    if (i != default_answer) dialog.buttons.push_back(std::move(b));
  }
  if (default_answer < answers.size()) {
    DialogButton b{capitalized(answers[default_answer]), default_answer, true};
    if (yes_no && verb) b.label = answers[default_answer] == "yes" ? *verb : std::string("Cancel");
    dialog.buttons.push_back(std::move(b));
  }
  return dialog;
}

std::size_t dialog_answer(int result, std::size_t answer_count, std::size_t default_answer) noexcept {
  if (result <= 0 || static_cast<std::size_t>(result) > answer_count) return default_answer;
  return static_cast<std::size_t>(result) - 1;
}

// --- selection inspector ---------------------------------------------------------------

namespace {

void mixed_view(InspectorView& view, const SelectionCounts& counts, std::size_t labels, std::size_t uses) {
  view.title = "Mixed selection";
  view.subtitle = std::to_string(counts.tiles) + " tiles · " + std::to_string(labels) + " labels · " +
                  std::to_string(uses) + " instances";
  for (const auto& [name, n] : counts.layers) {
    view.rows.push_back({name, std::to_string(n) + (n == 1 ? " tile" : " tiles")});
  }
  if (labels > 0) view.rows.push_back({"Labels", std::to_string(labels)});
  if (uses > 0) view.rows.push_back({"Instances", std::to_string(uses)});
}

/// The kind-specific part of the selection view.
void kind_view(InspectorView& view, const db::CellDef& def, const SelectionCounts& counts,
               const tech::Technology& tech, std::vector<InspectorAction>* actions) {
  const std::size_t labels = def.labels().size();
  const std::size_t uses = def.uses().size();
  const int kinds = (counts.tiles > 0 ? 1 : 0) + (labels > 0 ? 1 : 0) + (uses > 0 ? 1 : 0);
  if (kinds > 1) {
    mixed_view(view, counts, labels, uses);
  } else if (uses == 1) {
    use_view(view, *def.uses().front(), actions);
  } else if (uses > 1) {
    view.title = std::to_string(uses) + " instances";
    view.subtitle = "Instances";
  } else if (labels == 1) {
    label_view(view, def.labels().front(), tech, actions);
  } else if (labels > 1) {
    view.title = std::to_string(labels) + " labels";
    view.subtitle = "Labels";
  } else {
    paint_view(view, counts);
  }
}

}  // namespace

std::optional<InspectorView> selection_inspector_view(const editor::EditorState& editor,
                                                      const db::CellDef* window_root, const UnitScale& scale,
                                                      std::vector<InspectorAction>* actions) {
  const select::Selection& sel = editor.selection;
  if (sel.empty() || sel.def() == nullptr || (window_root != nullptr && sel.root() != window_root)) {
    return std::nullopt;
  }
  const db::CellDef& def = *sel.def();
  const SelectionCounts counts = paint_counts(def, *editor.technology);
  InspectorView view;
  view.section = "SELECTION";
  kind_view(view, def, counts, *editor.technology, actions);
  const db::Rect bbox = sel.bbox();
  if (!bbox.is_null()) add_bbox_rows(view, bbox, scale);
  if (actions != nullptr) {
    if (counts.tiles > 0 && def.uses().empty()) actions->push_back({"Select net", "select net"});
    actions->push_back({"What", "what"});
    actions->push_back({"Delete", "delete"});
  }
  return view;
}

// --- undo and redo -------------------------------------------------------------------------

UndoAffordance undo_affordance(const editor::EditorState& editor) {
  UndoAffordance a;
  const std::size_t undoable = editor.undo.position();
  const std::size_t redoable = editor.undo.step_count() - std::min(editor.undo.step_count(), undoable);
  a.can_undo = undoable > 0;
  a.can_redo = redoable > 0;
  a.recording = !editor.undo.suspended();
  const auto steps = [](std::size_t n) { return std::to_string(n) + (n == 1 ? " step" : " steps"); };
  a.undo_tooltip = a.can_undo ? "Undo (u): " + steps(undoable) : std::string("Undo (u): nothing to undo");
  a.redo_tooltip = a.can_redo ? "Redo (U): " + steps(redoable) : std::string("Redo (U): nothing to redo");
  if (!a.recording) a.undo_tooltip += " · undo is off (undo enable)";
  return a;
}

// --- feedback list ----------------------------------------------------------------------------

std::vector<FeedbackRow> feedback_rows(const editor::EditorState& editor, const UnitScale& scale) {
  std::vector<FeedbackRow> rows;
  std::size_t n = 0;
  for (const view::FeedbackArea& area : editor.feedback.areas()) {
    ++n;
    const db::Rect& r = area.area;
    rows.push_back({std::to_string(n) + "  (" + point_text(r.x0, r.y0, scale) + ") – (" + point_text(r.x1, r.y1, scale) +
                        ")  " + area.message(),
                    "feedback find " + std::to_string(n)});
  }
  return rows;
}

}  // namespace hefesto::shell
