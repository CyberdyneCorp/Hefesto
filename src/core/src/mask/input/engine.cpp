// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The read engine behind ReadSink (capability mask-input-styles; design D9):
// the cells being read and their read planes, painting rectangles and
// polygons ("Polygon to region conversion" through db::decompose_polygon),
// calls with the copyup copy ("Input operator copyup"), labels ("Label
// placement semantics during read"), properties, and the end of a cell and
// of the read.
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFrdcl.c
// (CIFReadCellInit, cifFindCell, CIFParseStart's cell set-up,
// CIFParseFinish, CIFReadCellCleanup's top-level processing),
// cif/CIFrdpt.c (PaintPolygon) and calma/CalmaRdcl.c (gdsCopyPaintFunc,
// the copyup copy of calmaElementSref), Copyright (C) 1985, 1990 Regents of
// the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: INPUT (openspec/changes/m5-mask-gds-cif/design.md D2).

#include "hefesto/mask/input/engine.hpp"

#include <algorithm>
#include <cstdlib>

#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/polygon.hpp"
#include "mask/input/engine_state.hpp"
#include "mask/input/reader.hpp"

namespace hefesto::mask {

namespace {

constexpr std::string_view kEngineMisuse = "MASK-INPUT-016";
constexpr std::string_view kCopyTooLarge = "MASK-INPUT-017";
/// Array elements a copyup copy may visit (D21 resource cap).
constexpr std::int64_t kMaxCopyElements = std::int64_t{1} << 20;

std::vector<MaskPlane*> acquire_planes(PlanePool& pool, std::size_t count) {
  std::vector<MaskPlane*> planes;
  planes.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    planes.push_back(&pool.acquire());
  }
  return planes;
}

/// The next free port index of `cell`, or the index of a port with `text`.
std::uint32_t port_index(const db::CellDef& cell, const std::string& text) {
  std::uint32_t next = 0;
  for (const db::Label& l : cell.labels()) {
    if (!l.is_port()) {
      continue;
    }
    if (l.text == text) {
      return l.port_index;
    }
    next = std::max(next, l.port_index + 1);
  }
  return next;
}

std::int32_t array_count(std::int32_t lo, std::int32_t hi) noexcept {
  return static_cast<std::int32_t>(std::min<std::int64_t>(std::abs(std::int64_t{hi} - lo) + 1, 1 << 16));
}

}  // namespace

ReadEngine::ReadEngine(editor::EditorState& editor, const InputStyle& style, const compat::CompatSettings& compat)
    : ReadEngine(editor, style, compat, Options{}) {}

ReadEngine::ReadEngine(editor::EditorState& editor, const InputStyle& style, const compat::CompatSettings& compat,
                       const Options& options)
    : editor_(editor), compat_(compat), state_(std::make_unique<State>()) {
  state_->style = style;
  state_->options = options;
  const std::size_t count = std::min(style.cif_layers.size(), kMaxInputLayers);
  state_->top = acquire_planes(state_->pool, count);
  state_->cell = acquire_planes(state_->pool, count);
}

ReadEngine::~ReadEngine() = default;

const InputStyle& ReadEngine::style() const noexcept { return state_->style; }

db::CellDef* ReadEngine::current_cell() const noexcept { return state_->reading; }

std::vector<MaskPlane*>& ReadEngine::read_planes() noexcept {
  return state_->reading != nullptr ? state_->cell : state_->top;
}

db::CellDef* ReadEngine::target_cell() const noexcept {
  return state_->reading != nullptr ? state_->reading : state_->options.top;
}

diag::Outcome<db::CellDef*> ReadEngine::find_or_create(std::string_view name) {
  if (db::CellDef* def = editor_.cells.find(name)) {
    return def;
  }
  return editor_.cells.create(name, editor_.technology->plane_count());
}

// --- ReadSink -------------------------------------------------------------------

diag::Status ReadEngine::begin_cell(std::string_view name) {
  const diag::Outcome<db::CellDef*> def = find_or_create(name);
  if (!def.ok()) {
    return def.error();
  }
  return begin_cell(*def.value());
}

diag::Status ReadEngine::begin_cell(db::CellDef& cell) {
  if (state_->reading != nullptr) {
    return diag::make(kEngineMisuse, "read engine: cell " + cell.name() + " started inside cell " +
                                         state_->reading->name());
  }
  state_->reading = &cell;
  cell.flags().available = true;  // DBCellSetAvail
  for (MaskPlane* plane : state_->cell) {
    plane->clear();
  }
  return diag::Done{};
}

diag::Status ReadEngine::paint(std::size_t cif_layer, const ReadShape& shape) {
  std::vector<MaskPlane*>& planes = read_planes();
  if (cif_layer >= planes.size()) {
    return diag::Done{};  // a layer no rule uses is discarded
  }
  MaskPlane& plane = *planes[cif_layer];
  if (const auto* rect = std::get_if<db::Rect>(&shape)) {
    return plane.paint(*rect);
  }
  const Polygon& polygon = std::get<Polygon>(shape);
  const auto pieces = db::decompose_polygon(polygon);
  if (!pieces.ok()) {
    return diag::Done{};  // fewer than three vertices: nothing to paint
  }
  for (const db::PolygonPiece& piece : pieces.value()) {
    if (piece.triangle) {
      (void)plane.paint_triangle(piece.area, piece.triangle->diagonal, piece.triangle->right_side,
                                 compat_.magic_compat);
    } else {
      (void)plane.paint(piece.area);
    }
  }
  return diag::Done{};
}

diag::Status ReadEngine::label(const ReadLabel& label) {
  db::CellDef* cell = target_cell();
  if (cell == nullptr || label.text.empty()) {
    return diag::Done{};
  }
  CifLayerLabels mapping;
  if (label.cif_layer) {
    mapping = state_->style.label_of(*label.cif_layer);
  }
  if (mapping.type == kDiscardLabels) {
    return diag::Done{};
  }
  if (mapping.kind == LabelKind::cell_id) {
    return editor_.cells.rename(*cell, label.text);  // the text names the cell instead
  }
  db::Label l;
  l.text = label.text;
  l.type = static_cast<tech::TileType>(mapping.type);
  l.rect = scale_rect(label.area, Snap::any);
  l.justification = static_cast<db::Position>(label.position);
  l.sticky = mapping.kind != LabelKind::none && l.type != tech::kSpace;
  if (mapping.kind == LabelKind::port || label.port) {
    l.port_index = port_index(*cell, l.text);
    l.port_directions = db::kPortNorth | db::kPortEast | db::kPortSouth | db::kPortWest;
  }
  const auto put = db::put_label(*cell, std::move(l));
  if (!put.ok()) {
    return put.error();
  }
  return diag::Done{};
}

diag::Status ReadEngine::call(std::string_view child, const db::Transform& transform, const db::ArrayInfo& array) {
  if (target_cell() == nullptr) {
    return diag::make(kEngineMisuse, "read engine: call of " + std::string{child} + " without a cell");
  }
  const diag::Outcome<db::CellDef*> def = find_or_create(child);
  if (!def.ok()) {
    return def.error();
  }
  if (state_->options.copyup_on_call) {
    copy_saved_planes(*def.value(), transform, array);
  }
  // Translation, then array spacing, exactly; a later subdivision multiplies
  // the values converted before it.
  const std::array<db::Coord, 4> in{transform.c, transform.f, array.xsep, array.ysep};
  std::array<std::int64_t, 4> out{};
  for (std::size_t i = 0; i < in.size(); ++i) {
    const std::int64_t before = state_->style.scale_factor;
    out[i] = scale_coord(in[i], Snap::exact);
    const std::int64_t after = std::max<std::int64_t>(state_->style.scale_factor, 1);
    for (std::size_t k = 0; k < i && before != after; ++k) {
      out[k] = std::clamp<std::int64_t>(out[k] * (before / after), db::kPlaneMin, db::kPlaneMax);
    }
  }
  db::Transform t = transform;
  t.c = static_cast<db::Coord>(out[0]);
  t.f = static_cast<db::Coord>(out[1]);
  db::ArrayInfo a = array;
  a.xsep = static_cast<db::Coord>(out[2]);
  a.ysep = static_cast<db::Coord>(out[3]);
  const auto placed = place(*def.value(), t, a);
  if (!placed.ok()) {
    return placed.error();
  }
  return diag::Done{};
}

diag::Status ReadEngine::property(std::string_view key, std::string_view value) {
  if (db::CellDef* cell = target_cell()) {
    db::set_property(*cell, key, db::PropertyValue{std::string{value}});
  }
  return diag::Done{};
}

diag::Status ReadEngine::end_cell() {
  db::CellDef* cell = state_->reading;
  if (cell == nullptr) {
    return diag::make(kEngineMisuse, "read engine: end of a cell without a cell");
  }
  paint_current(state_->cell, *cell);
  (void)db::adjust_labels(*cell, *editor_.technology, db::whole_plane());
  db::recompute_bbox(*cell, compat_.magic_compat);
  state_->reading = nullptr;
  return diag::Done{};
}

diag::Status ReadEngine::finish() {
  if (state_->reading != nullptr) {
    (void)end_cell();
  }
  if (db::CellDef* top = state_->options.top) {
    paint_current(state_->top, *top);
    (void)db::adjust_labels(*top, *editor_.technology, db::whole_plane());
    db::recompute_bbox(*top, compat_.magic_compat);
  }
  for (auto& [cell, planes] : state_->saved) {
    for (MaskPlane* plane : planes) {
      if (plane != nullptr) {
        state_->pool.release(*plane);
      }
    }
  }
  state_->saved.clear();
  return diag::Done{};
}

// --- lower level ------------------------------------------------------------------

const std::vector<MaskPlane*>* ReadEngine::saved_planes(const db::CellDef& cell) const {
  const auto it = state_->saved.find(&cell);
  return it == state_->saved.end() ? nullptr : &it->second;
}

void ReadEngine::copy_saved_planes(const db::CellDef& child, const db::Transform& transform,
                                   const db::ArrayInfo& array) {
  const std::vector<MaskPlane*>* saved = saved_planes(child);
  if (saved == nullptr) {
    return;
  }
  std::vector<MaskPlane*>& planes = read_planes();
  const std::int32_t cols = array_count(array.xlo, array.xhi);
  const std::int32_t rows = array_count(array.ylo, array.yhi);
  if (std::int64_t{cols} * rows > kMaxCopyElements) {
    state_->emit(diag::make_native(kCopyTooLarge, "copyup planes of " + child.name() + " not copied into a " +
                                               std::to_string(cols) + " x " + std::to_string(rows) + " array"));
    return;
  }
  const auto offset = [](std::int64_t i, db::Coord sep) {
    return static_cast<db::Coord>(std::clamp<std::int64_t>(i * sep, db::kPlaneMin, db::kPlaneMax));
  };
  for (std::size_t layer = 0; layer < saved->size() && layer < planes.size(); ++layer) {
    const MaskPlane* source = (*saved)[layer];
    if (source == nullptr) {
      continue;
    }
    const std::vector<db::TileView> tiles = source->solid_tiles();
    for (std::int32_t x = 0; x < cols; ++x) {
      for (std::int32_t y = 0; y < rows; ++y) {
        const db::Transform t = db::compose(db::translation(offset(x, array.xsep), offset(y, array.ysep)), transform);
        for (const db::TileView& tile : tiles) {
          db::TileView moved = tile;
          if (tile.split) {
            const db::TriangleSide side = db::transform_triangle(t, tile.diagonal, tile.right_side);
            moved.diagonal = side.diagonal;
            moved.right_side = side.right_side;
          }
          input_detail::paint_tile(*planes[layer], moved, db::apply(t, tile.area), compat_.magic_compat);
        }
      }
    }
  }
}

diag::Outcome<db::CellUse*> ReadEngine::place(db::CellDef& child, const db::Transform& transform,
                                              const db::ArrayInfo& array, std::string_view id) {
  db::CellDef* parent = target_cell();
  if (parent == nullptr) {
    return diag::make(kEngineMisuse, "read engine: placement of " + child.name() + " without a cell");
  }
  db::PlaceOptions options;
  if (!id.empty()) {
    options.id = std::string{id};
  }
  options.transform = transform;
  options.array = array;
  return db::place(*parent, child, options);
}

}  // namespace hefesto::mask
