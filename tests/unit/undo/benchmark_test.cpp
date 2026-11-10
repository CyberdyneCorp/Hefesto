// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Undo recording cost (m2-interactive-editing task 4.6, design.md R2):
// paint, erase, undo and redo of 10,000 rectangles in a cell holding a
// 100 x 100 array of sky130_fd_sc_hd__dfxtp_1, and one paint merging them
// all (with its undo), against hefesto-performance
// "Edit latency" (an edit of up to 10,000 rectangles updates the view
// within 50 ms). The database work (edit + recording + replay + bounding
// boxes) is timed; the view update itself is VIEW/SHELL's. Timings are
// reported with WARN; the 50 ms check runs only in optimized builds
// (NDEBUG). Skips without the pinned sky130A cache.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>

#include "../cell/cell_fixture.hpp"
#include "../db/db_fixture.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/undo/db_recorder.hpp"
#include "hefesto/undo/undo_log.hpp"
#include "reference_data.hpp"

namespace db = hefesto::db;
namespace fs = std::filesystem;

namespace {

double millis(const std::function<void()>& work) {
  const auto start = std::chrono::steady_clock::now();
  work();
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

std::string slurp(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::ostringstream s;
  s << in.rdbuf();
  return s.str();
}

}  // namespace

TEST_CASE("paint, erase, undo and redo of 10,000 rectangles over a 100x100 dfxtp_1 array",
          "[cap:layout-undo][req:Undo and redo semantics][cap:hefesto-performance][req:Edit latency]") {
  const auto root = hefesto::test::pinned_sky130a();
  if (!root) {
    SKIP("sky130A with sky130_fd_sc_hd not in .cache/pdks");
  }
  hefesto::platform::LocalFileSystem local;
  hefesto::diag::CollectingSink tech_sink;
  auto tech = hefesto::tech::load_technology(local, (*root / "libs.tech/magic/sky130A.tech").string(),
                                             hefesto::tech::LoadOptions{}, tech_sink);
  REQUIRE(tech.ok());
  const auto m1 = tech.value()->find_type("metal1");
  REQUIRE(m1.ok());

  hefesto::editor::EditorState editor;
  editor.technology = tech.value();
  {
    hefesto::cell::test::CellIo io{tech.value(), &local};
    const fs::path mag = *root / "libs.ref/sky130_fd_sc_hd/mag/sky130_fd_sc_hd__dfxtp_1.mag";
    hefesto::diag::Status status = hefesto::diag::Done{};
    (void)io.read("sky130_fd_sc_hd__dfxtp_1", slurp(mag), &status);
    REQUIRE(status.ok());
    editor.cells = std::move(io.library);
  }
  db::CellDef* leaf = editor.cells.find("sky130_fd_sc_hd__dfxtp_1");
  REQUIRE(leaf != nullptr);
  db::recompute_bbox(*leaf);
  REQUIRE(leaf->bbox().width() > 100);
  db::CellDef* top = editor.cells.create("top", tech.value()->plane_count()).value();
  db::PlaceOptions array;
  array.id = "x";
  array.array = db::ArrayInfo{0, 99, 0, 99, leaf->bbox().x1 - leaf->bbox().x0, leaf->bbox().y1 - leaf->bbox().y0};
  REQUIRE(db::place(*top, *leaf, array).ok());
  db::recompute_bbox(*top);

  hefesto::undo::DbRecorder recorder(editor.undo);
  editor.cells.set_recorder(&recorder);
  const db::Rect span = top->bbox();
  const db::Coord pitch_x = (span.x1 - span.x0) / 100;
  const db::Coord pitch_y = (span.y1 - span.y0) / 100;
  REQUIRE(pitch_x > 2);
  REQUIRE(pitch_y > 2);

  // One command painting 10,000 separate rectangles (one per cell of the
  // array), then one erasing them all at once.
  const auto paint_all = [&] {
    for (db::Coord i = 0; i < 100; ++i) {
      for (db::Coord j = 0; j < 100; ++j) {
        const db::Coord x = span.x0 + i * pitch_x;
        const db::Coord y = span.y0 + j * pitch_y;
        REQUIRE(db::paint(*top, *tech.value(), db::Rect{x, y, x + pitch_x / 2, y + pitch_y / 2}, *m1.id).ok());
      }
    }
  };
  const auto erase_all = [&] {
    REQUIRE(db::erase(*top, *tech.value(), span, hefesto::tech::TypeMask::of(*m1.id)).ok());
  };
  const double paint_ms = millis([&] {
    paint_all();
    REQUIRE(editor.undo.close_step());
  });
  const auto painted = db::test::rects(top->plane(*tech.value()->home_plane(*m1.id)));
  REQUIRE(painted.size() == 10000);
  const double erase_ms = millis([&] {
    erase_all();
    REQUIRE(editor.undo.close_step());
  });
  const double undo_ms = millis([&] {
    auto r = editor.undo.undo(editor);
    REQUIRE(r.ok());
    REQUIRE(r.value().steps == 1);
  });
  CHECK(db::test::rects(top->plane(*tech.value()->home_plane(*m1.id))) == painted);
  const double redo_ms = millis([&] { REQUIRE(editor.undo.redo(editor).steps == 1); });
  CHECK(db::test::rects(top->plane(*tech.value()->home_plane(*m1.id))).empty());
  const double undo_paint_ms = millis([&] {
    auto r = editor.undo.undo(editor, 2);
    REQUIRE(r.ok());
    REQUIRE(r.value().steps == 2);
  });
  CHECK(db::test::rects(top->plane(*tech.value()->home_plane(*m1.id))).empty());
  // One `paint` command over the whole array that merges the 10,000
  // rectangles into one (the "command affecting 10,000 rectangles" of the
  // target), and its undo.
  paint_all();
  REQUIRE(editor.undo.close_step());
  const double cover_ms = millis([&] {
    REQUIRE(db::paint(*top, *tech.value(), span, *m1.id).ok());
    REQUIRE(editor.undo.close_step());
  });
  CHECK(db::test::rects(top->plane(*tech.value()->home_plane(*m1.id))).size() == 1);
  const double undo_cover_ms = millis([&] {
    auto r = editor.undo.undo(editor);
    REQUIRE(r.ok());
    REQUIRE(r.value().steps == 1);
  });
  CHECK(db::test::rects(top->plane(*tech.value()->home_plane(*m1.id))) == painted);
  REQUIRE(editor.undo.undo(editor).ok());
  CHECK(db::test::rects(top->plane(*tech.value()->home_plane(*m1.id))).empty());
  // The same edits without a recorder: the cost of recording.
  editor.cells.set_recorder(nullptr);
  const double plain_paint_ms = millis(paint_all);
  const double plain_erase_ms = millis(erase_all);

  WARN("undo benchmark (ms): paint 10k rects " << paint_ms << " (unrecorded " << plain_paint_ms << "), erase 10k "
                                              << erase_ms << " (unrecorded " << plain_erase_ms << "), undo erase "
                                              << undo_ms << ", redo erase " << redo_ms << ", undo erase+paint "
                                              << undo_paint_ms << ", paint over 10k " << cover_ms
                                              << ", undo it " << undo_cover_ms);
#ifdef NDEBUG
  CHECK(cover_ms < 50.0);
  CHECK(undo_cover_ms < 50.0);
  CHECK(erase_ms < 50.0);
  CHECK(undo_ms < 50.0);
  CHECK(redo_ms < 50.0);
#endif
}
