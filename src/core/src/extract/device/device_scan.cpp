// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The perimeter scan of one device: terminals on the device plane, the gate
// length, the device perimeter, and record selection by terminals,
// substrate and identifier layers. Capability extract-devices "Terminal
// discovery on the device plane", "Device record selection";
// openspec/changes/m7-extraction/design.md D10.
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtBasic.c:3311-3720
// (extTransFindSubs, extTransFindId, extDevFindMatch, extTransTileFunc) and
// :3949-4210 (extTransPerimFunc), Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: EXTDEV.

#include <algorithm>
#include <string>
#include <vector>

#include "device_internal.hpp"

namespace hefesto::extract::device_detail {

namespace {

const tech::TypeMask kNoTypes;

// --- Context ---

}  // namespace

std::vector<GateLabel>& Context::gate_labels(NodeRef gate) {
  static thread_local std::vector<GateLabel> none;
  if (!gate || *gate >= in.nodes->size()) {
    none.clear();
    return none;
  }
  if (labels.size() < in.nodes->size()) {
    labels.resize(in.nodes->size());
  }
  std::optional<std::vector<GateLabel>>& slot = labels[*gate];
  if (!slot) {
    slot.emplace();
    for (const LabelRegion& l : (*in.nodes)[*gate].labels) {
      db::Rect rect{l.position.x, l.position.y, l.position.x, l.position.y};
      for (const db::Label& cl : cell.labels()) {
        if (cl.text == l.text && cl.rect.x0 == l.position.x && cl.rect.y0 == l.position.y) {
          rect = cl.rect;
          break;
        }
      }
      slot->push_back(GateLabel{l.text, rect, kNoAttr});
    }
  }
  return *slot;
}

tech::PlaneId Context::home(tech::TileType t) const {
  return tech.home_plane(t).value_or(tech::PlaneId{0});
}

std::string Context::type_name(tech::TileType t) const {
  return std::string(tech.type_name(t));
}

std::size_t Context::pattern_index(const DevicePattern* p) const {
  return static_cast<std::size_t>(p - style.devices.data());
}

void Context::message(std::string_view code, diag::Severity severity, std::string text) {
  diag::Diagnostic d = diag::make(code, std::move(text));
  d.severity = severity;
  out.messages.push_back(std::move(d));
}

void Context::bad(const db::TileView& tile, std::string text) {
  out.feedback.push_back(DeviceFeedback{tile.area, std::move(text)});
  ++out.warnings;
}

const tech::TypeMask& sd_types(const DevicePattern& p, std::size_t i) {
  return i < p.terminals.size() ? p.terminals[i] : kNoTypes;
}

const DevicePattern* next_record(const ExtStyle& style, const DevicePattern* p, tech::TileType type) {
  const std::vector<const DevicePattern*> list = style.records_for(type);
  const auto it = std::find(list.begin(), list.end(), p);
  if (it == list.end() || it + 1 == list.end()) {
    return nullptr;
  }
  return *(it + 1);
}

LabelRole label_role(std::string_view text) noexcept {
  if (text.empty()) {
    return LabelRole::name;
  }
  switch (text.back()) {
    case '@':
      return LabelRole::node_attr;
    case '^':
      return LabelRole::gate_attr;
    case '$':
      return LabelRole::term_attr;
    default:
      return LabelRole::name;
  }
}

namespace {

// extDevFindMatch's terminal comparison of one candidate.
bool terminals_agree(const TransRec& tr, const DevicePattern& cand, const DevicePattern& entry) {
  unsigned j = kMatchTerm;
  std::size_t i = 0;
  for (std::size_t k = 0; k < static_cast<std::size_t>(cand.terminal_count) && k < kMaxTerms; ++k) {
    if (!tr.terms[k].node) {
      break;
    }
    if ((tr.match & j) != 0 && (sd_types(cand, i).empty() || sd_types(cand, i) != sd_types(entry, i))) {
      return false;
    }
    j <<= 1U;
    if (!sd_types(cand, i + 1).empty()) {
      ++i;
    }
  }
  return true;
}

bool record_agrees(const TransRec& tr, const DevicePattern& cand, const DevicePattern& entry) {
  if (tr.match == 0) {
    return true;
  }
  if ((tr.match & kMatchId) != 0 && cand.identifiers != entry.identifiers) {
    return false;
  }
  if ((tr.match & kMatchSub) != 0 && cand.substrate_types != entry.substrate_types) {
    return false;
  }
  return terminals_agree(tr, cand, entry);
}

}  // namespace

const DevicePattern* find_match(const Context& ctx, const TransRec& tr, const DevicePattern* entry,
                                tech::TileType type) {
  const std::vector<const DevicePattern*> list = ctx.style.records_for(type);
  if (list.empty() || entry == nullptr) {
    return nullptr;
  }
  // The search starts after `entry` and wraps to the list head. Magic loops
  // forever when `entry` is not in the type's list (a region of two device
  // types); one full turn ends the search in both modes.
  const auto at = std::find(list.begin(), list.end(), entry);
  std::size_t i = (at == list.end() || at + 1 == list.end()) ? 0 : static_cast<std::size_t>(at - list.begin()) + 1;
  for (std::size_t steps = 0; steps < list.size(); ++steps) {
    const DevicePattern* cand = list[i];
    if (cand == entry) {
      return nullptr;
    }
    if (record_agrees(tr, *cand, *entry)) {
      return cand;
    }
    i = (i + 1) % list.size();
  }
  return nullptr;
}

namespace {

// --- extTransFindSubs / extTransFindId ---

bool enumerate_under(const db::Plane& plane, const db::TileView& tile, const db::Rect& area,
                     const tech::TypeMask& mask, const std::function<bool(const db::TileView&)>& visit) {
  if (tile.split) {
    return plane.enumerate_triangle(area, tile.diagonal, tile.right_side, mask, visit);
  }
  return plane.enumerate(area, mask, visit);
}

}  // namespace

// extTransFindSubs: the first node of a `mask` type under `tile` grown by 1.
Found find_subs(const Context& ctx, const db::TileView& tile, const tech::TypeMask& mask) {
  tech::TypeMask lmask = mask;
  lmask.clear_type(tech::kSpace);
  const db::Rect plus = db::expand(tile.area, 1);
  Found found;
  for (std::size_t p = tech::kFirstUserPlane; p < ctx.cell.plane_count(); ++p) {
    const auto pnum = static_cast<tech::PlaneId>(p);
    if (!ctx.tech.plane_types(pnum).intersects(lmask)) {
      continue;
    }
    const bool stopped = !enumerate_under(ctx.cell.plane(pnum), tile, plus, mask, [&](const db::TileView& t) {
      const NodeRef n = ctx.node(pnum, t);
      if (!n || (t.split && t.type == tech::kSpace)) {
        return true;
      }
      found = Found{n, t.type};
      return false;
    });
    if (stopped) {
      return found;
    }
  }
  return {};
}

namespace {

tech::TileType find_id(const Context& ctx, const db::TileView& tile, const tech::TypeMask& mask) {
  tech::TileType id = tech::kSpace;
  for (std::size_t p = tech::kFirstUserPlane; p < ctx.cell.plane_count(); ++p) {
    const auto pnum = static_cast<tech::PlaneId>(p);
    if (!ctx.tech.plane_types(pnum).intersects(mask)) {
      continue;
    }
    const bool stopped = !enumerate_under(ctx.cell.plane(pnum), tile, tile.area, mask, [&](const db::TileView& t) {
      id = t.type;
      return false;
    });
    if (stopped) {
      return id;
    }
  }
  return id;
}

// --- extTransPerimFunc ---

// The inside type and the outside view and type of a Manhattan boundary.
struct Sides {
  tech::TileType inside = tech::kSpace;
  tech::TileType outside = tech::kSpace;
  db::TileView out_view;
};

Sides boundary_sides(const Boundary& b) {
  Sides s;
  const db::TileView& o = b.outside;
  switch (b.dir) {
    case BoundaryDir::left:
      s.inside = left_type(b.inside);
      s.outside = right_type(o);
      s.out_view = side_view(o, true);
      break;
    case BoundaryDir::top:
      s.inside = top_type(b.inside);
      s.outside = bottom_type(o);
      s.out_view = side_view(o, !split_direction(o));
      break;
    case BoundaryDir::right:
      s.inside = right_type(b.inside);
      s.outside = left_type(o);
      s.out_view = side_view(o, false);
      break;
    default:
      s.inside = bottom_type(b.inside);
      s.outside = top_type(o);
      s.out_view = side_view(o, split_direction(o));
      break;
  }
  if (!b.inside.split) {
    s.inside = b.inside.type;
  }
  if (!o.split) {
    s.outside = o.type;
    s.out_view = o;
  }
  return s;
}

struct PerimScan {
  Context& ctx;
  TransRec& tr;

  void update_position(TermPos& pos, tech::TileType outside, const db::TileView& otile) const {
    const tech::PlaneId p = ctx.home(outside);
    if (p < pos.plane) {
      pos.plane = p;
      pos.pt = {otile.area.x0, otile.area.y0};
    } else if (p == pos.plane) {
      if (otile.area.x0 < pos.pt.x) {
        pos.pt = {otile.area.x0, otile.area.y0};
      } else if (otile.area.x0 == pos.pt.x && otile.area.y0 < pos.pt.y) {
        pos.pt.y = otile.area.y0;
      }
    }
  }

  void add_vector(Terminal& term, BoundaryDir dir, std::int64_t len) const {
    switch (dir) {
      case BoundaryDir::left:
        term.vector.y += static_cast<db::Coord>(len);
        break;
      case BoundaryDir::top:
        term.vector.x += static_cast<db::Coord>(len);
        break;
      case BoundaryDir::right:
        term.vector.y -= static_cast<db::Coord>(len);
        break;
      default:
        term.vector.x -= static_cast<db::Coord>(len);
        break;
    }
  }

  void bind_term_labels(const Boundary& b, std::size_t thisterm) const {
    for (GateLabel& l : ctx.gate_labels(tr.gate_node)) {
      if (l.attr == kNoAttr && db::encloses(b.segment, {l.rect.x0, l.rect.y0}) &&
          label_role(l.text) == LabelRole::term_attr) {
        l.attr = static_cast<int>(thisterm);
      }
    }
  }

  // The terminal index of the outside node for `devptr`, or nullopt when
  // that index holds another node (Magic's break).
  std::optional<std::size_t> claim(const DevicePattern& devptr, std::size_t i, const Boundary& b, const Sides& s,
                                   NodeRef node) {
    std::size_t thisterm = i;
    if (sd_types(devptr, 1).empty()) {
      for (thisterm = 0; thisterm < tr.nterm && thisterm < kMaxTerms; ++thisterm) {
        if (tr.terms[thisterm].node == node) {
          break;
        }
      }
    }
    if (thisterm >= kMaxTerms) {
      return std::nullopt;  // past MAXSD: Magic writes out of bounds; not a terminal in both modes
    }
    Terminal& term = tr.terms[thisterm];
    if (!term.node && !term.pos.tile) {
      ++tr.nterm;
      term = Terminal{};
      term.node = node;
      term.pos = TermPos{ctx.home(s.outside), b.plane, {b.outside.area.x0, b.outside.area.y0}, s.out_view, s.outside};
    } else if (term.node == node) {
      update_position(term.pos, s.outside, b.outside);
    } else {
      return std::nullopt;
    }
    return thisterm;
  }

  // One terminal segment for record `devptr` (mask i matched); returns true
  // when the segment was taken as a terminal (SDterm).
  bool take_terminal(const DevicePattern*& devptr, const DevicePattern* deventry, std::size_t i, const Boundary& b,
                     const Sides& s, NodeRef node) {
    const std::optional<std::size_t> claimed = claim(*devptr, i, b, s, node);
    if (!claimed) {
      return false;
    }
    const std::size_t thisterm = *claimed;
    Terminal& term = tr.terms[thisterm];
    term.len += b.length();
    add_vector(term, b.dir, b.length());
    bind_term_labels(b, thisterm);
    tr.match |= kMatchTerm << thisterm;
    if (thisterm >= static_cast<std::size_t>(devptr->terminal_count)) {
      devptr = find_match(ctx, tr, devptr, s.inside);
      if (devptr == deventry) {
        ctx.message("EXTRACT-DEV-002", diag::Severity::warning,
                    "Warning:  Device has more terminals than defined for type!");
      } else {
        tr.rec = devptr;
      }
    }
    return true;
  }

  // The terminal masks of `devptr` against the outside type: kTaken when a
  // terminal took the segment, kSpace when the outside is space.
  enum class Outcome : std::uint8_t { none, taken, space };
  Outcome try_masks(const DevicePattern*& devptr, const DevicePattern* deventry, const Boundary& b, const Sides& s,
                    NodeRef node) {
    for (std::size_t i = 0; !sd_types(*devptr, i).empty(); ++i) {
      if (s.outside == tech::kSpace) {
        return Outcome::space;
      }
      if (sd_types(*devptr, i).test(s.outside)) {
        return take_terminal(devptr, deventry, i, b, s, node) ? Outcome::taken : Outcome::none;
      }
    }
    return s.outside == tech::kSpace ? Outcome::space : Outcome::none;
  }

  // The records tried for one segment, from the current one: a terminal,
  // the gate, or the next candidate (Magic's while loop).
  void classify(const Boundary& b, const Sides& s, NodeRef node) {
    const std::vector<const DevicePattern*> list = ctx.style.records_for(s.inside);
    const DevicePattern* devptr = tr.rec != nullptr ? tr.rec : (list.empty() ? nullptr : list.front());
    const DevicePattern* deventry = devptr;
    while (devptr != nullptr) {
      tr.rec = devptr;
      const Outcome o = try_masks(devptr, deventry, b, s, node);
      if (o != Outcome::none) {
        break;
      }
      if (s.inside < ctx.style.node_conn.size() && ctx.style.node_conn[s.inside].test(s.outside)) {
        tr.gatelen += b.length();
        break;
      }
      devptr = find_match(ctx, tr, devptr, s.inside);
      if (devptr == deventry) {
        devptr = nullptr;
      }
    }
    if (devptr == nullptr) {
      tr.rec = deventry;
    }
  }

  void operator()(const Boundary& b) {
    if (!b.manhattan()) {
      return;  // non-Manhattan boundaries are not handled (Magic returns 0)
    }
    const Sides s = boundary_sides(b);
    classify(b, s, ctx.node(b.plane, s.out_view));
    const bool contact = ctx.tech.is_contact(s.outside) || ctx.tech.is_contact(s.inside);
    if (!contact || b.plane == tr.plane) {
      tr.perim += b.length();
    }
  }
};

// --- extTransTileFunc's record selection ---

tech::TypeMask union_of(const Context& ctx, tech::TileType type, tech::TypeMask DevicePattern::*field) {
  tech::TypeMask m;
  for (const DevicePattern* p : ctx.style.records_for(type)) {
    m |= p->*field;
  }
  return m;
}

// Switches to a record (after `devptr`) that `ok` accepts, setting `flag`;
// nullptr when the turn comes back to `devptr` or ends.
const DevicePattern* switch_record(const Context& ctx, TransRec& tr, const DevicePattern* devptr, tech::TileType t,
                                   unsigned flag, const std::function<bool(const DevicePattern&)>& ok) {
  const DevicePattern* entry = devptr;
  while (devptr != nullptr) {
    devptr = find_match(ctx, tr, devptr, t);
    if (devptr == nullptr || devptr == entry) {
      return nullptr;
    }
    if (ok(*devptr)) {
      tr.match |= flag;
      return devptr;
    }
  }
  return nullptr;
}

const DevicePattern* select_substrate(Context& ctx, TransRec& tr, const db::TileView& tile,
                                      const DevicePattern* devptr) {
  tech::TypeMask cmask = union_of(ctx, tile.type, &DevicePattern::substrate_types);
  if (cmask.empty() || tr.subs_set) {
    return devptr;
  }
  const bool allow_glob = cmask.test(tech::kSpace);
  cmask.clear_type(tech::kSpace);
  Found f = find_subs(ctx, tile, cmask);
  if (!f.node && allow_glob && ctx.in.substrate_node) {
    f.node = ctx.in.substrate_node;
  }
  tr.subs_node = f.node;
  tr.subs_set = f.node.has_value();
  if (f.node && !devptr->substrate_types.test(f.layer)) {
    const tech::TileType layer = f.layer;
    const DevicePattern* found = switch_record(ctx, tr, devptr, tile.type, kMatchSub,
                                               [&](const DevicePattern& p) { return p.substrate_types.test(layer); });
    if (found == nullptr) {
      ctx.message("EXTRACT-DEV-003", diag::Severity::error,
                  "No matching device for " + ctx.type_name(tile.type) + " with substrate layer " +
                      ctx.type_name(layer));
    }
    return found;
  }
  if (!f.node && (!ctx.style.substrate.types.empty() || devptr->substrate_node.empty())) {
    ctx.message("EXTRACT-DEV-004", diag::Severity::error,
                "Device " + ctx.type_name(tile.type) + " does not have a compatible substrate node!");
    return nullptr;
  }
  return devptr;
}

const DevicePattern* select_identifier(Context& ctx, TransRec& tr, const db::TileView& tile,
                                       const DevicePattern* devptr) {
  const tech::TypeMask cmask = union_of(ctx, tile.type, &DevicePattern::identifiers);
  if (cmask.empty()) {
    return devptr;
  }
  const tech::TileType id = find_id(ctx, tile, cmask);
  if (id == tech::kSpace && !devptr->identifiers.empty()) {
    const DevicePattern* found = switch_record(ctx, tr, devptr, tile.type, kMatchId,
                                               [](const DevicePattern& p) { return p.identifiers.empty(); });
    if (found == nullptr) {
      ctx.message("EXTRACT-DEV-005", diag::Severity::error,
                  "No matching device for " + ctx.type_name(tile.type) + " with no ID layer");
    }
    return found;
  }
  if (id != tech::kSpace && !devptr->identifiers.test(id)) {
    const DevicePattern* found = switch_record(ctx, tr, devptr, tile.type, kMatchId,
                                               [id](const DevicePattern& p) { return p.identifiers.test(id); });
    if (found == nullptr) {
      ctx.message("EXTRACT-DEV-006", diag::Severity::warning,
                  "ID layer " + ctx.type_name(id) + " on non-matching device " + ctx.type_name(tile.type) +
                      " was ignored.");
      return devptr;
    }
    return found;
  }
  tr.match |= kMatchId;
  return devptr;
}

void mark_gate_labels(Context& ctx, const TransRec& tr, const db::TileView& tile) {
  for (GateLabel& l : ctx.gate_labels(tr.gate_node)) {
    if (l.attr == kNoAttr && db::touch(tile.area, l.rect) && label_role(l.text) == LabelRole::gate_attr) {
      l.attr = kGateAttr;
    }
  }
}

void scan_tile(Context& ctx, TransRec& tr, tech::PlaneId pnum, const db::TileView& tile) {
  mark_gate_labels(ctx, tr, tile);
  const tech::TypeMask mask =
      tile.type < ctx.style.device_conn.size() ? ~ctx.style.device_conn[tile.type] : tech::TypeMask::all();
  PerimScan scan{ctx, tr};
  tr.perim += enum_tile_perim(ctx.cell.plane(pnum), pnum, tile, mask, [&](const Boundary& b) { scan(b); });
  const DevicePattern* devptr = tr.rec;
  if (devptr == nullptr) {
    return;
  }
  devptr = select_substrate(ctx, tr, tile, devptr);
  tr.rec = devptr;
  if (devptr == nullptr) {
    return;
  }
  tr.rec = select_identifier(ctx, tr, tile, devptr);
}

}  // namespace

void scan_tiles(Context& ctx, const DeviceRegionTiles& /*reg*/, const std::vector<PlaneTile>& tiles, TransRec& tr) {
  for (const auto& [pnum, tile] : tiles) {
    scan_tile(ctx, tr, pnum, tile);
  }
}

}  // namespace hefesto::extract::device_detail
