// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Label tool's canvas popover, Escape after a measure and the Paint
// button without a paint layer (label_tool.hpp; M4 CARRY task 11.4), with
// the commands run through a real core session.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "hefesto/db/cell.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/label_tool.hpp"
#include "hefesto/tech/loader.hpp"

using namespace hefesto::shell;
namespace db = hefesto::db;
namespace tech = hefesto::tech;

namespace {

hefesto::platform::PortableOptions test_options() {
  return hefesto::platform::PortableOptions{.storage_root = "/app", .pdk_roots = {}, .in_memory = true,
                                           .inline_tasks = true};
}

std::shared_ptr<const tech::Technology> tiny() {
  hefesto::platform::LocalFileSystem local;
  hefesto::diag::CollectingSink sink;
  auto loaded = tech::load_technology(local, std::string{HEFESTO_SOURCE_DIR} + "/tests/data/tech/tiny.tech",
                                      tech::LoadOptions{}, sink);
  REQUIRE(loaded.ok());
  return loaded.value();
}

struct Session {
  CoreSession core{test_options()};
  db::CellDef* top = nullptr;

  Session() {
    auto& ed = core.editor();
    ed.technology = tiny();
    top = ed.cells.create("top", ed.technology->plane_count()).value();
    ed.edit_cell = top;
    auto w = ed.windows.open(top, hefesto::view::ScreenRect{0, 0, 499, 499});
    REQUIRE(w.ok());
    ed.windows.set_current(w.value()->id());
  }
  bool run(const std::string& line) { return CoreCommandSink(core.dispatcher()).submit(line).ok(); }
  std::uint64_t count() { return core.editor().command_count; }
};

}  // namespace

TEST_CASE("the label popover issues label with the text, position and layer",
          "[cap:hefesto-input-and-touch][req:Desktop Paint, Label and Measure tools]") {
  Session s;
  REQUIRE(s.run("box values 1i 1i 3i 3i"));  // the Label sweep's box
  LabelPopover popover;
  popover.open(db::Rect{1, 1, 3, 3}, "metal1");
  CHECK(popover.is_open());
  CHECK_FALSE(popover.commit().has_value());  // no text yet: stays open
  CHECK(popover.is_open());
  popover.set_text("A");
  CHECK_FALSE(popover.layer_enabled());
  const auto line = popover.commit();
  CHECK(line == "label A");  // the spec's scenario: `label A` on the swept box
  CHECK_FALSE(popover.is_open());
  REQUIRE(s.run(*line));
  REQUIRE(s.top->labels().size() == 1U);
  CHECK(s.top->labels().front().text == "A");

  // Position and layer; a text with Tcl characters is quoted.
  popover.open(db::Rect{1, 1, 3, 3}, "metal1");
  popover.set_text("VDD[1]");
  popover.set_position("ne");
  popover.set_position("upwards");  // not a position: ignored
  CHECK(popover.position() == "ne");
  CHECK(popover.layer_enabled());
  const auto full = popover.commit();
  CHECK(full == "label {VDD[1]} ne metal1");
  REQUIRE(s.run(*full));
  CHECK(s.top->labels().back().text == "VDD[1]");

  // Escape closes without a command.
  popover.open(db::Rect{1, 1, 3, 3});
  popover.set_text("B");
  popover.cancel();
  CHECK_FALSE(popover.is_open());
  CHECK_FALSE(popover.commit().has_value());
}

TEST_CASE("the label popover sits beside the box and stays on the canvas",
          "[cap:hefesto-input-and-touch][req:Desktop Paint, Label and Measure tools]") {
  CHECK(label_popover_point(100.0, 50.0, 40.0, 1000.0, 700.0) == PopoverPoint{148.0, 50.0});
  // Near the right edge it flips to the box's left.
  CHECK(label_popover_point(800.0, 50.0, 40.0, 1000.0, 700.0) == PopoverPoint{800.0 - 8.0 - kLabelPopoverWidth, 50.0});
  // Near the bottom it moves up; a canvas narrower than the card pins it left.
  CHECK(label_popover_point(100.0, 690.0, 40.0, 1000.0, 700.0).y == 700.0 - kLabelPopoverHeight);
  CHECK(label_popover_point(10.0, 10.0, 40.0, 200.0, 700.0).x == 0.0);
}

TEST_CASE("Escape right after a measure issues unmeasure",
          "[cap:hefesto-input-and-touch][req:Desktop Paint, Label and Measure tools]") {
  Session s;
  REQUIRE(s.run("box values 0i 0i 4i 0i"));
  REQUIRE(s.run("measure"));
  MeasureEscape escape;
  escape.measured(s.count());
  CHECK(escape.pending(s.count()));
  const auto line = escape.escape(s.count());
  REQUIRE(line == "unmeasure");
  REQUIRE(s.run(*line));
  CHECK(s.top->elements().empty());  // the ruler is gone
  // A second Escape does nothing.
  CHECK_FALSE(escape.escape(s.count()).has_value());

  // Anything run after the measure closes the window.
  REQUIRE(s.run("measure"));
  escape.measured(s.count());
  REQUIRE(s.run("box values 0i 0i 6i 0i"));
  CHECK_FALSE(escape.escape(s.count()).has_value());
}

TEST_CASE("the Paint button is disabled until a paint layer is chosen",
          "[cap:hefesto-input-and-touch][req:Desktop Paint, Label and Measure tools]") {
  const PaintToolState none = paint_tool_state("", "Paint", "Paint (box tool mode)");
  CHECK_FALSE(none.enabled);
  CHECK(none.tooltip == "Choose a layer to paint");
  CHECK(none.accessible_name == "Paint, unavailable");
  const PaintToolState m1 = paint_tool_state("metal1", "Paint", "Paint (box tool mode)");
  CHECK(m1.enabled);
  CHECK(m1.tooltip == "Paint (box tool mode)");
  CHECK(m1.accessible_name == "Paint");
  // Clearing the paint layer leaves the Paint mode.
  CHECK(mode_for_paint_layer(EditMode::paint, "") == EditMode::none);
  CHECK(mode_for_paint_layer(EditMode::paint, "metal1") == EditMode::paint);
  CHECK(mode_for_paint_layer(EditMode::measure, "") == EditMode::measure);
}
