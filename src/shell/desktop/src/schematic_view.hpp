// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Schematic tab's Raven view (openspec/changes/m8-schematic design D15,
// task 13.2): the active schematic document of the session drawn read-only
// from the canvas view model (schematic_canvas.hpp), fitted to the view,
// with the hierarchy breadcrumb on top. A click selects the instance under
// the pointer with `schematic select instance <name>` through the window's
// command line (logged and replayable). The library browser, properties,
// netlist and messages panels are view models (schematic_library.hpp,
// schematic_panels.hpp) not yet laid out in Raven: see
// docs/design/m8-ui-conformance.md.
//
// Owner: SHELL.

#pragma once

#include <memory>

#include "hefesto/shell/schematic_canvas.hpp"
#include "hefesto/shell/schematic_panels.hpp"
#include "raven_ui.hpp"
#include "screen_context.hpp"

namespace hefesto::shell {

class SchematicView {
 public:
  explicit SchematicView(ScreenContext& ctx);
  SchematicView(const SchematicView&) = delete;
  SchematicView& operator=(const SchematicView&) = delete;
  ~SchematicView();

  [[nodiscard]] ui::Widget& widget() noexcept;
  /// Rebuilds the scene from the session's active document (after a command).
  void refresh();

 private:
  void paint(ui::Painter& p, float w, float h, const ThemeTokens& t);
  void click(float x, float y);

  ScreenContext& ctx_;
  std::unique_ptr<ui::Painted> canvas_;
  SchematicScene scene_;
  SchematicViewport viewport_;
  HierarchyBreadcrumb breadcrumb_;
  bool has_document_ = false;
};

}  // namespace hefesto::shell
