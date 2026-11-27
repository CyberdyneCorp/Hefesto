// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Path length extraction (length.hpp): extLength, extLengthYank,
// extLengthLabels, extPathLabel and the depth-first flood of
// extPathPairDistance, ported from Magic VLSI 8.3.694 (fc5daa9e)
// extract/ExtLength.c:147-1040 and database/DBlabel2.c:212-255
// (DBSrLabelLoc). Magic is Copyright (C) 1985, 1990 Regents of the
// University of California (permissive license, THIRD_PARTY_NOTICES.md).
//
// The flood is Magic's tile for tile (Q-EXTRACT-12): a visit marks its
// tile; the destination test comes first; then the neighbours across the
// top (right to left), right (top to bottom), bottom (left to right) and
// left (bottom to top), each entered at the midpoint of the shared
// boundary when it is still unmarked and connects; then, for a contact,
// the tile at the same lower-left point on each other plane of the contact
// (entered at the same point and distance when its slot is still
// CLIENTDEFAULT and it connects); then, for a type with connections on
// other planes, the unmarked connecting tiles over the tile's area grown by
// one. Each candidate is tested when the walk reaches it, after the floods
// of the earlier candidates, exactly as Magic's loops re-read the slot.
//
// Owner: EXTCMD (openspec/changes/m7-extraction/design.md D3).

#include "hefesto/extract/length/length.hpp"

#include <algorithm>
#include <utility>

#include "hefesto/cell/hier.hpp"
#include "hefesto/connect/search.hpp"
#include "hefesto/connect/tree.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extract/file/writer.hpp"
#include "hefesto/util/magic_hash_table.hpp"

namespace hefesto::extract {

namespace {

constexpr std::string_view kCodeInputs = "EXTRACT-CMD-001";
constexpr std::string_view kCodeMissing = "EXTRACT-LEN-001";
/// Magic's MARKED (ExtLength.c:116).
constexpr std::uintptr_t kMarked = 1;
/// extDriverHash's initial size (INITHASHSIZE, ExtLength.c:76).
constexpr std::size_t kDriverHashSize = 32;
/// The name of the scratch cell (DBNewYank("__PATHYANK__")).
constexpr const char* kScratchName = "__PATHYANK__";

/// TiGetType: the left type of a split tile.
tech::TileType type_of(const db::TileView& t) noexcept {
  return t.split ? t.left_type : t.type;
}

/// GEOCLIP: `r` clipped to `area` (may leave it inverted).
db::Rect clip(db::Rect r, const db::Rect& area) noexcept {
  r.x0 = std::max(r.x0, area.x0);
  r.y0 = std::max(r.y0, area.y0);
  r.x1 = std::min(r.x1, area.x1);
  r.y1 = std::min(r.y1, area.y1);
  return r;
}

/// The midpoint of a rectangle, C integer division.
db::Point center(const db::Rect& r) noexcept {
  return db::Point{static_cast<db::Coord>((std::int64_t{r.x0} + r.x1) / 2),
                   static_cast<db::Coord>((std::int64_t{r.y0} + r.y1) / 2)};
}

/// GeoClipPoint.
db::Point clip_point(db::Point p, const db::Rect& r) noexcept {
  p.x = std::clamp(p.x, r.x0, r.x1);
  p.y = std::clamp(p.y, r.y0, r.y1);
  return p;
}

std::int64_t absdiff(db::Coord a, db::Coord b) noexcept {
  return a > b ? std::int64_t{a} - b : std::int64_t{b} - a;
}

/// extPathTileDist: the cost of crossing `tile` from p1 to p2.
std::int64_t tile_dist(db::Point p1, db::Point p2, const db::Rect& tile, std::int64_t old) noexcept {
  std::int64_t d = old + absdiff(p1.x, p2.x) + absdiff(p1.y, p2.y);
  if (p1.x == p2.x && (p1.x == tile.x0 || p1.x == tile.x1)) {
    d += tile.width();
  }
  if (p1.y == p2.y && (p1.y == tile.y0 || p1.y == tile.y1)) {
    d += tile.height();
  }
  return d;
}

// --- the flood ---------------------------------------------------------------------------

/// One way out of a visited tile, in Magic's walk order.
struct Step {
  enum class Kind : std::uint8_t { side, contact, cross };
  Kind kind = Kind::side;
  tech::PlaneId plane = 0;
  db::TileView tile;
  db::Point entry;
  std::int64_t distance = 0;
};

/// One extPathFlood call: its steps, built when the tile is visited.
struct Frame {
  std::vector<Step> steps;
  std::size_t next = 0;
};

class Flood {
 public:
  Flood(const db::CellDef& scratch, const tech::Technology& tech, const PathLabel& receiver)
      : scratch_(scratch), tech_(tech), receiver_(receiver) {}

  /// extPathPairFunc: floods from `start` on `plane`.
  void run(tech::PlaneId plane, const db::TileView& start, db::Point entry) {
    std::vector<Frame> stack;
    visit(plane, start, entry, 0, stack);
    while (!stack.empty()) {
      Frame& top = stack.back();
      if (top.next == top.steps.size()) {
        stack.pop_back();
        continue;
      }
      const Step step = top.steps[top.next++];
      if (enterable(step)) {
        visit(step.plane, step.tile, step.entry, step.distance, stack);
      }
    }
  }

  [[nodiscard]] const PathDistance& result() const noexcept { return result_; }

 private:
  [[nodiscard]] const db::Plane& plane(tech::PlaneId p) const { return scratch_.plane(p); }

  /// The slot test of each step kind (TiGetClient != MARKED, or ==
  /// CLIENTDEFAULT for the contact branch and the cross-plane search).
  [[nodiscard]] bool enterable(const Step& s) const {
    const std::uintptr_t c = plane(s.plane).client(s.tile);
    return s.kind == Step::Kind::side ? c != kMarked : c == db::kClientDefault;
  }

  /// extPathFlood's body: mark, the destination test, then the steps.
  void visit(tech::PlaneId p, const db::TileView& tile, db::Point entry, std::int64_t distance,
             std::vector<Frame>& stack) {
    plane(p).set_client(tile, kMarked);
    const tech::TileType type = type_of(tile);
    if (db::touch(tile.area, receiver_.rect) && tech_.connects_to(type).test(receiver_.type)) {
      const db::Point at = clip_point(entry, receiver_.rect);
      const std::int64_t d = tile_dist(entry, at, tile.area, distance);
      // Magic compares with INFINITY and MINFINITY; a path past 2^30 (an
      // int overflow there) is kept as it is.
      result_.min = reached_ ? std::min(result_.min, d) : d;
      result_.max = reached_ ? std::max(result_.max, d) : d;
      reached_ = true;
      return;
    }
    Frame frame;
    add_sides(p, tile, entry, distance, frame.steps);
    add_contact_planes(p, tile, entry, distance, frame.steps);
    add_cross_planes(p, tile, entry, distance, frame.steps);
    stack.push_back(std::move(frame));
  }

  /// The four perimeter loops (top, right, bottom, left). The candidates'
  /// type test (DBConnectsTo(neighbour, type)) does not depend on the
  /// flood, so it is made here.
  void add_sides(tech::PlaneId p, const db::TileView& tile, db::Point entry, std::int64_t distance,
                 std::vector<Step>& out) const {
    const tech::TileType type = type_of(tile);
    const db::Rect& a = tile.area;
    const std::pair<db::Side, bool> sides[] = {
        {db::Side::top, true}, {db::Side::right, true}, {db::Side::bottom, false}, {db::Side::left, false}};
    std::vector<db::TileView> across;
    for (const auto& [side, reversed] : sides) {
      const bool horizontal = side == db::Side::top || side == db::Side::bottom;
      plane(p).neighbors(tile, side, horizontal ? a.x0 : a.y0, horizontal ? a.x1 : a.y1, across);
      if (reversed) {
        std::reverse(across.begin(), across.end());
      }
      for (const db::TileView& n : across) {
        if (tech_.connects_to(type_of(n)).test(type)) {
          out.push_back(side_step(p, tile, entry, distance, n));
        }
      }
    }
  }

  /// extPathFloodTile: entered at the midpoint of the shared boundary.
  [[nodiscard]] static Step side_step(tech::PlaneId p, const db::TileView& src, db::Point entry,
                                      std::int64_t distance, const db::TileView& dst) {
    const db::Point mid = center(clip(src.area, dst.area));
    return Step{Step::Kind::side, p, dst, mid, tile_dist(entry, mid, src.area, distance)};
  }

  /// The contact branch: the tile at the same lower-left point.
  void add_contact_planes(tech::PlaneId p, const db::TileView& tile, db::Point entry, std::int64_t distance,
                          std::vector<Step>& out) const {
    const tech::TileType type = type_of(tile);
    if (!tech_.is_contact(type)) {
      return;
    }
    const tech::PlaneMask mask = tech_.contact_planes(type) & ~tech::plane_bit(p);
    for (tech::PlaneId q = tech::kFirstUserPlane; q < scratch_.plane_count(); ++q) {
      if ((mask & tech::plane_bit(q)) == 0) {
        continue;
      }
      const db::TileView there = plane(q).tile_at(db::Point{tile.area.x0, tile.area.y0});
      if (tech_.connects_to(type).test(type_of(there))) {
        out.push_back(Step{Step::Kind::contact, q, there, entry, distance});
      }
    }
  }

  /// The cross-plane search over the tile grown by one (extPathFloodFunc).
  void add_cross_planes(tech::PlaneId p, const db::TileView& tile, db::Point entry, std::int64_t distance,
                        std::vector<Step>& out) const {
    const tech::TileType type = type_of(tile);
    const tech::PlaneMask mask = tech_.cross_planes(type);
    const db::Rect bigger = db::expand(tile.area, 1);
    for (tech::PlaneId q = tech::kFirstUserPlane; q < scratch_.plane_count(); ++q) {
      if (q == p || (mask & tech::plane_bit(q)) == 0) {
        continue;
      }
      plane(q).enumerate(bigger, tech_.connects_to(type), [&](const db::TileView& dst) {
        out.push_back(cross_step(q, tile, entry, distance, dst));
        return true;
      });
    }
  }

  [[nodiscard]] static Step cross_step(tech::PlaneId q, const db::TileView& src, db::Point entry,
                                       std::int64_t distance, const db::TileView& dst) {
    if (db::overlap(src.area, dst.area)) {
      return Step{Step::Kind::cross, q, dst, entry, distance};
    }
    const db::Point mid = center(clip(src.area, dst.area));
    return Step{Step::Kind::cross, q, dst, mid, tile_dist(entry, mid, src.area, distance)};
  }

  const db::CellDef& scratch_;
  const tech::Technology& tech_;
  const PathLabel& receiver_;
  PathDistance result_;
  bool reached_ = false;
};

// --- the yank ----------------------------------------------------------------------------

/// DBTreeCopyConnect's copy into the scratch cell.
class Copier final : public connect::NetVisitor {
 public:
  Copier(db::CellDef& out, const tech::Technology& tech, bool magic_compat)
      : out_(out), tech_(tech), compat_(magic_compat) {}

  bool on_tile(const connect::NetTile& t) override {
    const db::PaintRow row = db::paint_row(tech_, t.plane, t.tile.type);
    db::Plane& plane = out_.plane(t.plane);
    const diag::Status st = t.tile.split
                                ? plane.paint_triangle(t.tile.area, t.tile.diagonal, t.tile.right_side, row, compat_)
                                : plane.paint(t.tile.area, row);
    return st.ok();
  }

 private:
  db::CellDef& out_;
  const tech::Technology& tech_;
  bool compat_;
};

/// The receivers met through `tile` (extLengthLabels), consed in front.
void receivers_at(const db::CellDef& root, const db::TileView& tile, const LengthLists& lists, bool magic_compat,
                  std::vector<PathLabel>& found) {
  db::Rect area = tile.area;
  ++area.x1;
  ++area.y1;
  connect::search_tree_labels(root, area, tech::TypeMask::of(type_of(tile)), {}, magic_compat,
                              [&](const connect::TreeLabel& l) {
                                const std::string& prefix = *l.path;
                                std::string name = prefix.empty() ? l.label->text : prefix + "/" + l.label->text;
                                if (std::find(lists.receivers.begin(), lists.receivers.end(), name) !=
                                    lists.receivers.end()) {
                                  found.push_back(PathLabel{std::move(name), l.label->type,
                                                            db::apply(l.to_root, l.label->rect)});
                                }
                                return true;
                              });
}

/// OFF: each label once, first occurrence in list order kept.
void drop_repeats(std::vector<PathLabel>& labels) {
  std::vector<PathLabel> out;
  for (PathLabel& l : labels) {
    if (std::find(out.begin(), out.end(), l) == out.end()) {
      out.push_back(std::move(l));
    }
  }
  labels = std::move(out);
}

void report_missing(ExtractContext& ctx, const std::string& name) {
  if (ctx.sink != nullptr) {
    ctx.sink->emit(diag::make(kCodeMissing, "Can't find terminal \"" + name + "\""));
  }
}

}  // namespace

std::vector<std::string> driver_order(const LengthLists& lists) {
  util::MagicHashTable<int> table(kDriverHashSize);
  for (const std::string& name : lists.drivers) {
    table.find_or_insert(name);
  }
  std::vector<std::string> out;
  table.for_each([&out](const auto& entry) { out.push_back(entry.key); });
  return out;
}

std::vector<PathLabel> driver_labels(db::CellDef& root, const std::string& name) {
  const db::CellDef* def = &root;
  db::Transform to_root;
  std::string_view leaf = name;
  if (const std::size_t slash = name.rfind('/'); slash != std::string::npos) {
    const auto inst = cell::find_instance(root, std::string_view{name}.substr(0, slash));
    if (!inst) {
      return {};
    }
    def = inst->use->def;
    to_root = inst->to_root;
    leaf = std::string_view{name}.substr(slash + 1);
  }
  std::vector<PathLabel> out;
  for (const db::Label& l : def->labels()) {
    if (l.text == leaf) {
      out.push_back(PathLabel{name, l.type, db::apply(to_root, l.rect)});
    }
  }
  std::reverse(out.begin(), out.end());  // consed onto the front
  return out;
}

YankedPath yank_path(const db::CellDef& root, const tech::Technology& technology, const std::vector<PathLabel>& drivers,
                     const LengthLists& lists, bool magic_compat) {
  YankedPath out;
  out.scratch = std::make_unique<db::CellDef>(kScratchName, technology.plane_count());
  Copier copier(*out.scratch, technology, magic_compat);
  for (const PathLabel& d : drivers) {
    if (d.type == tech::kSpace) {
      continue;
    }
    connect::SearchStart start;
    start.area = db::expand(d.rect, 1);
    start.types = technology.connects_to(d.type);
    connect::SearchOptions options;
    options.magic_compat = magic_compat;
    (void)connect::search_hierarchical(root, technology, start, options, copier);
  }
  for (tech::PlaneId p = tech::kFirstUserPlane; p < out.scratch->plane_count(); ++p) {
    out.scratch->plane(p).enumerate(db::whole_plane(), ~tech::TypeMask::of(tech::kSpace), [&](const db::TileView& t) {
      receivers_at(root, t, lists, magic_compat, out.receivers);
      return true;
    });
  }
  std::reverse(out.receivers.begin(), out.receivers.end());  // consed onto the front
  if (!magic_compat) {
    drop_repeats(out.receivers);
  }
  return out;
}

PathDistance pair_distance(const db::CellDef& scratch, const tech::Technology& technology, const PathLabel& driver,
                           const PathLabel& receiver) {
  Flood flood(scratch, technology, receiver);
  const db::Rect r = db::expand(driver.rect, 1);
  const tech::TypeMask& mask = technology.connects_to(driver.type);
  tech::PlaneMask planes = 0;
  for (std::size_t t = 0; t < tech::kMaxTileTypes; ++t) {
    if (mask.test(static_cast<tech::TileType>(t))) {
      planes |= technology.type_planes(static_cast<tech::TileType>(t));
    }
  }
  for (tech::PlaneId p = tech::kFirstUserPlane; p < scratch.plane_count(); ++p) {
    if ((planes & tech::plane_bit(p)) == 0) {
      continue;
    }
    std::vector<db::TileView> starts;
    scratch.plane(p).enumerate(r, mask, [&starts](const db::TileView& t) {
      starts.push_back(t);
      return true;
    });
    for (const db::TileView& t : starts) {
      if (scratch.plane(p).client(t) == db::kClientDefault) {
        flood.run(p, t, center(clip(t.area, driver.rect)));
      }
    }
  }
  for (tech::PlaneId p = tech::kFirstUserPlane; p < scratch.plane_count(); ++p) {
    scratch.plane(p).reset_clients(db::whole_plane());
  }
  return flood.result();
}

PathLengthResult compute_path_lengths(db::CellDef& root, const tech::Technology& technology, const LengthLists& lists,
                                      bool magic_compat) {
  PathLengthResult result;
  std::vector<std::pair<std::string, std::vector<PathLabel>>> drivers;
  for (const std::string& name : driver_order(lists)) {
    drivers.emplace_back(name, driver_labels(root, name));
    if (drivers.back().second.empty()) {
      result.missing.push_back(name);
    }
  }
  for (const auto& [name, labels] : drivers) {
    if (labels.empty()) {
      continue;
    }
    const YankedPath yanked = yank_path(root, technology, labels, lists, magic_compat);
    for (const PathLabel& d : labels) {
      for (const PathLabel& r : yanked.receivers) {
        if (d.type != tech::kSpace) {
          result.records.push_back(PathRecord{d.text, r.text, pair_distance(*yanked.scratch, technology, d, r)});
        }
      }
    }
  }
  return result;
}

diag::Status path_lengths(ExtractContext& ctx, db::CellDef& cell, const LengthLists& lists, ExtWriter& writer) {
  if (lists.drivers.empty()) {
    return diag::Done{};  // extLength writes nothing without a driver (ExtLength.c:258-275)
  }
  if (ctx.technology == nullptr) {
    return diag::make(kCodeInputs, "Cannot extract path lengths of " + cell.name() + ": no technology.");
  }
  const PathLengthResult result = compute_path_lengths(cell, *ctx.technology, lists, ctx.compat.magic_compat);
  for (const std::string& name : result.missing) {
    report_missing(ctx, name);
  }
  for (const PathRecord& r : result.records) {
    writer.distance(r.driver, r.receiver, r.distance.min, r.distance.max);
  }
  return diag::Done{};
}

}  // namespace hefesto::extract
