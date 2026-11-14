// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Operator group 3, close (capability mask-output-operators "close";
// design D6, Q-MASK-2): CIFOP_CLOSE.
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFgen.c (CIFGenLayer's
// CIFOP_CLOSE case, cifCloseFunc, cifGatherFunc), Copyright (C) 1985, 1990
// Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: OPS.

#include "mask/ops/kernel.hpp"

namespace hefesto::mask {

namespace {

/// Magic's INFINITY, the area of a region that is not enclosed.
constexpr std::int64_t kUnbounded = db::kInfinity;

enum class Gather : std::uint8_t { search, fill, done };

bool split_dir(const db::TileView& t) noexcept { return t.split && t.diagonal == db::Diagonal::nw_se; }

bool at_plane_edge(const db::Rect& a) noexcept {
  return a.x0 <= db::kPlaneMin || a.y0 <= db::kPlaneMin || a.x1 >= db::kPlaneMax || a.y1 >= db::kPlaneMax;
}

class Closer {
 public:
  Closer(const OpArgs& args, OperatorContext& ctx, const db::Plane& from, MaskPlane& into)
      : ctx_(ctx), from_(from), into_(into) {
    threshold_.scaled = detail::value(args.op, 0);
    threshold_.declared = detail::value(args.op, 1, threshold_.scaled);
    threshold_.multiply = detail::value(args.op, 2, 1);
    threshold_.reduce = detail::value(args.op, 3, 1);
  }

  /// cifCloseFunc for space tile `t`.
  void close(const db::TileView& t) {
    if (marks_.get(t) != detail::Mark::unprocessed) {
      return;
    }
    std::int64_t total = 0;
    gather(t, Gather::search, total);
    const bool fill = total != kUnbounded && (below(total) || threshold_.scaled == 0);
    gather(t, fill ? Gather::fill : Gather::done, total);
  }

 private:
  struct Item {
    db::TileView tile;
    bool side = false;
  };

  [[nodiscard]] bool below(std::int64_t total) const { return q_mask_2_below(total, threshold_, ctx_.compat); }

  void accumulate(const db::TileView& t, std::int64_t& total) const {
    if (total == kUnbounded || !below(total)) {
      return;
    }
    std::int64_t area = t.area.width() * t.area.height();
    if (t.split) {
      area /= 2;
    }
    total = area > kUnbounded ? kUnbounded : total + area;
  }

  void fill_tile(const Item& item) {
    db::TileView side = item.tile;
    if (side.split) {
      side.right_side = side.left_type != tech::kSpace;
    }
    detail::paint_shape(into_, side, side.area, ctx_.compat.magic_compat);
    detail::count_tile(ctx_);
  }

  void push_neighbors(const Item& item, detail::Mark wanted, std::vector<Item>& stack) const {
    const db::Rect& a = item.tile.area;
    const bool dir = split_dir(item.tile);
    const auto take = [&](const db::TileView& u, tech::TileType facing, bool side) {
      if (marks_.get(u) == wanted && facing == tech::kSpace) {
        stack.push_back(Item{u, side});
      }
    };
    if (a.y1 != db::kPlaneMax) {
      const std::vector<db::TileView> n = detail::side_tiles(from_, item.tile, db::Side::top);
      for (auto u = n.rbegin(); u != n.rend(); ++u) {
        take(*u, detail::bottom_type(*u), dir != split_dir(*u));
      }
    }
    if (a.y0 != db::kPlaneMin) {
      for (const db::TileView& u : detail::side_tiles(from_, item.tile, db::Side::bottom)) {
        take(u, detail::top_type(u), dir == split_dir(u));
      }
    }
    if (a.x0 != db::kPlaneMin) {
      for (const db::TileView& u : detail::side_tiles(from_, item.tile, db::Side::left)) {
        take(u, detail::right_type(u), true);
      }
    }
    if (a.x1 != db::kPlaneMax) {
      const std::vector<db::TileView> n = detail::side_tiles(from_, item.tile, db::Side::right);
      for (auto u = n.rbegin(); u != n.rend(); ++u) {
        take(*u, detail::left_type(*u), false);
      }
    }
  }

  /// cifGatherFunc: walks the space region of `start` in `mode`.
  void gather(const db::TileView& start, Gather mode, std::int64_t& total) {
    const detail::Mark wanted = mode == Gather::search ? detail::Mark::unprocessed : detail::Mark::pending;
    std::vector<Item> stack{Item{start, start.right_side}};
    while (!stack.empty()) {
      const Item item = stack.back();
      stack.pop_back();
      if (marks_.get(item.tile) != wanted) {
        continue;
      }
      if (at_plane_edge(item.tile.area)) {
        total = kUnbounded;
      }
      if (mode == Gather::search) {
        accumulate(item.tile, total);
      } else if (mode == Gather::fill) {
        fill_tile(item);
      }
      marks_.set(item.tile, mode == Gather::search ? detail::Mark::pending : detail::Mark::processed);
      push_neighbors(item, wanted, stack);
    }
  }

  OperatorContext& ctx_;
  const db::Plane& from_;
  MaskPlane& into_;
  CloseThreshold threshold_;
  detail::Marks marks_;
};

}  // namespace

diag::Status op_close(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  ScratchPlane next{*ctx.pool};
  (void)next->unite(out);
  Closer closer{args, ctx, out.plane(), *next};
  out.plane().enumerate(db::whole_plane(), detail::space_mask(), [&](const db::TileView& t) {
    closer.close(t);
    return true;
  });
  out.swap(*next);
  return diag::Done{};
}

}  // namespace hefesto::mask
