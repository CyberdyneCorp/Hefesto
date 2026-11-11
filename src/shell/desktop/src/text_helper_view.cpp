// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The text helper's model (see text_helper_view.hpp).
// (Magic ref: tcltk/texthelper.tcl:1-420)

#include "hefesto/shell/text_helper_view.hpp"

#include <algorithm>

#include "hefesto/shell/manager_views.hpp"

namespace hefesto::shell {

const std::array<std::string_view, 11>& justification_choices() {
  static constexpr std::array<std::string_view, 11> kChoices = {"NW", "N", "NE", "W", "center", "E",
                                                                "SW", "S", "SE", "center", "default"};
  return kChoices;
}

std::string TextHelperView::title() const {
  switch (target_) {
    case TextHelperTarget::new_label: return "New label";
    case TextHelperTarget::selected_label: return "Selected label";
    case TextHelperTarget::selected_labels: break;
  }
  return "Selected labels";
}

void TextHelperView::show_new_label(long long port_last) {
  const bool port = model_.port;
  model_ = gui::TextHelperModel{};
  model_.port = port;  // the check box keeps its state (Magic leaves it)
  model_.port_index = std::to_string(port_last + 1);
  target_ = TextHelperTarget::new_label;
}

void TextHelperView::show_selection(const gui::TextHelperModel& fields, std::size_t count) {
  model_ = fields;
  target_ = count > 1 ? TextHelperTarget::selected_labels : TextHelperTarget::selected_label;
}

void TextHelperView::set_justification(std::string_view just) {
  const auto& choices = justification_choices();
  if (std::ranges::find(choices, just) != choices.end()) model_.justification = std::string{just};
}

std::vector<std::string> TextHelperView::apply_lines(bool port_exists, std::string_view previous_units) const {
  if (target_ == TextHelperTarget::new_label) return command_lines(gui::new_label_commands(model_));
  std::vector<std::string> lines = {command_line({"units", "microns"})};
  for (std::string& line : command_lines(gui::change_label_commands(model_, port_exists))) {
    lines.push_back(std::move(line));
  }
  lines.push_back("units " + std::string{previous_units});
  return lines;
}

std::string TextHelperView::preview(bool port_exists, std::string_view previous_units) const {
  std::string text;
  for (const std::string& line : apply_lines(port_exists, previous_units)) {
    if (!text.empty()) text += " · ";
    text += line;
  }
  return text;
}

void TextHelperView::after_apply(std::optional<long long> port_last) {
  if (target_ == TextHelperTarget::new_label && !model_.text.empty() && model_.port && !model_.port_index.empty()) {
    model_.port_index = std::to_string(port_last.value_or(0) + 1);
  }
}

}  // namespace hefesto::shell
