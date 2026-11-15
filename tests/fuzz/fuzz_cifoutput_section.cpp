// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the technology `cifoutput` section reader
// (capability mask-output-styles, the operator arguments of
// mask-output-operators; hefesto-testing-strategy "Parser fuzzing";
// m5-mask-gds-cif design D21). The first byte is a mode: bits 0-2 pick the
// base technology (mask_section_fuzz.hpp), bit 3 magic-compat OFF, bits
// 4-5 the grid rescale n (1-4) and bits 6-7 its d (1-4). The rest is the
// body of a `cifoutput` section that replaces the base's own one.
//
// The technology is loaded (which reads the first style's grid), every
// style is read and selected, the current one rescaled by n/d and asked
// for its halo and contact cut sizes, and then a small cell (a 20 x 20 box
// of every user type, and a child in a 2 x 2 array) is written as GDS and
// as CIF into memory through the current style.
//
// Resource cap: the write is skipped when a cut operator (squares, slots)
// could make more than kMaxCuts cuts over the cell grown by every operator
// distance of the style, so a time-out is a real hang, not a large cut
// array (an accepted limit: generation is proportional to its output,
// docs/contributing/security.md).
//
// Owner: INTEGRATION (tests/fuzz/, design D2), written by the FUZZ workstream.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "fuzz_common.hpp"
#include "hefesto/cif/write/write.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/write/write.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "mask_section_fuzz.hpp"

namespace {

namespace db = hefesto::db;
namespace mask = hefesto::mask;
namespace tech = hefesto::tech;

constexpr db::Coord kBoxSize = 20;
constexpr double kMaxCuts = 1e6;
constexpr double kMaxSide = 1e9;

const hefesto::fuzz::SectionBases& bases() {
  static const hefesto::fuzz::SectionBases instance{"cifoutput", "tests/data/mask/style/styles.tech"};
  return instance;
}

/// How far an operator can grow its operand (grow, bloat, bridge); 0 for
/// operators that do not grow.
double op_growth(const mask::LayerOp& op) {
  switch (op.kind) {
    case mask::OpKind::grow:
    case mask::OpKind::grow_min:
    case mask::OpKind::grow_grid:
    case mask::OpKind::bridge:
    case mask::OpKind::bridge_lim:
      return std::abs(static_cast<double>(op.distance));
    default:
      break;
  }
  double growth = 0;
  if (op.bloat) {
    for (const std::int32_t d : op.bloat->distance) {
      growth = std::max(growth, std::abs(static_cast<double>(d)));
    }
  }
  return growth;
}

/// The smallest cut pitch of a squares or slots operator; 0 for others.
double cut_pitch(const mask::LayerOp& op) {
  const auto value = [&op](std::size_t i) {
    return i < op.values.size() ? std::abs(static_cast<double>(op.values[i])) : 0.0;
  };
  switch (op.kind) {
    case mask::OpKind::squares:
    case mask::OpKind::squares_grid:
      return std::max(1.0, value(mask::squares_value::size) + value(mask::squares_value::sep));
    case mask::OpKind::slots:
      return std::max(1.0, std::min(value(mask::slots_value::ssize) + value(mask::slots_value::ssep),
                                    value(mask::slots_value::lsize) + value(mask::slots_value::lsep)));
    default:
      return 0.0;
  }
}

/// True when generating the sample cell through `style` stays within the
/// caps: each layer's extent is the cell grown by the growth of the layers
/// it reads plus its own operators; each cut operator cuts that extent.
bool generation_bounded(const mask::OutputStyle& style, db::Coord cell_side) {
  const double base = static_cast<double>(cell_side) * std::max(1, style.scale.scale_factor);
  std::vector<double> growth(style.layers.size(), 0.0);
  double cuts = 0;
  for (std::size_t i = 0; i < style.layers.size(); ++i) {
    for (const mask::LayerOp& op : style.layers[i].ops) {
      for (const std::size_t source : op.cif_layers) {
        growth[i] = std::max(growth[i], source < i ? growth[source] : 0.0);
      }
      growth[i] += op_growth(op);
      const double side = base + 2 * growth[i];
      if (side > kMaxSide) {
        return false;
      }
      const double pitch = cut_pitch(op);
      cuts += pitch > 0 ? (side / pitch) * (side / pitch) * 4 : 0.0;
    }
  }
  return cuts <= kMaxCuts;
}

/// The side of the square grid that holds one box of every user type.
db::Coord grid_side(const tech::Technology& technology) {
  const auto types = static_cast<double>(technology.user_type_end() - tech::kFirstUserType);
  return kBoxSize * static_cast<db::Coord>(std::ceil(std::sqrt(std::max(1.0, types))));
}

/// The sample hierarchy: `child` with a grid of boxes, one of every user
/// type, used by `top` in a 2 x 2 array above the same boxes; `top` spans
/// 3 x 3 grids.
db::CellDef* sample_cells(hefesto::editor::EditorState& editor, const tech::Technology& technology) {
  auto child = editor.cells.create("child", technology.plane_count());
  auto top = editor.cells.create("top", technology.plane_count());
  if (!child.ok() || !top.ok()) {
    return nullptr;
  }
  const db::Coord side = grid_side(technology);
  const db::Coord columns = side / kBoxSize;
  for (db::CellDef* def : {child.value(), top.value()}) {
    def->flags().available = true;
    db::Coord k = 0;
    for (tech::TileType t = tech::kFirstUserType; t < technology.user_type_end(); ++t, ++k) {
      const db::Coord x = (k % columns) * kBoxSize;
      const db::Coord y = (k / columns) * kBoxSize;
      (void)db::paint_type(*def, technology, db::Rect{x, y, x + kBoxSize, y + kBoxSize}, t);
    }
    db::recompute_bbox(*def);
  }
  db::PlaceOptions place;
  place.transform.f = side;
  place.array = db::ArrayInfo{0, 1, 0, 1, side, side};
  (void)db::place(*top.value(), *child.value(), place);
  db::recompute_bbox(*top.value());
  return top.value();
}

void write_sample(const std::shared_ptr<const tech::Technology>& technology, const mask::OutputStyleSet& styles,
                  const hefesto::compat::CompatSettings& compat) {
  const mask::OutputStyle* current = styles.current_style();
  if (current == nullptr || !generation_bounded(*current, 3 * grid_side(*technology))) {
    return;
  }
  hefesto::editor::EditorState editor;
  hefesto::compat::CompatSettings session_compat = compat;
  editor.technology = technology;
  editor.drc.compat = &session_compat;
  editor.mask.output_styles = styles;
  db::CellDef* top = sample_cells(editor, *technology);
  if (top == nullptr) {
    return;
  }
  hefesto::platform::MemoryFileSystem files;
  hefesto::gds::GdsWriteOptions gds_options;
  gds_options.files = &files;
  gds_options.time_zone = "UTC0";
  gds_options.now = 0;
  gds_options.settings.datestamp = 0;
  (void)hefesto::gds::write(editor, *top, "/out.gds", gds_options);
  hefesto::cif::CifWriteOptions cif_options;
  cif_options.files = &files;
  cif_options.time_zone = "UTC0";
  cif_options.now = 0;
  (void)hefesto::cif::write(editor, *top, "/out.cif", cif_options);
}

void query_current(const mask::OutputStyleSet& styles, const tech::Technology& technology) {
  const mask::OutputStyle* current = styles.current_style();
  if (current == nullptr) {
    return;
  }
  (void)current->halo();
  (void)current->compute_halo();
  for (tech::TileType t = 0; t < technology.type_count(); ++t) {
    (void)current->contact_size(t);
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const auto [mode, body] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  hefesto::compat::CompatSettings compat;
  compat.magic_compat = (mode & 8U) == 0;
  const auto technology = hefesto::fuzz::load_fuzz_technology(bases().text(mode & 7U, body), compat.magic_compat);
  if (technology == nullptr) {
    return 0;
  }
  hefesto::diag::NullSink sink;
  mask::OutputStyleSet styles = mask::read_output_styles(*technology, sink, compat);
  const auto n = static_cast<std::int32_t>(1 + ((mode >> 4U) & 3U));
  const auto d = static_cast<std::int32_t>(1 + ((mode >> 6U) & 3U));
  for (const std::string& name : styles.names()) {
    (void)mask::select_output_style(styles, name, n, d, compat, sink);
  }
  if (!styles.names().empty()) {
    (void)mask::select_output_style(styles, styles.names().front(), compat);
  }
  (void)mask::rescale_output_styles(styles, n, d, compat);
  query_current(styles, *technology);
  write_sample(technology, styles, compat);
  return 0;
}
