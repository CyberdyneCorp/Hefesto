// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The tool registry (ui-tools-and-input "Tool registry") and the ui session
// state accessors. Owner: DISPATCH.
// (Magic ref: dbwind/DBWbuttons.c:52-170, tcltk/tools.tcl:700-760)

#include "hefesto/ui/tools.hpp"

#include <array>
#include <utility>

#include "hefesto/editor/editor_state.hpp"

namespace hefesto::ui {

bool ToolRegistry::add(Tool tool) {
  if (tools_.size() >= kMaxTools || find(tool.name) != nullptr) {
    return false;
  }
  tools_.push_back(std::move(tool));
  return true;
}

const Tool* ToolRegistry::find(std::string_view name) const {
  for (const Tool& t : tools_) {
    if (t.name == name) {
      return &t;
    }
  }
  return nullptr;
}

bool ToolRegistry::set_current(std::string_view name) {
  for (std::size_t i = 0; i < tools_.size(); ++i) {
    if (tools_[i].name == name) {
      current_ = i;
      return true;
    }
  }
  return false;
}

const Tool* ToolRegistry::current() const {
  return current_ < tools_.size() ? &tools_[current_] : nullptr;
}

std::vector<Tool> builtin_tools() {
  return {
      Tool{"box", "Box tool: left button moves the box, right button resizes it, middle button paints or erases.", 0,
           true},
      Tool{"wiring", "Wiring tool: left button picks a wire type, middle button finishes, right button cancels.", 19,
           true},
      Tool{"nettool", "Netlist tool: select, join and toggle terminals (netlist module).", 18, true},
      Tool{"pick", "Pick tool: left button keeps the selection, middle button picks and drags, right button cancels.",
           22, true},
  };
}

std::string_view next_tool(std::string_view current) noexcept {
  static constexpr std::array<std::pair<std::string_view, std::string_view>, 4> kCycle = {
      {{"box", "wiring"}, {"wiring", "nettool"}, {"nettool", "pick"}, {"pick", "box"}}};
  for (const auto& [from, to] : kCycle) {
    if (from == current) {
      return to;
    }
  }
  return {};
}

UiState& state(editor::EditorState& editor) {
  return editor.ui;
}

const UiState& state(const editor::EditorState& editor) {
  return editor.ui;
}

bool request_interrupt(editor::EditorState& editor) {
  return state(editor).interrupt_pending.exchange(true);
}

void run_client_updates(editor::EditorState& editor) {
  for (const auto& handler : state(editor).update_handlers) {
    if (handler) {
      handler(editor);
    }
  }
}

}  // namespace hefesto::ui
