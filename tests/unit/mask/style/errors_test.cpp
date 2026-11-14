// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Mask generation error reporting (tasks.md 3.6): CIFError's feedback text
// and scale, `options no-errors`, and no cell designated for errors.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/mask/style/errors.hpp"
#include "style_fixture.hpp"

namespace mask = hefesto::mask;
namespace db = hefesto::db;
using mask::test::read;

namespace {

struct Session {
  hefesto::editor::EditorState editor;
  db::CellDef* top = nullptr;

  explicit Session(const mask::test::Read& r) {
    editor.technology = r.tech;
    top = editor.cells.create("top", r.tech->plane_count()).value();
    top->set_bboxes(db::Rect{0, 0, 100, 100}, db::Rect{0, 0, 100, 100});
    REQUIRE(editor.windows.open(top, {0, 0, 499, 399}).ok());
  }

  [[nodiscard]] mask::MaskErrorTarget target(const db::CellDef* cell) {
    return mask::MaskErrorTarget{cell, &editor.feedback, &editor.windows, nullptr};
  }
};

}  // namespace

TEST_CASE("a mask generation error becomes a feedback area", "[cap:mask-output-styles][req:Mask generation error reporting]") {
  const auto r = read("style s\nscalefactor 2\nlayer CONT pc\nsquares 2 4 4\nlayer M1 metal1");
  Session s(r);
  mask::MaskErrorReporter errors(r.style(), s.target(s.top));
  errors.set_layer(0);
  CHECK(errors.report(db::Rect{0, 0, 20, 20}, "Contact cut doesn't fit"));
  REQUIRE(s.editor.feedback.size() == 1);
  const auto& area = s.editor.feedback.areas()[0];
  CHECK(area.message() == "CIF error in cell top, layer CONT: Contact cut doesn't fit");
  CHECK(area.scale == r.style().scale.scale_factor);
  CHECK(area.area == db::Rect{0, 0, 10, 10});  // mask units over the scale factor
  CHECK(area.style == hefesto::view::producer_style::kMaskGeneration);
  errors.set_layer(1);
  CHECK(errors.text("x") == "CIF error in cell top, layer M1: x");
  CHECK(errors.reported() == 1);
  CHECK(errors.suppressed() == 0);
}

TEST_CASE("no-errors and a missing error cell suppress the feedback",
          "[cap:mask-output-styles][req:Mask generation error reporting]") {
  // Scenario "Suppressed errors".
  const auto quiet = read("style s\nscalefactor 2\noptions no-errors\nlayer CONT pc\nsquares 2 4 4");
  Session s(quiet);
  mask::MaskErrorReporter errors(quiet.style(), s.target(s.top));
  CHECK_FALSE(errors.report(db::Rect{0, 0, 2, 2}, "Contact cut doesn't fit"));
  CHECK(s.editor.feedback.size() == 0);
  CHECK(errors.suppressed() == 1);
  // No cell designated for errors (CIFErrorDef == NULL).
  const auto loud = read("style s\nscalefactor 2\nlayer CONT pc\nsquares 2 4 4");
  Session s2(loud);
  mask::MaskErrorReporter none(loud.style(), s2.target(nullptr));
  CHECK_FALSE(none.report(db::Rect{0, 0, 2, 2}, "x"));
  CHECK(s2.editor.feedback.size() == 0);
  CHECK(none.suppressed() == 1);
  CHECK(none.reported() == 0);
}
