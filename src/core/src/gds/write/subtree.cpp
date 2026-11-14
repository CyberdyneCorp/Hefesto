// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Subtree loading of the writers' prepare phase (see
// hefesto/gds/write/subtree.hpp). Ports Magic 8.3.694 (fc5daa9e)
// database/DBexpand.c DBCellReadArea and dbReadAreaFunc,
// database/DBcellsrch.c DBCellSrArea and dbCellSrFunc, and
// database/DBcellbox.c DBArrayOverlap, Copyright (C) 1985, 1990 Regents of
// the University of California (see THIRD_PARTY_NOTICES.md). The index
// arithmetic is 64-bit and clamped; Magic computes it in int.
// Owner: GDSW (shared with CIF).

#include "hefesto/gds/write/subtree.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>

#include "cell/io_session.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/use_index.hpp"

namespace hefesto::gds {

namespace {

struct ElementRange {
  std::int32_t xlo = 0;
  std::int32_t xhi = 0;
  std::int32_t ylo = 0;
  std::int32_t yhi = 0;
};

struct Axis {
  std::int64_t lo;      // the use's canonical (smaller) index
  std::int64_t hi;      // the larger index
  std::int64_t sep;     // the pitch
  std::int64_t a_lo;    // the area, child coordinates
  std::int64_t a_hi;
  std::int64_t b_lo;    // the child's bounding box
  std::int64_t b_hi;
};

// DBArrayOverlap along one axis: the canonical index range [out_lo, out_hi]
// whose elements may overlap the area (out_lo > out_hi when none).
void axis_overlap(Axis a, std::int64_t& out_lo, std::int64_t& out_hi) {
  if (a.sep < 0) {  // flip everything about the axis
    a.sep = -a.sep;
    a = Axis{a.lo, a.hi, a.sep, -a.a_hi, -a.a_lo, -a.b_hi, -a.b_lo};
  }
  if (a.sep != 0) {
    out_lo = a.lo + (a.a_lo - a.b_hi + a.sep - 1) / a.sep;
    out_hi = a.lo + (a.a_hi - a.b_lo) / a.sep;
  } else {
    out_lo = a.lo;
    out_hi = a.hi;
  }
  out_lo = std::max(out_lo, a.lo);
  out_hi = std::min(out_hi, a.hi);
}

// Back from canonical indices to the use's own index direction.
void to_use_indices(std::int32_t first, std::int32_t last, std::int64_t lo, std::int64_t hi, std::int32_t& out_lo,
                    std::int32_t& out_hi) {
  if (first > last) {
    out_hi = static_cast<std::int32_t>(std::int64_t{last} + first - lo);
    out_lo = static_cast<std::int32_t>(std::int64_t{last} + first - hi);
  } else {
    out_lo = static_cast<std::int32_t>(lo);
    out_hi = static_cast<std::int32_t>(hi);
  }
}

ElementRange array_overlap(const db::CellUse& use, const db::Rect& parent_rect) {
  const db::ArrayInfo& ar = use.array;
  if (!ar.is_array()) {
    return ElementRange{ar.xlo, ar.xlo, ar.ylo, ar.ylo};
  }
  const db::Rect child = db::apply(db::invert(use.transform), parent_rect);
  const db::Rect box = use.def->bbox();
  std::int64_t xlo = 0;
  std::int64_t xhi = 0;
  std::int64_t ylo = 0;
  std::int64_t yhi = 0;
  axis_overlap(Axis{std::min(ar.xlo, ar.xhi), std::max(ar.xlo, ar.xhi), ar.xsep, child.x0, child.x1, box.x0, box.x1},
               xlo, xhi);
  axis_overlap(Axis{std::min(ar.ylo, ar.yhi), std::max(ar.ylo, ar.yhi), ar.ysep, child.y0, child.y1, box.y0, box.y1},
               ylo, yhi);
  ElementRange r;
  to_use_indices(ar.xlo, ar.xhi, xlo, xhi, r.xlo, r.xhi);
  to_use_indices(ar.ylo, ar.yhi, ylo, yhi, r.ylo, r.yhi);
  return r;
}

// The search state of one DBCellReadArea.
class ReadArea {
 public:
  ReadArea(const SubtreeLoader& loader, bool halt, const compat::CompatSettings& compat)
      : loader_(loader), halt_(halt), compat_(compat) {}

  // dbReadAreaFunc: 1 to stop (an error), 2 when `area` covers the whole
  // definition, else 0.
  int read_area(db::CellDef& def, const db::Rect& area, std::size_t depth) {
    if (depth > kMaxSubtreeDepth) {
      result_ = SubtreeResult{&def, true};
      return 1;
    }
    if (!def.flags().available && !read(def)) {
      if (halt_) {
        result_.failed = &def;
        return 1;
      }
      return 0;
    }
    int stop = 0;
    (void)db::uses_overlapping(def, area, compat_, [&](db::CellUse& use) {
      stop = read_use(use, area, depth);
      return stop == 0;
    });
    if (stop != 0) {
      return 1;
    }
    return db::surround(area, def.bbox()) ? 2 : 0;
  }

  [[nodiscard]] const SubtreeResult& result() const noexcept { return result_; }

 private:
  bool read(db::CellDef& def) const { return loader_.read && loader_.read(def); }

  // dbCellSrFunc with dbReadAreaFunc as the client: 1 to stop, else 0.
  int read_use(db::CellUse& use, const db::Rect& area, std::size_t depth) {
    if (use.def == nullptr) {
      return 0;
    }
    const ElementRange r = array_overlap(use, area);
    const bool array = use.array.is_array();
    for (std::int64_t y = r.ylo; y <= r.yhi; ++y) {
      for (std::int64_t x = r.xlo; x <= r.xhi; ++x) {
        const db::Transform t = db::element_transform(use, static_cast<std::int32_t>(x), static_cast<std::int32_t>(y));
        const int client = read_area(*use.def, db::apply(db::invert(t), area), depth + 1);
        if (client == 1) {
          return 1;
        }
        if (client == 2 && array) {
          return 0;
        }
      }
    }
    return 0;
  }

  const SubtreeLoader& loader_;
  bool halt_;
  compat::CompatSettings compat_;
  SubtreeResult result_;
};

}  // namespace

SubtreeResult load_subtree(db::CellDef& root, const SubtreeLoader& loader, bool halt_on_error,
                           const compat::CompatSettings& compat) {
  ReadArea walk(loader, halt_on_error, compat);
  (void)walk.read_area(root, root.bbox(), 0);
  if (!walk.result().ok()) {
    return walk.result();
  }
  if (loader.fix_mismatches) {
    loader.fix_mismatches();
  }
  return {};
}

SubtreeLoader command_subtree_loader(cmd::CommandContext& ctx, editor::EditorState& editor) {
  auto session = std::make_shared<cell::detail::IoSession>(ctx, editor);
  SubtreeLoader loader;
  loader.read = [session](db::CellDef& def) {
    cell::ReadOptions options;
    options.ignore_tech = true;  // DBCellRead(def, TRUE, TRUE, NULL)
    return cell::read_cell(session->io, def, options).ok() || def.flags().available;
  };
  loader.fix_mismatches = [session] { session->process_mismatches(); };
  return loader;
}

}  // namespace hefesto::gds
