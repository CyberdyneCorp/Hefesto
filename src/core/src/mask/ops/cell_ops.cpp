// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Operator group 4, the operators that read the cell (capability
// mask-output-operators "net", "tagged", "bbox", "boundary", "mask-hints";
// design D6, Q-MASK-6, 22, 23): CIFOP_NET, CIFOP_TAGGED, CIFOP_BBOX,
// CIFOP_BOUNDARY, CIFOP_MASKHINTS (and CIFOP_COPYUP, which output skips).
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFgen.c (CIFGenLayer's
// cases of these operators, cifSrTiles2, cifCopyPropPlaneFunc) and
// commands/CmdFI.c (CmdFindNetProc, label names only), Copyright (C) 1985,
// 1990 Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: OPS.

#include <algorithm>
#include <string>

#include "hefesto/connect/search.hpp"
#include "hefesto/hints/hints.hpp"
#include "mask/ops/bloat_all.hpp"

namespace hefesto::mask {

namespace {

/// The longest hint name whose property key (`MASKHINTS_` + name) fits
/// Magic's 512-byte buffer.
constexpr std::size_t kMaxHintName = 511 - 10;

/// Collects the net's tiles of the operator's types (DBTreeCopyConnect into
/// a scratch cell, then cifSrTiles over it).
class NetPainter final : public connect::NetVisitor {
 public:
  NetPainter(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) : args_(args), ctx_(ctx), out_(out) {}

  bool on_tile(const connect::NetTile& tile) override {
    const db::TileView& t = tile.tile;
    if (args_.op.types.test(t.type) && db::overlap(t.area, args_.area)) {
      detail::paint_shape(out_, t, detail::scale_rect(t.area, detail::scale_of(args_)), ctx_.compat.magic_compat);
      detail::count_tile(ctx_);
    }
    return true;
  }

 private:
  const OpArgs& args_;
  OperatorContext& ctx_;
  MaskPlane& out_;
};

const db::Label* find_label(const db::CellDef& cell, const std::string& text) {
  const auto& labels = cell.labels();
  const auto it = std::find_if(labels.begin(), labels.end(), [&](const db::Label& l) { return l.text == text; });
  return it == labels.end() ? nullptr : &*it;
}

/// The label rectangle of `tagged`, with degenerate sides widened by 1.
db::Rect tag_area(const db::Label& l) {
  db::Rect r = l.rect;
  if (r.x0 == r.x1) {
    --r.x0;
    ++r.x1;
  }
  if (r.y0 == r.y1) {
    --r.y0;
    ++r.y1;
  }
  return r;
}

/// The lowest technology plane holding any of `types` (DBTechTypesToPlanes).
int first_plane_of(const tech::Technology* tech, const tech::TypeMask& types) {
  if (tech == nullptr) {
    return tech::kFirstUserPlane;
  }
  tech::PlaneMask planes = 0;
  for (std::size_t t = 0; t < tech->type_count(); ++t) {
    if (types.test(static_cast<tech::TileType>(t))) {
      planes |= tech->type_planes(static_cast<tech::TileType>(t));
    }
  }
  for (std::size_t p = tech::kFirstUserPlane; p < tech->plane_count(); ++p) {
    if ((planes & tech::plane_bit(static_cast<tech::PlaneId>(p))) != 0) {
      return static_cast<int>(p);
    }
  }
  return static_cast<int>(tech->plane_count());
}

/// The BloatData that CIFOP_TAGGED builds on the fly.
detail::BloatAllConfig tagged_config(const OpArgs& args, const MaskPlane* current) {
  detail::BloatAllConfig cfg;
  cfg.trigger_types = args.op.types;
  if (!args.op.cif_layers.empty()) {
    cfg.connect = detail::solid_mask();
    cfg.layers = detail::layer_sources(args.inputs, args.op.cif_layers);
  } else if (!args.op.types.empty()) {
    cfg.bloat_plane = first_plane_of(args.inputs.technology, args.op.types);
    cfg.connect = args.op.types;
  } else {
    cfg.connect = detail::solid_mask();
    cfg.layers = {current};
  }
  return cfg;
}

}  // namespace

diag::Status op_net(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  const db::CellDef* cell = ctx.cell != nullptr ? ctx.cell : args.inputs.layout;
  const tech::Technology* tech = args.inputs.technology;
  if (cell == nullptr || tech == nullptr) {
    return diag::Done{};
  }
  // Q-MASK-22 (replicate): without layers the operator adds nothing.
  const db::Label* label = find_label(*cell, args.op.text);
  if (label != nullptr && label->type != tech::kSpace && !args.op.types.empty()) {
    connect::SearchStart start;
    start.area = label->rect;
    start.types = tech->connects_to(label->type);
    connect::SearchOptions options;
    options.magic_compat = ctx.compat.magic_compat;
    NetPainter painter{args, ctx, out};
    (void)connect::search_hierarchical(*cell, *tech, start, options, painter);
  }
  // Mask layers in the list contribute their whole plane.
  for (const MaskPlane* m : detail::layer_sources(args.inputs, args.op.cif_layers)) {
    (void)out.unite(*m);
  }
  return diag::Done{};
}

diag::Status op_tagged(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  const db::CellDef* cell = args.inputs.layout;
  if (cell == nullptr) {
    return diag::Done{};
  }
  const bool on_current = args.op.types.empty() && args.op.cif_layers.empty();
  ScratchPlane next{*ctx.pool};
  MaskPlane& into = on_current ? *next : out;
  const detail::BloatAllConfig cfg = tagged_config(args, &out);
  detail::BloatAll engine{args, ctx, cfg, into};
  const std::int32_t scale = detail::scale_of(args);
  for (const db::Label& l : cell->labels()) {
    if (l.text != args.op.text) {
      continue;
    }
    const db::Rect area = q_mask_6_label_area(tag_area(l), scale, ctx.compat);
    detail::search_op_tiles2(args, ctx, area, &out, [&](const db::TileView& t, const detail::Source& s) {
      engine.trigger(t, s, s.layout ? scale : 1);
      return false;
    });
  }
  engine.finish();
  if (on_current) {
    out.swap(*next);
  }
  return diag::Done{};
}

diag::Status op_bbox(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  const db::CellDef* cell = ctx.cell;
  if (cell == nullptr) {
    return diag::Done{};
  }
  if (args.op.text == "top") {
    const auto& uses = cell->instances();
    const bool placed = std::any_of(uses.begin(), uses.end(), [](const db::CellUse* u) {
      return u->parent != nullptr && !u->parent->flags().internal;
    });
    if (placed) {
      return diag::Done{};
    }
  }
  // Q-MASK-23 (replicate): paint, subcell boxes and label anchors.
  detail::paint(out, detail::scale_rect(cell->bbox(), detail::scale_of(args)));
  return diag::Done{};
}

diag::Status op_boundary(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  const db::CellDef* cell = ctx.cell;
  if (cell == nullptr || !cell->flags().fixed_bbox) {
    return diag::Done{};
  }
  const db::PropertyValue* value = cell->properties().find("FIXED_BBOX");
  const auto* dims = value != nullptr ? std::get_if<db::DimensionList>(value) : nullptr;
  if (dims == nullptr || dims->values.size() != 4) {
    return diag::Done{};  // Q-MASK-24: unspecified in Magic; nothing here
  }
  const db::Rect r{dims->values[0], dims->values[1], dims->values[2], dims->values[3]};
  detail::paint(out, detail::scale_rect(r, detail::scale_of(args)));
  return diag::Done{};
}

diag::Status op_mask_hints(const OpArgs& args, OperatorContext& /*ctx*/, MaskPlane& out) {
  if (args.inputs.layout == nullptr) {
    return diag::Done{};
  }
  const std::string name = args.op.text.substr(0, kMaxHintName);
  const hints::HintRegion region = hints::output_region(*args.inputs.layout, name, detail::scale_of(args));
  for (const db::Rect& r : region.strips()) {
    detail::paint(out, r);
  }
  return diag::Done{};
}

diag::Status op_copyup(const OpArgs& /*args*/, OperatorContext& /*ctx*/, MaskPlane& /*out*/) { return diag::Done{}; }

}  // namespace hefesto::mask
