// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The layer toolbar and reorder dialog models (see toolbar_views.hpp).
// (Magic ref: tcltk/toolbar.tcl:7-241, tcltk/reorderLayers.tcl:1-316)

#include "hefesto/shell/toolbar_views.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <utility>

#include "hefesto/shell/manager_views.hpp"

namespace hefesto::shell {

namespace {

gui::ToolbarAction toolbar_action(PaletteGesture gesture) {
  switch (gesture) {
    case PaletteGesture::click: return gui::ToolbarAction::button1;
    case PaletteGesture::right: return gui::ToolbarAction::button3;
    case PaletteGesture::middle: return gui::ToolbarAction::button2;
    case PaletteGesture::shift_middle: return gui::ToolbarAction::shift_button2;
    case PaletteGesture::key_p: return gui::ToolbarAction::key_p;
    case PaletteGesture::key_e: return gui::ToolbarAction::key_e;
    case PaletteGesture::key_s: return gui::ToolbarAction::key_s;
    case PaletteGesture::key_shift_s: return gui::ToolbarAction::key_shift_s;
    case PaletteGesture::key_l: return gui::ToolbarAction::key_l;
    case PaletteGesture::key_u: break;
  }
  return gui::ToolbarAction::key_u;
}

std::string lower(std::string_view text) {
  std::string out{text};
  std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

}  // namespace

std::optional<std::string> palette_command(std::string_view window, std::string_view layer, bool locked,
                                           PaletteGesture gesture) {
  auto words = gui::toolbar_row_command(window, layer, locked, toolbar_action(gesture));
  if (!words) return std::nullopt;
  if (window.empty() && !words->empty()) words->erase(words->begin());
  return command_line(*words);
}

std::vector<std::size_t> toolbar_order(const std::vector<std::string>& names, const gui::ToolbarModel& toolbar) {
  std::vector<std::size_t> order;
  std::vector<bool> used(names.size(), false);
  for (const std::string& layer : toolbar.layers) {
    for (std::size_t i = 0; i < names.size(); ++i) {
      if (!used[i] && names[i] == layer) {
        used[i] = true;
        order.push_back(i);
      }
    }
  }
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (!used[i]) order.push_back(i);
  }
  return order;
}

ReorderModel::ReorderModel(std::vector<std::string> layers, std::vector<std::string> presets)
    : layers_(std::move(layers)), original_(layers_), presets_(std::move(presets)) {}

std::vector<ReorderRow> ReorderModel::rows() const {
  std::vector<ReorderRow> out;
  int row = 0;
  for (const std::string& layer : layers_) {
    const auto grouped = std::ranges::find_if(groups_, [&](const auto& g) { return g.first == layer; });
    const bool joins = grouped != groups_.end() && !out.empty() &&
                       std::ranges::any_of(out, [&](const ReorderRow& r) { return r.layer == grouped->second; });
    if (!joins) ++row;
    out.push_back(ReorderRow{layer, joins ? out.back().row : row});
  }
  return out;
}

std::vector<std::size_t> ReorderModel::filtered(std::string_view search) const {
  const std::string needle = lower(search);
  std::vector<std::size_t> out;
  for (std::size_t i = 0; i < layers_.size(); ++i) {
    if (needle.empty() || lower(layers_[i]).find(needle) != std::string::npos) out.push_back(i);
  }
  return out;
}

bool ReorderModel::move(std::size_t from, std::size_t to) {
  if (from >= layers_.size()) return false;
  to = std::min(to, layers_.size() - 1);
  std::string layer = std::move(layers_[from]);
  layers_.erase(layers_.begin() + static_cast<std::ptrdiff_t>(from));
  layers_.insert(layers_.begin() + static_cast<std::ptrdiff_t>(to), std::move(layer));
  selection_ = {to};
  return true;
}

bool ReorderModel::group_selected() {
  if (selection_.size() < 2) return false;
  std::vector<std::string> chosen;
  for (const std::size_t i : selection_) {
    if (i < layers_.size()) chosen.push_back(layers_[i]);
  }
  if (chosen.size() < 2) return false;
  const std::string head = chosen.front();
  std::vector<std::string> rest;
  for (const std::string& layer : layers_) {
    if (std::ranges::find(chosen, layer) == chosen.end() || layer == head) rest.push_back(layer);
  }
  const auto at = std::ranges::find(rest, head);
  const auto head_index = static_cast<std::size_t>(std::distance(rest.begin(), at));
  rest.insert(at + 1, chosen.begin() + 1, chosen.end());
  layers_ = std::move(rest);
  std::erase_if(groups_, [&](const auto& g) { return std::ranges::find(chosen, g.first) != chosen.end(); });
  for (std::size_t i = 1; i < chosen.size(); ++i) groups_.emplace_back(chosen[i], head);
  selection_.clear();
  for (std::size_t i = 0; i < chosen.size(); ++i) selection_.insert(head_index + i);
  return true;
}

bool ReorderModel::delete_selected() {
  if (selection_.empty()) return false;
  std::vector<std::string> kept;
  for (std::size_t i = 0; i < layers_.size(); ++i) {
    if (!selection_.contains(i)) kept.push_back(layers_[i]);
  }
  std::erase_if(groups_, [&](const auto& g) {
    return std::ranges::find(kept, g.first) == kept.end() || std::ranges::find(kept, g.second) == kept.end();
  });
  layers_ = std::move(kept);
  selection_.clear();
  return true;
}

void ReorderModel::replace(std::vector<std::string> layers) {
  layers_ = std::move(layers);
  groups_.clear();
  selection_.clear();
}

std::string ReorderModel::saved_text(std::string_view text) const {
  if (text.empty()) {
    gui::ToolbarModel toolbar;
    toolbar.layers = layers_;
    return gui::write_current_toolbar(toolbar);
  }
  return gui::rewrite_current_toolbar(text, layers_);
}

}  // namespace hefesto::shell
