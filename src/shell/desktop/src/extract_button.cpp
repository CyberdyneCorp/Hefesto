// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The top bar's Extract button view model (extract_button.hpp). Owner: SHELL.

#include "hefesto/shell/extract_button.hpp"

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/view/window.hpp"

namespace hefesto::shell {

namespace {

ExtractButtonView disabled(std::string tooltip) {
  return ExtractButtonView{false, std::move(tooltip)};
}

}  // namespace

ExtractButtonView extract_button_view(const ExtractButtonInput& input) {
  if (!input.style) {
    return disabled("Extract: technology " + input.technology +
                    " has no extract style (no extract section); load a technology with one");
  }
  if (!input.root) {
    return disabled("Extract: no cell in the active window");
  }
  if (input.job_running) {
    return disabled("Extract: a job is running; Interrupt it from its card or with Ctrl-C");
  }
  return ExtractButtonView{true, "Extract " + *input.root + ": " + std::string{kExtractCommand} + " (style " +
                                     *input.style + ")"};
}

ExtractButtonInput extract_button_input(const editor::EditorState& editor, bool job_running) {
  ExtractButtonInput in;
  in.technology = editor.technology ? editor.technology->name() : std::string{};
  if (editor.extract.current) {
    in.style = editor.extract.current->name;
  }
  const view::LayoutWindow* win = editor.windows.current();
  if (win != nullptr && win->root() != nullptr) {
    in.root = win->root()->name();
  }
  in.job_running = job_running;
  return in;
}

std::optional<std::string> extract_click_line(const ExtractButtonView& view) {
  if (!view.enabled) {
    return std::nullopt;
  }
  return std::string{kExtractCommand};
}

std::string extract_button_status(const ExtractButtonView& view) {
  return std::string{"hefesto: extract button "} + (view.enabled ? "enabled" : "disabled") + ": " + view.tooltip;
}

}  // namespace hefesto::shell
