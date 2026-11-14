// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The engine's check context, kernel seam and hierarchy search primitives.
// Ported from Magic 8.3.694 database/DBcellsrch.c (DBCellSrArea,
// dbCellSrFunc, DBArraySr, DBSrCellPlaneArea) and database/DBcellbox.c
// (DBArrayOverlap, DBComputeArrayArea); the engine-generated rules follow
// drc/DRCsubcell.c and drc/DRCarray.c (drcSubcellCookie, drcInSubCookie,
// drcArrayCookie, drcOffGridCookie). Magic is (c) the Regents of the
// University of California, distributed under its permissive license
// (THIRD_PARTY_NOTICES.md).
//
// Owner: ENGINE (openspec/changes/m3-drc/design.md D2).

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>

#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/use_index.hpp"
#include "hefesto/drc/engine/kernel_hook.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "internal.hpp"

namespace hefesto::drc {

namespace {

std::atomic<KernelFunction> g_kernel{nullptr};

}  // namespace

ScopedKernel::ScopedKernel(KernelFunction kernel) noexcept : previous_(g_kernel.exchange(kernel)) {}

ScopedKernel::~ScopedKernel() {
  g_kernel.store(previous_);
}

std::int32_t current_halo(const editor::EditorState& editor) {
  const std::int32_t style = editor.drc.rules->halo().value;
  return editor.drc.halo_override ? std::min(*editor.drc.halo_override, style) : style;
}

std::int32_t current_step_size(const editor::EditorState& editor) {
  const std::int32_t step = editor.drc.step_size.value_or(editor.drc.rules->step_size());
  // `drc *stepsize` never stores a value <= 0 (Q-DRC-24), and a grid rescale
  // or the default step of a style without rules never computes one
  // (Q-DRC-25): the checker divides by the step size.
  assert(step > 0);
  return step;
}

namespace detail {

KernelFn kernel() noexcept {
  const KernelFunction installed = g_kernel.load();
  return installed != nullptr ? installed : &basic_check;
}

const Rule& engine_rule(WhyIndex why) noexcept {
  static const auto rules = [] {
    std::array<Rule, why::kFirstUser> out{};
    for (WhyIndex w = 0; w < why::kFirstUser; ++w) {
      out[w].why = w;
    }
    return out;
  }();
  return rules[why < rules.size() ? why : 0];
}

Context::Context(const editor::EditorState& editor, ErrorSink& error_sink, const CancelToken& cancel)
    : technology(editor.technology),
      rules(editor.drc.rules),
      halo(current_halo(editor)),
      step(current_step_size(editor)),
      sink(error_sink),
      yank("__DRC__yank", editor.technology->plane_count()) {
  yank.flags().internal = true;
  yank.flags().available = true;
  input.technology = technology.get();
  input.rules = rules.get();
  input.cif = &editor.drc.cif;
  input.euclidean = editor.drc.euclidean;
  input.compat = editor.drc.compat_settings();
  input.cancel = cancel;
  if (sink.statistics() == nullptr) {
    sink.set_statistics(&own_stats);
  }
  stats = sink.statistics();
}

Context::~Context() {
  if (sink.statistics() == &own_stats) {
    sink.set_statistics(nullptr);
  }
}

bool Context::cancel_requested() {
  cancelled = cancelled || input.cancel.requested();
  return cancelled;
}

bool Context::basic(const db::CellDef& cell, const db::Rect& check_area, const db::Rect& clip_area) {
  if (cancelled) {
    return false;
  }
  CheckInput in = input;
  in.cell = &cell;
  if (kernel()(in, check_area, clip_area, sink) == CheckOutcome::cancelled) {
    cancelled = true;
  }
  return !cancelled;
}

void Context::clear_yank() {
  const std::size_t planes = yank.plane_count();
  yank.set_plane_count(0);
  yank.set_plane_count(planes);
  yank.properties().clear();
}

// --- Hierarchy search -------------------------------------------------------

IndexRange array_overlap(const db::CellUse& use, const db::Rect& parent_rect) {
  const db::ArrayInfo& a = use.array;
  if (!a.is_array()) {
    return IndexRange{a.xlo, a.xlo, a.ylo, a.ylo};
  }
  db::Rect box = use.def->bbox();
  db::Rect child = db::apply(db::invert(use.transform), parent_rect);
  db::Coord xsep = a.xsep;
  db::Coord ysep = a.ysep;
  const std::int32_t xlo = std::min(a.xlo, a.xhi);
  const std::int32_t xhi = std::max(a.xlo, a.xhi);
  const std::int32_t ylo = std::min(a.ylo, a.yhi);
  const std::int32_t yhi = std::max(a.ylo, a.yhi);
  if (xsep < 0) {  // flip about the y axis
    xsep = -xsep;
    child = db::Rect{-child.x1, child.y0, -child.x0, child.y1};
    box = db::Rect{-box.x1, box.y0, -box.x0, box.y1};
  }
  if (ysep < 0) {
    ysep = -ysep;
    child = db::Rect{child.x0, -child.y1, child.x1, -child.y0};
    box = db::Rect{box.x0, -box.y1, box.x1, -box.y0};
  }
  IndexRange out{xlo, xhi, ylo, yhi};
  if (xsep != 0) {
    out.xlo = xlo + (child.x0 - box.x1 + xsep - 1) / xsep;
    out.xhi = xlo + (child.x1 - box.x0) / xsep;
  }
  if (ysep != 0) {
    out.ylo = ylo + (child.y0 - box.y1 + ysep - 1) / ysep;
    out.yhi = ylo + (child.y1 - box.y0) / ysep;
  }
  out.xlo = std::max(out.xlo, xlo);
  out.xhi = std::min(out.xhi, xhi);
  out.ylo = std::max(out.ylo, ylo);
  out.yhi = std::min(out.yhi, yhi);
  if (a.xlo > a.xhi) {  // back to the use's own index direction
    out = IndexRange{a.xhi + a.xlo - out.xhi, a.xhi + a.xlo - out.xlo, out.ylo, out.yhi};
  }
  if (a.ylo > a.yhi) {
    out = IndexRange{out.xlo, out.xhi, a.yhi + a.ylo - out.yhi, a.yhi + a.ylo - out.ylo};
  }
  return out;
}

db::Rect array_area(const db::Rect& area, const db::CellUse& use, std::int32_t x, std::int32_t y) {
  const db::ArrayInfo& a = use.array;
  const std::int32_t dx = a.xlo > a.xhi ? a.xlo - x : x - a.xlo;
  const std::int32_t dy = a.ylo > a.yhi ? a.ylo - y : y - a.ylo;
  const db::Coord xd = a.xsep * dx;
  const db::Coord yd = a.ysep * dy;
  return db::Rect{area.x0 + xd, area.y0 + yd, area.x1 + xd, area.y1 + yd};
}

std::vector<const db::CellUse*> uses_overlapping(const db::CellDef& def, const db::Rect& area) {
  // DBTreeSrCells / DBCellSrArea: a BPE_OVERLAP search of the use index,
  // which may bin the cell's uses as Magic's bplane does.
  std::vector<const db::CellUse*> out;
  (void)db::uses_overlapping(def, area, [&](db::CellUse& u) {
    if (u.def != nullptr) {
      out.push_back(&u);
    }
    return true;
  });
  return out;
}

namespace {

// dbCellSrFunc for one use; false when the search must stop.
bool search_use(const db::CellUse& use, const db::Rect& area,
                const std::function<Visit(const Element&, const db::Rect&)>& visit) {
  const IndexRange r = array_overlap(use, area);
  for (std::int32_t y = r.ylo; y <= r.yhi; ++y) {
    for (std::int32_t x = r.xlo; x <= r.xhi; ++x) {
      const db::Transform t = db::element_transform(use, x, y);
      const Visit v = visit(Element{&use, x, y, t}, db::apply(db::invert(t), area));
      if (v == Visit::stop) {
        return false;
      }
      if (v == Visit::skip_array) {
        return true;
      }
    }
  }
  return true;
}

}  // namespace

bool search_elements(const db::CellDef& def, const db::Rect& area,
                     const std::function<Visit(const Element&, const db::Rect&)>& visit) {
  for (const db::CellUse* u : uses_overlapping(def, area)) {
    if (!search_use(*u, area, visit)) {
      return false;
    }
  }
  return true;
}

void array_elements(const db::CellUse& use, const db::Rect& area, const std::function<void(const Element&)>& visit) {
  const IndexRange r = array_overlap(use, area);
  for (std::int32_t y = r.ylo; y <= r.yhi; ++y) {
    for (std::int32_t x = r.xlo; x <= r.xhi; ++x) {
      visit(Element{&use, x, y, db::element_transform(use, x, y)});
    }
  }
}

}  // namespace detail

}  // namespace hefesto::drc
