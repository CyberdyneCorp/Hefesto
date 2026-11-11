// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The manager panels' view models (see manager_views.hpp).
// (Magic ref: tcltk/cellmgr.tcl:1-364, libmgr.tcl:1-249, wrapper.tcl:286-531)

#include "hefesto/shell/manager_views.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>

#include "hefesto/script/binding/tags.hpp"

namespace hefesto::shell {

namespace {

std::optional<std::int64_t> positive_integer(std::string_view text) {
  std::int64_t value = 0;
  const auto* end = text.data() + text.size();
  const auto [ptr, ec] = std::from_chars(text.data(), end, value);
  if (ec != std::errc{} || ptr != end || value <= 0) return std::nullopt;
  return value;
}

std::string_view parent_id(std::string_view id) {
  const std::size_t slash = id.rfind('/');
  return slash == std::string_view::npos ? std::string_view{} : id.substr(0, slash);
}

}  // namespace

std::string command_line(const std::vector<std::string>& words) {
  std::string line;
  for (const std::string& word : words) {
    if (!line.empty()) line += ' ';
    line += script::quote_list_element(word);
  }
  return line;
}

std::vector<std::string> command_lines(const std::vector<std::vector<std::string>>& commands) {
  std::vector<std::string> lines;
  lines.reserve(commands.size());
  for (const auto& words : commands) lines.push_back(command_line(words));
  return lines;
}

std::string tree_cell_text(int depth, std::optional<bool> open, std::string_view text) {
  std::string out(static_cast<std::size_t>(std::max(depth, 0)) * 2U, ' ');
  if (!open) {
    out += "  ";
  } else {
    out += *open ? "▾ " : "▸ ";
  }
  out += text;
  return out;
}

// --- cell manager ------------------------------------------------------------------------

void CellsView::reset(const gui::CellManagerModel& model) {
  std::vector<std::string> open_ids;
  for (const Node& node : nodes_) {
    if (node.open) open_ids.push_back(node.row.id);
  }
  const bool first = nodes_.empty();
  target_ = model.window;
  nodes_.clear();
  for (const gui::CellManagerRow& row : model.rows) {
    Node node{row, false, row.depth > 0 ? false : true, true};
    nodes_.push_back(std::move(node));
  }
  // A top cell's children came with the model: it has children when the
  // next row is deeper; it starts open the first time (Magic opens tops).
  for (std::size_t i = 0; i < nodes_.size(); ++i) {
    const bool deeper = i + 1 < nodes_.size() && nodes_[i + 1].row.depth > nodes_[i].row.depth;
    if (nodes_[i].row.depth == 0) {
      nodes_[i].has_children = deeper;
      nodes_[i].open = deeper && (first || std::ranges::find(open_ids, nodes_[i].row.id) != open_ids.end());
    }
  }
  for (const std::string& id : open_ids) {
    if (const auto at = find(id); at && nodes_[*at].row.depth > 0) {
      load_children(*at);
      nodes_[*at].open = nodes_[*at].has_children;
    }
  }
  if (selected_id_ && !find(*selected_id_)) selected_id_.reset();
  rebuild();
}

const gui::CellManagerRow& CellsView::visible_row(std::size_t index) const {
  return nodes_.at(visible_.at(index)).row;
}

std::string CellsView::cell_text(std::size_t index, int column) const {
  if (index >= visible_.size()) return {};
  const Node& node = nodes_[visible_[index]];
  if (column == 0) {
    const std::optional<bool> open = node.has_children ? std::optional<bool>{node.open} : std::nullopt;
    return tree_cell_text(node.row.depth, open, node.row.cell);
  }
  return node.row.depth == 0 ? std::string{"(top cell)"} : node.row.instance;
}

std::string CellsView::accessible_text(std::size_t index) const {
  if (index >= visible_.size()) return {};
  const Node& node = nodes_[visible_[index]];
  std::string text = node.row.cell;
  text += node.row.depth == 0 ? ", top cell" : ", instance " + node.row.instance;
  text += ", level " + std::to_string(node.row.depth + 1);
  if (node.has_children) text += node.open ? ", open" : ", closed";
  return text;
}

bool CellsView::toggle(std::size_t index) {
  if (index >= visible_.size()) return false;
  const std::size_t at = visible_[index];
  if (!nodes_[at].open) load_children(at);
  if (!nodes_[at].has_children) return false;
  nodes_[at].open = !nodes_[at].open;
  rebuild();
  return true;
}

std::optional<std::size_t> CellsView::key_right(std::size_t index) {
  if (index >= visible_.size()) return std::nullopt;
  const std::size_t at = visible_[index];
  if (!nodes_[at].open) {
    toggle(index);
  } else if (index + 1 < visible_.size()) {
    select(index + 1);
  }
  return selected();
}

std::optional<std::size_t> CellsView::key_left(std::size_t index) {
  if (index >= visible_.size()) return std::nullopt;
  const std::size_t at = visible_[index];
  if (nodes_[at].open) {
    toggle(index);
    return selected();
  }
  const std::string_view parent = parent_id(nodes_[at].row.id);
  for (std::size_t i = 0; i < visible_.size() && !parent.empty(); ++i) {
    if (nodes_[visible_[i]].row.id == parent) select(i);
  }
  return selected();
}

void CellsView::select(std::optional<std::size_t> index) {
  if (index && *index < visible_.size()) {
    selected_id_ = nodes_[visible_[*index]].row.id;
  } else {
    selected_id_.reset();
  }
}

std::optional<std::size_t> CellsView::selected() const {
  if (!selected_id_) return std::nullopt;
  for (std::size_t i = 0; i < visible_.size(); ++i) {
    if (nodes_[visible_[i]].row.id == *selected_id_) return i;
  }
  return std::nullopt;
}

std::string CellsView::instance_path(std::size_t index) const {
  if (index >= visible_.size()) return {};
  // Walk up the ids: each ancestor below the top contributes its instance.
  std::vector<std::string> parts;
  std::string_view id = nodes_[visible_[index]].row.id;
  while (!id.empty()) {
    if (const auto at = find(id); at && nodes_[*at].row.depth > 0) parts.push_back(nodes_[*at].row.instance);
    id = parent_id(id);
  }
  std::string path;
  for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
    if (!path.empty()) path += '/';
    path += *it;
  }
  return path;
}

std::vector<std::string> CellsView::commands(gui::CellManagerAction action) const {
  const auto index = selected();
  if (!index) return {};
  return command_lines(gui::cell_manager_commands(action, visible_row(*index), instance_path(*index)));
}

std::optional<std::string> CellsView::select_instance(std::string_view top,
                                                      const std::vector<std::pair<std::string, std::string>>& path,
                                                      std::string_view sstr) {
  std::string id{top};
  if (!find(id)) return std::nullopt;
  for (const auto& [instance, def] : path) {
    if (const auto at = find(id); at && !nodes_[*at].open) {
      load_children(*at);
      nodes_[*at].open = nodes_[*at].has_children;
    }
    id += "/" + def;
    const auto at = find(id);
    if (!at) {
      rebuild();
      return std::nullopt;
    }
    nodes_[*at].row.instance = instance;  // scantree: the column shows the selected instance
  }
  if (const auto at = find(id); at && nodes_[*at].open) nodes_[*at].open = false;  // Magic: -open false
  rebuild();
  selected_id_ = id;
  return "Selected cell is " + id + " (" + std::string{sstr} + ")";
}

void CellsView::rebuild() {
  visible_.clear();
  std::size_t i = 0;
  while (i < nodes_.size()) {
    visible_.push_back(i);
    i = nodes_[i].open ? i + 1 : subtree_end(i);
  }
}

std::optional<std::size_t> CellsView::find(std::string_view id) const {
  for (std::size_t i = 0; i < nodes_.size(); ++i) {
    if (nodes_[i].row.id == id) return i;
  }
  return std::nullopt;
}

std::size_t CellsView::subtree_end(std::size_t node) const {
  std::size_t end = node + 1;
  while (end < nodes_.size() && nodes_[end].row.depth > nodes_[node].row.depth) ++end;
  return end;
}

void CellsView::load_children(std::size_t node) {
  if (nodes_[node].loaded) return;
  nodes_[node].loaded = true;
  std::vector<gui::CellManagerRow> rows;
  if (children_) rows = children_(nodes_[node].row);
  nodes_[node].has_children = !rows.empty();
  std::vector<Node> inserted;
  inserted.reserve(rows.size());
  for (gui::CellManagerRow& row : rows) inserted.push_back(Node{std::move(row), false, false, true});
  nodes_.insert(nodes_.begin() + static_cast<std::ptrdiff_t>(node) + 1, inserted.begin(), inserted.end());
}

// --- library manager ---------------------------------------------------------------------

std::string display_directory(std::string_view directory,
                              const std::vector<std::pair<std::string, std::string>>& variables) {
  const std::pair<std::string, std::string>* best = nullptr;
  for (const auto& variable : variables) {
    std::string_view value = variable.second;
    while (value.size() > 1 && value.back() == '/') value.remove_suffix(1);
    if (value.empty() || !directory.starts_with(value)) continue;
    const bool boundary = directory.size() == value.size() || directory[value.size()] == '/';
    if (boundary && (best == nullptr || value.size() > best->second.size())) best = &variable;
  }
  if (best == nullptr) return std::string{directory};
  std::string_view value = best->second;
  while (value.size() > 1 && value.back() == '/') value.remove_suffix(1);
  return "$" + best->first + std::string{directory.substr(value.size())};
}

void LibrariesView::reset(const gui::LibraryManagerModel& model,
                          const std::vector<std::pair<std::string, std::string>>& variables) {
  std::vector<std::string> open;
  for (const Group& group : groups_) {
    if (group.open) open.push_back(group.directory);
  }
  const bool first = groups_.empty();
  groups_.clear();
  cell_count_ = 0;
  for (const std::string& directory : model.directories) {
    Group group{directory, display_directory(directory, variables), {}, false};
    for (const gui::LibraryManagerRow& row : model.rows) {
      if (row.directory == directory) group.rows.push_back(row);
    }
    cell_count_ += group.rows.size();
    group.open = first ? groups_.empty() : std::ranges::find(open, directory) != open.end();
    groups_.push_back(std::move(group));
  }
  selected_.reset();
  rebuild();
}

std::string LibrariesView::cell_text(std::size_t index, int column) const {
  if (index >= visible_.size()) return {};
  const Visible& v = visible_[index];
  const Group& group = groups_[v.group];
  if (!v.row) return column == 0 ? tree_cell_text(0, group.open, group.label) : std::string{};
  const gui::LibraryManagerRow& row = group.rows[*v.row];
  return column == 0 ? tree_cell_text(1, std::nullopt, row.cell) : row.technology;
}

std::string LibrariesView::accessible_text(std::size_t index) const {
  if (index >= visible_.size()) return {};
  const Visible& v = visible_[index];
  const Group& group = groups_[v.group];
  if (!v.row) {
    return "Directory " + group.label + ", " + std::to_string(group.rows.size()) + " cells, " +
           (group.open ? "open" : "closed");
  }
  const gui::LibraryManagerRow& row = group.rows[*v.row];
  return row.cell + ", technology " + row.technology;
}

bool LibrariesView::is_directory(std::size_t index) const {
  return index < visible_.size() && !visible_[index].row;
}

std::string LibrariesView::cell(std::size_t index) const {
  if (index >= visible_.size() || !visible_[index].row) return {};
  return groups_[visible_[index].group].rows[*visible_[index].row].cell;
}

bool LibrariesView::toggle(std::size_t index) {
  if (!is_directory(index)) return false;
  Group& group = groups_[visible_[index].group];
  group.open = !group.open;
  rebuild();
  return true;
}

std::vector<std::string> LibrariesView::commands(gui::LibraryManagerAction action) const {
  if (!selected_) return {};
  const std::string name = cell(*selected_);
  if (name.empty()) return {};
  return command_lines(gui::library_manager_commands(action, name));
}

void LibrariesView::rebuild() {
  visible_.clear();
  for (std::size_t g = 0; g < groups_.size(); ++g) {
    visible_.push_back(Visible{g, std::nullopt});
    if (!groups_[g].open) continue;
    for (std::size_t r = 0; r < groups_[g].rows.size(); ++r) visible_.push_back(Visible{g, r});
  }
  if (selected_ && *selected_ >= visible_.size()) selected_.reset();
}

// --- technology manager ------------------------------------------------------------------

std::string TechView::title() const {
  std::string text = model_.technology;
  if (!model_.version.empty()) text += " " + model_.version;
  return text;
}

std::vector<TechField> TechView::fields() const {
  const auto style = [](const std::string& name) { return name.empty() ? std::string{"(none)"} : name; };
  return {
      {"Microns per lambda (CIF)", model_.microns_per_lambda},
      {"Internal units per lambda", model_.lambda_numerator + " / " + model_.lambda_denominator},
      {"CIF input", style(model_.cif_input_style)},
      {"CIF output", style(model_.cif_output_style)},
      {"Extract", style(model_.extract_style)},
      {"DRC", style(model_.drc_style)},
  };
}

const std::vector<std::string>& TechView::styles(TechStyle style) const {
  switch (style) {
    case TechStyle::cif_input: return model_.cif_input_styles;
    case TechStyle::cif_output: return model_.cif_output_styles;
    case TechStyle::extract: return model_.extract_styles;
    case TechStyle::drc: break;
  }
  return model_.drc_styles;
}

const std::string& TechView::current(TechStyle style) const {
  switch (style) {
    case TechStyle::cif_input: return model_.cif_input_style;
    case TechStyle::cif_output: return model_.cif_output_style;
    case TechStyle::extract: return model_.extract_style;
    case TechStyle::drc: break;
  }
  return model_.drc_style;
}

std::string TechView::style_command(TechStyle style, std::string_view name) {
  switch (style) {
    case TechStyle::cif_input: return command_line({"cif", "istyle", std::string{name}});
    case TechStyle::cif_output: return command_line({"cif", "ostyle", std::string{name}});
    case TechStyle::extract: return command_line({"extract", "style", std::string{name}});
    case TechStyle::drc: break;
  }
  return command_line({"drc", "style", std::string{name}});
}

std::optional<std::string> TechView::tech_menu_command(std::size_t index) const {
  if (index >= model_.tech_menu.size()) return std::nullopt;
  return model_.tech_menu[index].command;
}

std::optional<std::string> TechView::scalegrid_command(std::string_view numerator,
                                                       std::string_view denominator) const {
  const auto a = positive_integer(numerator);
  const auto b = positive_integer(denominator);
  const auto num = positive_integer(model_.lambda_numerator);
  const auto den = positive_integer(model_.lambda_denominator);
  if (!a || !b || !num || !den) return std::nullopt;
  return command_line(gui::tech_manager_scalegrid(*a, *b, *num, *den));
}

}  // namespace hefesto::shell
