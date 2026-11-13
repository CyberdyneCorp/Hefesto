// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold (openspec/changes/m5-mask-gds-cif/design.md D3/D4): the tables,
// paint, erase and the pool delegate to db::Plane; subtract() is a
// CORE-NOTIMPL-005 stub. Owner: OPS.

#include "hefesto/mask/plane/plane.hpp"

#include <algorithm>

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::mask {

namespace {

db::PaintRow filled_row(tech::TileType type) {
  db::PaintRow row{};
  row.fill(type);
  return row;
}

}  // namespace

const db::PaintRow& cif_paint_table() noexcept {
  static const db::PaintRow row = filled_row(kMaskSolid);
  return row;
}

const db::PaintRow& cif_erase_table() noexcept {
  static const db::PaintRow row = filled_row(tech::kSpace);
  return row;
}

diag::Status MaskPlane::paint(const db::Rect& area) {
  return plane_.paint(area, cif_paint_table());
}

diag::Status MaskPlane::erase(const db::Rect& area) {
  return plane_.paint(area, cif_erase_table());
}

diag::Status MaskPlane::subtract(const MaskPlane& /*other*/) {
  return cmd::not_implemented_status("mask::MaskPlane::subtract");
}

void MaskPlane::clear() {
  plane_ = db::Plane{};
}

bool MaskPlane::empty() const {
  return plane_.empty();
}

MaskPlane& PlanePool::acquire() {
  if (!free_.empty()) {
    MaskPlane* plane = free_.back();
    free_.pop_back();
    plane->clear();
    return *plane;
  }
  planes_.push_back(std::make_unique<MaskPlane>());
  return *planes_.back();
}

void PlanePool::release(MaskPlane& plane) {
  if (std::find(free_.begin(), free_.end(), &plane) == free_.end()) {
    free_.push_back(&plane);
  }
}

}  // namespace hefesto::mask
