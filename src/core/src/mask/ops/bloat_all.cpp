// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The bloat-all flood engine and CIFOP_BLOATALL (capability
// mask-output-operators "bloat-all"; design D6, Q-MASK-230, 231, 232).
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFgen.c (CIFGenLayer's
// CIFOP_BLOATALL case, cifBloatAllFunc, cifFoundFunc, cifProcessResetFunc)
// and drc/DRCtech.c (CoincidentPlanes), Copyright (C) 1985, 1990 Regents of
// the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: OPS.

#include "mask/ops/bloat_all.hpp"

#include <algorithm>

namespace hefesto::mask {

namespace detail {

namespace {

bool split_dir(const db::TileView& t) noexcept { return t.split && t.diagonal == db::Diagonal::nw_se; }

db::Rect intersect(const db::Rect& a, const db::Rect& b) noexcept {
  return db::Rect{std::max(a.x0, b.x0), std::max(a.y0, b.y0), std::min(a.x1, b.x1), std::min(a.y1, b.y1)};
}

}  // namespace

BloatAll::BloatAll(const OpArgs& args, OperatorContext& ctx, const BloatAllConfig& cfg, MaskPlane& out)
    : args_(args), ctx_(ctx), cfg_(cfg), out_(out) {
  for (const auto& [tile, entry] : ctx.stale_marks.tiles) {
    where_[tile] = entry;
    db::TileView v;
    v.tile = tile;
    marks_.set(v, Mark::processed);
  }
}

Mark BloatAll::mark(const db::TileView& t) const { return marks_.get(t); }

void BloatAll::set_mark(const db::TileView& t, const db::Plane* plane, Mark m) {
  marks_.set(t, m);
  if (m == Mark::unprocessed) {
    where_.erase(t.tile);
  } else {
    where_[t.tile] = StaleMarks::Entry{plane, left_type(t), right_type(t)};
  }
}

void BloatAll::push(const db::TileView& t, bool side, const db::Plane* plane) {
  if (mark(t) == Mark::unprocessed) {
    set_mark(t, plane, Mark::pending);
    stack_.push_back(Pending{t, side, plane});
  }
}

const db::Plane* BloatAll::bloat_plane() const {
  const db::CellDef* layout = args_.inputs.layout;
  if (layout == nullptr || cfg_.bloat_plane < 0 ||
      static_cast<std::size_t>(cfg_.bloat_plane) >= layout->plane_count()) {
    return nullptr;
  }
  return &layout->plane(static_cast<tech::PlaneId>(cfg_.bloat_plane));
}

tech::PlaneMask BloatAll::coincident_planes(tech::TileType type) const {
  const tech::Technology* tech = args_.inputs.technology;
  if (tech == nullptr || cfg_.bloat_plane < 0) {
    return 0;
  }
  tech::PlaneMask planes = 0;
  if (tech->is_contact(type)) {
    planes = tech->type_planes(type);
  } else if (const auto home = tech->home_plane(type)) {
    planes = tech::plane_bit(*home);
  }
  for (std::size_t i = tech::kMagnet; i < tech->type_count(); ++i) {
    if (cfg_.connect.test(static_cast<tech::TileType>(i))) {
      planes &= tech->type_planes(static_cast<tech::TileType>(i));
    }
  }
  return planes;
}

void BloatAll::seed(const db::Plane& plane, const db::Rect& area, const tech::TypeMask& mask) {
  const bool all = q_mask_231_seed_all(ctx_.compat);
  plane.enumerate(area, mask, [&](const db::TileView& t) {
    if (mark(t) != Mark::unprocessed) {
      return true;
    }
    push(t, t.right_side, &plane);
    return all;
  });
}

void BloatAll::trigger(const db::TileView& t, const Source& source, std::int32_t scale) {
  const std::int32_t style_scale = scale_of(args_);
  db::Rect area = t.area;
  db::Rect clip;
  std::int32_t loc_scale = 1;
  tech::PlaneMask pmask = 0;
  if (!source.layout) {
    loc_scale = cfg_.bloat_plane < 0 ? 1 : style_scale;
    area = q_mask_232_to_internal(t.area, loc_scale, ctx_.compat);
    clip = area;
  } else {
    pmask = coincident_planes(t.type);
    if (cfg_.bloat_plane < 0) {
      area = scale_rect(t.area, scale);
      clip = area;
    } else {
      loc_scale = scale;
      clip = t.area;
    }
  }
  if (pmask != 0) {
    push(t, t.right_side, source.plane);
  } else if (cfg_.bloat_plane < 0) {
    for (const MaskPlane* layer : cfg_.layers) {
      seed(layer->plane(), area, solid_mask());
    }
  } else if (const db::Plane* p2 = bloat_plane()) {
    seed(*p2, area, cfg_.connect);
  }
  if (cfg_.distance > 0) {
    clip = db::expand(scale_rect(clip, loc_scale), cfg_.distance);
  }
  flood(loc_scale, clip);
  count_tile(ctx_);
}

void BloatAll::paint_tile(const Pending& p, const db::Rect& a, const db::Rect& clipped) {
  if (!p.tile.split) {
    paint(out_, cfg_.distance > 0 ? clipped : a);
    return;
  }
  const bool whole =
      (p.side && cfg_.connect.test(p.tile.left_type)) || (!p.side && cfg_.connect.test(p.tile.right_type));
  if (whole) {
    paint(out_, a);
    return;
  }
  db::TileView side = p.tile;
  side.right_side = p.side;
  paint_shape(out_, side, a, ctx_.compat.magic_compat);
}

void BloatAll::push_neighbors(const Pending& p, const db::Rect& a, const db::Rect& clip) {
  const bool unclipped = cfg_.distance == 0;
  const bool dir = split_dir(p.tile);
  const db::Plane& plane = *p.plane;
  if (unclipped || a.y1 < clip.y1) {
    const std::vector<db::TileView> n = side_tiles(plane, p.tile, db::Side::top);
    for (auto u = n.rbegin(); u != n.rend(); ++u) {
      if (cfg_.connect.test(bottom_type(*u))) {
        push(*u, split_dir(*u) != dir, p.plane);
      }
    }
  }
  if (unclipped || a.x0 > clip.x0) {
    for (const db::TileView& u : side_tiles(plane, p.tile, db::Side::left)) {
      if (cfg_.connect.test(right_type(u))) {
        push(u, true, p.plane);
      }
    }
  }
  if (unclipped || a.y0 > clip.y0) {
    for (const db::TileView& u : side_tiles(plane, p.tile, db::Side::bottom)) {
      if (cfg_.connect.test(top_type(u))) {
        push(u, split_dir(u) == dir, p.plane);
      }
    }
  }
  if (unclipped || a.x1 < clip.x1) {
    const std::vector<db::TileView> n = side_tiles(plane, p.tile, db::Side::right);
    for (auto u = n.rbegin(); u != n.rend(); ++u) {
      if (cfg_.connect.test(left_type(*u))) {
        push(*u, false, p.plane);
      }
    }
  }
}

void BloatAll::flood(std::int32_t loc_scale, const db::Rect& clip) {
  while (!stack_.empty()) {
    const Pending p = stack_.back();
    stack_.pop_back();
    if (mark(p.tile) != Mark::pending) {
      continue;
    }
    set_mark(p.tile, p.plane, Mark::processed);
    const db::Rect a = scale_rect(p.tile.area, loc_scale);
    db::Rect clipped = a;
    if (cfg_.distance > 0) {
      reset_.push_back(p.tile);
      clipped = intersect(a, clip);
      if (clipped.x0 >= clipped.x1 || clipped.y0 >= clipped.y1) {
        continue;
      }
    }
    paint_tile(p, a, clipped);
    push_neighbors(p, a, clip);
  }
  for (const db::TileView& t : reset_) {
    set_mark(t, nullptr, Mark::unprocessed);
  }
  reset_.clear();
}

void BloatAll::finish() {
  if (q_mask_230_reset_all(ctx_.compat)) {
    marks_.clear();
    where_.clear();
    ctx_.stale_marks.clear();
    return;
  }
  // Magic's resets: every solid tile of the bloated mask layers, or the
  // tiles of the connecting and trigger types on the bloat plane.
  std::vector<const db::Plane*> planes;
  tech::TypeMask mask = cfg_.connect | cfg_.trigger_types;
  if (cfg_.bloat_plane < 0) {
    for (const MaskPlane* layer : cfg_.layers) {
      planes.push_back(&layer->plane());
    }
    mask = solid_mask();
  } else if (const db::Plane* p2 = bloat_plane()) {
    planes.push_back(p2);
  }
  ctx_.stale_marks.clear();
  for (const auto& [tile, entry] : where_) {
    const bool on_reset_plane = std::find(planes.begin(), planes.end(), entry.plane) != planes.end();
    if (!on_reset_plane || !(mask.test(entry.left) || mask.test(entry.right))) {
      ctx_.stale_marks.tiles[tile] = entry;
    }
  }
  marks_.clear();
  where_.clear();
}

}  // namespace detail

diag::Status op_bloat_all(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  detail::BloatAllConfig cfg;
  cfg.bloat_plane = detail::value32(args.op, 1, -1);
  cfg.distance = detail::value32(args.op, 0);
  cfg.trigger_types = args.op.types;
  const std::size_t n = args.op.values.size() > 2 ? args.op.values.size() - 2 : 0;
  std::vector<std::size_t> bloated;
  for (std::size_t i = 0; i < n; ++i) {
    if (args.op.values[2 + i] != 0) {
      bloated.push_back(i);
    }
  }
  if (cfg.bloat_plane < 0) {
    cfg.connect = detail::solid_mask();
    std::vector<std::size_t> positive;
    std::copy_if(bloated.begin(), bloated.end(), std::back_inserter(positive),
                 [&](std::size_t i) { return args.op.values[2 + i] > 0; });
    cfg.layers = detail::layer_sources(args.inputs, positive);
  } else {
    for (const std::size_t t : bloated) {
      cfg.connect.set(static_cast<tech::TileType>(t));
    }
  }
  detail::BloatAll engine{args, ctx, cfg, out};
  // The triggers in cifSrTiles' order: layout planes, then mask layers.
  for (const detail::Source& s : detail::layout_sources(args.inputs, args.op.types)) {
    s.plane->enumerate(args.area, args.op.types, [&](const db::TileView& t) {
      engine.trigger(t, s, detail::scale_of(args));
      return true;
    });
  }
  for (const MaskPlane* m : detail::layer_sources(args.inputs, args.op.cif_layers)) {
    const detail::Source src{&m->plane(), false, 0};
    m->plane().enumerate(db::whole_plane(), detail::solid_mask(), [&](const db::TileView& t) {
      engine.trigger(t, src, 1);
      return true;
    });
  }
  engine.finish();
  return diag::Done{};
}

}  // namespace hefesto::mask
