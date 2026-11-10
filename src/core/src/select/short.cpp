// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `select short NAME1 NAME2` (cmd-selection "Selection utility options"):
// the net of the first label is selected, then replaced by the path with
// the fewest tile-to-tile steps between the tiles under the two labels
// (a breadth-first search over the selected tiles). Owner: SELECT.
// (Magic ref: select/selOps.c:413-1050 SelectShort)

#include <deque>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "hefesto/db/labels.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/select/diag_codes.hpp"
#include "select/commands_internal.hpp"
#include "select/ops.hpp"

namespace hefesto::select {

namespace cu = db::cmdutil;
using cmd::CommandResult;
using ops::Context;

namespace {

struct Node {
  tech::PlaneId plane = 0;
  db::TileView tile;
};

using Key = std::tuple<tech::PlaneId, db::Coord, db::Coord, db::Coord, db::Coord>;

Key key_of(const Node& n) {
  return {n.plane, n.tile.area.x0, n.tile.area.y0, n.tile.area.x1, n.tile.area.y1};
}

/// The selected tile under a label that connects to its type.
std::optional<Node> tile_under(const Context& c, const db::Label& label) {
  const db::CellDef& sel = *c.ed->selection.def();
  const db::Rect at{label.rect.x0, label.rect.y0, label.rect.x0 + 1, label.rect.y0 + 1};
  std::optional<Node> found;
  for (std::size_t p = tech::kFirstUserPlane; p < sel.plane_count() && !found; ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    if ((c.tech->type_planes(label.type) & tech::plane_bit(plane)) == 0) {
      continue;
    }
    sel.plane(plane).enumerate(at, c.tech->connects_to(label.type), [&](const db::TileView& t) {
      found = Node{plane, t};
      return false;
    });
  }
  return found;
}

/// Tiles edge-adjacent to `n` on its plane and the contact images on the
/// contact's other planes, of types connecting to it.
std::vector<Node> neighbours(const Context& c, const Node& n) {
  std::vector<Node> out;
  const db::CellDef& sel = *c.ed->selection.def();
  const tech::TypeMask mask = c.tech->connects_to(n.tile.type);
  const db::Rect r = n.tile.area;
  for (const db::Rect strip : {db::Rect{r.x0 - 1, r.y0, r.x0, r.y1}, db::Rect{r.x0, r.y0 - 1, r.x1, r.y0},
                               db::Rect{r.x1, r.y0, r.x1 + 1, r.y1}, db::Rect{r.x0, r.y1, r.x1, r.y1 + 1}}) {
    sel.plane(n.plane).enumerate(strip, mask, [&](const db::TileView& t) {
      out.push_back(Node{n.plane, t});
      return true;
    });
  }
  const tech::PlaneMask planes = c.tech->contact_planes(n.tile.type) & ~tech::plane_bit(n.plane);
  for (std::size_t p = tech::kFirstUserPlane; p < sel.plane_count(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    if ((planes & tech::plane_bit(plane)) != 0) {
      sel.plane(plane).enumerate(r, mask, [&](const db::TileView& t) {
        out.push_back(Node{plane, t});
        return true;
      });
    }
  }
  return out;
}

/// Breadth-first search from `from` to `to`; the path, `to` first.
std::vector<Node> shortest_path(const Context& c, const Node& from, const Node& to) {
  std::map<Key, Node> parent;
  std::deque<Node> queue{from};
  parent.emplace(key_of(from), from);
  while (!queue.empty()) {
    const Node n = queue.front();
    queue.pop_front();
    if (key_of(n) == key_of(to)) {
      std::vector<Node> path{n};
      for (Key k = key_of(n); k != key_of(from); k = key_of(path.back())) {
        path.push_back(parent.at(k));
      }
      return path;
    }
    for (const Node& next : neighbours(c, n)) {
      if (parent.emplace(key_of(next), n).second) {
        queue.push_back(next);
      }
    }
  }
  return {};
}

const db::Label* find_label(const db::CellDef& def, const std::string& glob) {
  for (const db::Label& l : def.labels()) {
    if (db::glob_match(glob, l.text)) {
      return &l;
    }
  }
  return nullptr;
}

}  // namespace

CommandResult select_short(const Context& c, const std::string& name1, const std::string& name2) {
  Snapshot before = ops::begin(c);
  c.ed->selection.clear();
  // CmdFindNetProc: the first label matching NAME1 in the hierarchy.
  std::optional<db::Label> start;
  (void)connect::search_tree_labels(*c.root, db::whole_plane(), tech::TypeMask::all(), {},
                                    [&](const connect::TreeLabel& l) {
                                      if (!db::glob_match(name1, l.label->text) || l.label->type == tech::kSpace) {
                                        return true;
                                      }
                                      start = connect::transform_label(*l.label, l.to_root, c.s.magic_compat);
                                      return false;
                                    });
  std::vector<Node> path;
  if (start) {
    const db::Rect at{start->rect.x0, start->rect.y0, start->rect.x0 + 1, start->rect.y0 + 1};
    (void)ops::select_net(c, at, start->type, false);
    const db::Label* src = find_label(*c.ed->selection.def(), name1);
    const db::Label* dst = find_label(*c.ed->selection.def(), name2);
    const auto from = src != nullptr ? tile_under(c, *src) : std::nullopt;
    const auto to = dst != nullptr ? tile_under(c, *dst) : std::nullopt;
    if (from && to) {
      path = shortest_path(c, *from, *to);
    }
  }
  if (path.empty()) {
    ops::commit(c, std::move(before));
    return fail_text(codes::kNoShortPath, "No shorting path found between source and destination!");
  }
  c.ed->selection.clear();
  db::CellDef& sel = *c.ed->selection.def();
  for (const Node& n : path) {
    (void)db::paint_type(sel, *c.tech, n.tile.area, n.tile.type);
  }
  ops::commit(c, std::move(before));
  return CommandResult::success();
}

}  // namespace hefesto::select
