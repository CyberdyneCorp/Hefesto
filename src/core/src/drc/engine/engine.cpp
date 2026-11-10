// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Step squares, commits, catch-up, the forced recheck, the flat check and
// the error-plane tables (drc-engine "DRC check and error planes",
// "Pending list and background scheduling", "Step square processing",
// "Forced recheck of an area", "Flat check", "Halo override"). Ported from
// Magic 8.3.694 drc/DRCcontin.c (DRCContinuous, drcCheckTile,
// drcPutBackFunc, drcIncludeArea), drc/DRCmain.c (DRCCheck, drcCheckFunc,
// DRCCatchUp, drcPaintError), drc/DRCsubcell.c (DRCFlatCheck) and
// textio/txCommands.c (the DRCBackGround save/restore around a command);
// Magic is (c) the Regents of the University of California, distributed
// under its permissive license (THIRD_PARTY_NOTICES.md).
//
// Owner: ENGINE (openspec/changes/m3-drc/design.md D2).

#include "hefesto/drc/engine/engine.hpp"
#include "hefesto/drc/rules/style.hpp"

#include <array>
#include <string>

#include "drc/engine/codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/drc/engine/marking.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "internal.hpp"

namespace hefesto::drc {

namespace {

constexpr std::array<tech::TileType, 4> kErrorBits = {tech::kSpace, tech::kErrorP, tech::kErrorS, tech::kErrorPS};

std::size_t error_bits(tech::TileType t) noexcept {
  for (std::size_t i = 0; i < kErrorBits.size(); ++i) {
    if (kErrorBits[i] == t) {
      return i;
    }
  }
  return 0;
}

// The error plane's paint (P|S) and erase (P&~S) tables (DBtpaint.c
// dbTechPaintErasePlanes for TT_ERROR_P/S/PS) as rows over every type.
std::array<db::PaintRow, 8> make_error_rows() {
  std::array<db::PaintRow, 8> rows{};
  for (std::size_t op = 0; op < kErrorBits.size(); ++op) {
    rows[op] = identity_paint_row(tech::kSpace);
    rows[4 + op] = identity_paint_row(tech::kSpace);
    for (std::size_t have = 0; have < tech::kMaxTileTypes; ++have) {
      const auto h = static_cast<tech::TileType>(have);
      const std::size_t bits = error_bits(h);
      const bool error_type = bits != 0 || h == tech::kSpace;
      rows[op][have] = error_type ? kErrorBits[bits | op] : h;
      rows[4 + op][have] = error_type ? kErrorBits[bits & ~op] : h;
    }
  }
  return rows;
}

const std::array<db::PaintRow, 8>& error_rows() {
  static const std::array<db::PaintRow, 8> rows = make_error_rows();
  return rows;
}

// drcPaintError: paints DRCErrorType (the context's error type, followed
// once the context exists) into a temporary error plane.
class TempPlaneSink final : public ErrorSink {
 public:
  void follow(const tech::TileType& error_type) noexcept { error_type_ = &error_type; }
  [[nodiscard]] const db::Plane& plane() const noexcept { return plane_; }

 protected:
  void on_error(const db::Rect& area, const Rule& /*rule*/) override {
    (void)plane_.paint(area, error_paint_row(error_type_ != nullptr ? *error_type_ : tech::kErrorP));
  }

 private:
  const tech::TileType* error_type_ = nullptr;
  db::Plane plane_;
};

// The first check tile of `cell` in area-enumeration order.
std::optional<db::TileView> first_check_tile(const db::CellDef& cell) {
  std::optional<db::TileView> found;
  cell.plane(tech::kPlaneDrcCheck).enumerate(db::whole_plane(), ~tech::TypeMask::of(tech::kSpace),
                                             [&](const db::TileView& t) {
                                               found = t;
                                               return false;
                                             });
  return found;
}

// drcCheckTile's square, erase box and check box.
void square_geometry(const db::CellDef& cell, const db::TileView& tile, std::int32_t step, std::int32_t halo,
                     SquareResult& out) {
  const db::Coord x = detail::floor_multiple(tile.area.x0, step);
  const db::Coord y = detail::floor_multiple(tile.area.y0, step);
  out.square = db::Rect{x, y, x + step, y + step};
  db::Rect erase = detail::kNullRect;
  cell.plane(tech::kPlaneDrcCheck).enumerate(out.square, ~tech::TypeMask::of(tech::kSpace), [&](const db::TileView& t) {
    erase = db::include(erase, t.area);
    return true;
  });
  out.erase_box = detail::clip(erase, out.square);
  out.check_box = detail::clip(db::expand(out.erase_box, halo), out.square);
}

std::vector<ErrorTile> tiles_of(const db::Plane& plane) {
  std::vector<ErrorTile> out;
  plane.enumerate(db::whole_plane(), ~tech::TypeMask::of(tech::kSpace), [&](const db::TileView& t) {
    out.push_back(ErrorTile{t.area, t.type});
    return true;
  });
  return out;
}

}  // namespace

const db::PaintRow& error_paint_row(tech::TileType paint) noexcept {
  return error_rows()[error_bits(paint)];
}

const db::PaintRow& error_erase_row(tech::TileType erase) noexcept {
  return error_rows()[4 + error_bits(erase)];
}

std::optional<SquareResult> process_square(const editor::EditorState& editor, const CancelToken& cancel) {
  db::CellDef* cell = editor.drc.pending.front();
  if (cell == nullptr) {
    return std::nullopt;
  }
  SquareResult out;
  out.cell = cell;
  out.generation = editor.drc.worker ? editor.drc.worker->generation() : 0;
  out.epoch = editor.drc.worker ? editor.drc.worker->epoch() : 0;
  const std::optional<db::TileView> tile = first_check_tile(*cell);
  if (!tile) {
    out.cell_done = true;
    return out;
  }
  out.check_type = tile->type;
  TempPlaneSink sink;
  sink.set_statistics(&out.statistics);
  detail::Context ctx(editor, sink, cancel);
  sink.follow(ctx.error_type);
  square_geometry(*cell, *tile, ctx.step, ctx.halo, out);
  ++out.statistics.squares;
  ctx.error_type = tech::kErrorS;
  detail::interaction_check(ctx, *cell, out.square, out.erase_box);
  if (ctx.cancelled) {
    return std::nullopt;
  }
  out.errors = tiles_of(sink.plane());
  return out;
}

namespace {

// DRCContinuous: no check tiles left, knock the cell off the list.
void finish_cell(editor::EditorState& editor, db::CellDef& cell) {
  if (editor.drc.pending.front() != &cell) {
    return;  // the list changed since the square started
  }
  db::recompute_bbox(cell, editor.drc.compat_settings().magic_compat);
  editor.drc.pending.pop_front();
}

void apply_square(editor::EditorState& editor, const SquareResult& r) {
  db::CellDef& cell = *r.cell;
  const tech::Technology& tech = *editor.technology;
  db::Plane& check = cell.plane(tech::kPlaneDrcCheck);
  db::Plane& errors = cell.plane(tech::kPlaneDrcError);
  (void)check.paint(r.erase_box, db::erase_row(tech, tech::kPlaneDrcCheck, r.check_type));
  (void)errors.paint(r.erase_box, error_erase_row(tech::kErrorP));
  (void)errors.paint(r.check_box, error_erase_row(tech::kErrorS));
  for (const ErrorTile& e : r.errors) {
    (void)errors.paint(e.area, error_paint_row(e.type));
  }
  // Saved with the cell, so the cell is modified, but without a new
  // timestamp; non-editable and vendor-GDS cells are left unmodified.
  if (!cell.flags().no_edit && !cell.flags().vendor_gds) {
    cell.flags().modified = true;
  }
}

}  // namespace

void commit(editor::EditorState& editor, const SquareResult& result) {
  if (result.cell == nullptr) {
    return;
  }
  if (editor.drc.worker) {
    editor.drc.worker->advance_epoch();
  }
  if (result.cell_done) {
    finish_cell(editor, *result.cell);
    return;
  }
  apply_square(editor, result);
  editor.drc.statistics += result.statistics;
  editor.drc.events.publish_square(*result.cell, result.square);
}

diag::Status catchup(editor::EditorState& editor, diag::DiagnosticSink* messages) {
  if (editor.drc.pending.empty()) {
    return diag::Done{};
  }
  editor.drc.events.publish_state(CheckerState::busy);
  while (std::optional<SquareResult> r = process_square(editor)) {
    commit(editor, *r);
    if (!r->cell_done) {
      after_basic_check(editor, messages);  // DRCBasicCheck ends with drcCifCheck
    }
  }
  editor.drc.events.publish_state(CheckerState::idle);
  return diag::Done{};
}

namespace {

// DBCellReadArea: reads every unavailable cell under `area`; the first
// cell that cannot be read, or nullptr.
db::CellDef* read_area(db::CellDef& def, const db::Rect& area, const CellReadFn& read) {
  if (!def.flags().available && read && !read(def)) {
    return &def;
  }
  db::CellDef* failed = nullptr;
  (void)detail::search_elements(def, area, [&](const detail::Element& e, const db::Rect& child_area) {
    failed = read_area(*e.use->def, child_area, read);  // NOLINT: the hierarchy is the editor's (mutable)
    return failed != nullptr ? detail::Visit::stop : detail::Visit::next;
  });
  return failed;
}

// drcCheckFunc: marks `area` (clipped to the cell) in `def`, then its
// children, then enqueues `def`.
detail::Visit check_func(editor::EditorState& editor, db::CellDef& def, const db::Rect& area) {
  const db::Rect cell_area = db::expand(detail::clip(area, def.bbox()), current_halo(editor));
  (void)def.plane(tech::kPlaneDrcCheck).paint(cell_area,
                                              db::paint_row(*editor.technology, tech::kPlaneDrcCheck, tech::kCheckPaint));
  (void)detail::search_elements(def, area, [&](const detail::Element& e, const db::Rect& child_area) {
    return check_func(editor, *e.use->def, child_area);
  });
  check_this(editor, def, std::nullopt);
  return db::surround(cell_area, def.bbox()) ? detail::Visit::skip_array : detail::Visit::next;
}

}  // namespace

diag::Status force_recheck(editor::EditorState& editor, db::CellDef& root, const db::Rect& area,
                           const CellReadFn& read) {
  if (db::CellDef* failed = read_area(root, area, read)) {
    return diag::make(codes::kSubtreeReadFailure,
                      "Failure to read in entire subtree of cell.\nFailed on cell " + failed->name() + ".");
  }
  (void)check_func(editor, root, area);
  return diag::Done{};
}

namespace {

class CountingSink final : public ErrorSink {
 protected:
  void on_error(const db::Rect& /*area*/, const Rule& /*rule*/) override {}
};

}  // namespace

diag::Outcome<std::size_t> flat_check(const editor::EditorState& editor, const db::CellDef& root,
                                      const db::Rect& area) {
  CountingSink sink;
  detail::Context ctx(editor, sink, CancelToken{});
  constexpr db::Coord kChunk = 300;
  for (db::Coord y = area.y0; y < area.y1; y += kChunk) {
    for (db::Coord x = area.x0; x < area.x1; x += kChunk) {
      const db::Rect chunk{x, y, std::min(x + kChunk, area.x1), std::min(y + kChunk, area.y1)};
      const db::Rect yank_area = db::expand(chunk, ctx.halo);
      ctx.clear_yank();
      detail::yank_paint(ctx, root, yank_area, db::kIdentity);
      detail::yank_hints(ctx, root, yank_area, db::kIdentity);
      (void)ctx.basic(ctx.yank, yank_area, chunk);
    }
  }
  return sink.count();
}

diag::Outcome<std::optional<db::Rect>> interaction_area(const editor::EditorState& editor, const db::CellDef& cell,
                                                        const db::Rect& area, std::int32_t radius) {
  CountingSink sink;
  detail::Context ctx(editor, sink, CancelToken{});
  db::Rect found;
  if (detail::find_interactions(ctx, cell, area, radius, found) <= 0) {
    return std::optional<db::Rect>{};
  }
  return std::optional<db::Rect>{found};
}

CheckOutcome interaction_check(const editor::EditorState& editor, const db::CellDef& cell, const db::Rect& area,
                               const db::Rect& erase_box, ErrorSink& sink, const CancelToken& cancel) {
  detail::Context ctx(editor, sink, cancel);
  ctx.error_type = tech::kErrorS;
  detail::interaction_check(ctx, cell, area, erase_box);
  return ctx.cancelled ? CheckOutcome::cancelled : CheckOutcome::completed;
}

BackgroundMode begin_command(editor::EditorState& editor) {
  const BackgroundMode saved = editor.drc.background;
  if (saved != BackgroundMode::off) {
    editor.drc.background = BackgroundMode::not_set;
  }
  return saved;
}

void end_command(editor::EditorState& editor, BackgroundMode saved) {
  if (editor.drc.background == BackgroundMode::not_set) {
    editor.drc.background = saved;
  }
}

}  // namespace hefesto::drc
