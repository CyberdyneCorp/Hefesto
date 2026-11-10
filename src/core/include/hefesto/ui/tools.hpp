// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The tool registry (capability ui-tools-and-input, requirements "Tool
// registry", "tool command"; the `tool` command of ui-gui-managers "Tool
// modes", pulled forward into M2; design.md D7). Each tool has a name, a
// usage text, a cursor glyph and its own macro table (the table name is the
// tool name). The built-in tools are `box` (glyph 0), `wiring` (19),
// `nettool` (18) and `pick` (22), as tools.tcl sets them up in Magic's Tcl
// build; `box` is current at startup and `tool` cycles box -> wiring ->
// nettool -> pick. `macro copy NAME` registers NAME as a tool without a
// button handler.
// (Magic ref: dbwind/DBWbuttons.c:52-170, tcltk/tools.tcl:512-788)
//
// Owner: DISPATCH (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>


namespace hefesto::ui {

struct Tool {
  std::string name;
  std::string usage;
  int cursor = 0;  // cursor glyph index (box 0, wiring 19, nettool 18, pick 22)
  /// False for tools created by `macro copy` (no button handler).
  bool handler = true;
};

/// "The system SHALL keep at most 10 tools."
inline constexpr std::size_t kMaxTools = 10;

class ToolRegistry {
 public:
  /// Registers a tool; false when kMaxTools are registered or the name is
  /// taken. The first tool becomes current.
  bool add(Tool tool);
  [[nodiscard]] const Tool* find(std::string_view name) const;
  /// Makes `name` current; false when no such tool exists.
  bool set_current(std::string_view name);
  /// The current tool; nullptr before any tool is registered.
  [[nodiscard]] const Tool* current() const;
  [[nodiscard]] const std::vector<Tool>& tools() const noexcept { return tools_; }

 private:
  std::vector<Tool> tools_;
  std::size_t current_ = 0;
};

/// The built-in tools in registration order.
[[nodiscard]] std::vector<Tool> builtin_tools();

/// The `tool next` successor of a tool (box -> wiring -> nettool -> pick ->
/// box); other names have none (empty).
[[nodiscard]] std::string_view next_tool(std::string_view current) noexcept;

}  // namespace hefesto::ui
