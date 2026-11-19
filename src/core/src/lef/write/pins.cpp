// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF macro pins (capability lef-write "Pin enumeration and ordering",
// "Pin header records", "Power and ground pin detection", "Antenna
// areas", "Default pin geometry", "Pin-only pin geometry", "Hidden-detail
// pin geometry with zero setback", "Hidden-detail pin geometry with
// positive setback", "Top-layer pins", "PORT blocks"; Q-LEFDEF-36, 37, 44,
// 46, 50, 51): the PIN records of lefWriteMacro, one per port index, each
// label's pin selection (its net, chunk or setback ring), the antenna
// areas and the PORT block written from it.
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/lefWrite.c lefWriteMacro (the
// port loop, lefWrite.c:1389-1820), LefWritePinHeader and
// lefAccumulateArea, Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: LEFW (design D2).

#include <algorithm>
#include <cstdint>
#include <set>

#include "hefesto/connect/search.hpp"
#include "hefesto/db/paint.hpp"
#include "lef/write/chunk.hpp"
#include "lef/write/macro_impl.hpp"

namespace hefesto::lef::write_detail {

namespace {

// Degenerate (line or point) labels grow by 1 in each direction.
db::Rect grown_label(const db::Rect& r) {
  db::Rect g = r;
  if (g.x1 == g.x0) {
    ++g.x1;
    --g.x0;
  }
  if (g.y1 == g.y0) {
    ++g.y1;
    --g.y0;
  }
  return g;
}

bool power_use(db::PortUse use) noexcept {
  return use == db::PortUse::power || use == db::PortUse::ground;
}

// Power and ground ports and `lef nocheck` nets get no antenna areas.
bool antenna_ignored(const MacroContext& ctx, const db::Label& lab) {
  if (power_use(lab.port_use)) {
    return true;
  }
  return std::any_of(ctx.nocheck.begin(), ctx.nocheck.end(),
                     [&](const std::string& n) { return n == lab.text || n == "*"; });
}

/// Collects a net into the selection cell (SelectNet into SelectDef).
struct NetCollector final : connect::NetVisitor {
  db::CellDef& dest;
  const tech::Technology& tech;
  bool magic_compat;
  NetCollector(db::CellDef& d, const tech::Technology& t, bool compat) : dest(d), tech(t), magic_compat(compat) {}
  bool on_tile(const connect::NetTile& t) override {
    const db::PaintRow row = db::paint_row(tech, t.plane, t.tile.type);
    if (t.tile.split) {
      (void)dest.plane(t.plane).paint_triangle(t.tile.area, t.tile.diagonal, t.tile.right_side, row, magic_compat);
    } else {
      (void)dest.plane(t.plane).paint(t.tile.area, row);
    }
    return true;
  }
};

// SelectNet(&scx, type, ...) over the flat copy (DBTreeCopyConnect from
// the types connecting to `type`; the flat copy has no labels).
void select_net(const MacroState& m, db::CellDef& sel, const db::Rect& area, tech::TileType type) {
  if (type == tech::kSpace) {
    return;
  }
  connect::SearchStart start;
  start.area = area;
  start.types = m.tech().connects_to(type);
  connect::SearchOptions options;
  options.magic_compat = m.ctx.magic_compat;
  NetCollector collect(sel, m.tech(), m.ctx.magic_compat);
  (void)connect::search_hierarchical(*m.flat, m.tech(), start, options, collect);
}

// The antenna area of `types` in the selection: the first plane whose sum
// is positive; a split tile counts half its rectangle (integer division).
std::int64_t antenna_area(const db::CellDef& sel, const tech::TypeMask& types) {
  for (std::size_t p = tech::kFirstUserPlane; p < sel.plane_count(); ++p) {
    std::int64_t sum = 0;
    sel.plane(static_cast<tech::PlaneId>(p)).enumerate(db::whole_plane(), types, [&](const db::TileView& t) {
      const std::int64_t area = t.area.width() * t.area.height();
      sum += t.split ? area / 2 : area;
      return true;
    });
    if (sum > 0) {
      return sum;
    }
  }
  return 0;
}

// SelRemoveArea(area, DBAllButSpaceAndDRCBits): every type, every plane.
void remove_area(db::CellDef& sel, const db::Rect& area) {
  db::PaintRow to_space{};
  to_space.fill(tech::kSpace);
  for (std::size_t p = tech::kFirstUserPlane; p < sel.plane_count(); ++p) {
    (void)sel.plane(static_cast<tech::PlaneId>(p)).paint(area, to_space);
  }
}

// DBPaintPlane of the label's type on its home plane (labels on space are
// ignored).
void paint_label_type(const MacroState& m, db::CellDef& sel, const db::Rect& area, tech::TileType type) {
  const auto home = m.tech().home_plane(type);
  if (type == tech::kSpace || !home || *home >= sel.plane_count()) {
    return;
  }
  (void)sel.plane(*home).paint(area, db::paint_row(m.tech(), *home, type), db::PaintMerge::none);
}

db::Rect shrunk(const db::Rect& r, db::Coord by) {
  return db::expand(r, -by);
}

// `c` minus `hole` as up to four rectangles (below, above, left, right).
std::vector<db::Rect> subtract(const db::Rect& c, const db::Rect& hole) {
  const db::Rect h{std::max(c.x0, hole.x0), std::max(c.y0, hole.y0), std::min(c.x1, hole.x1), std::min(c.y1, hole.y1)};
  if (h.is_null()) {
    return {c};
  }
  std::vector<db::Rect> out;
  const auto keep = [&](const db::Rect& r) {
    if (!r.is_null()) {
      out.push_back(r);
    }
  };
  keep(db::Rect{c.x0, c.y0, c.x1, h.y0});
  keep(db::Rect{c.x0, h.y1, c.x1, c.y1});
  keep(db::Rect{c.x0, h.y0, h.x0, h.y1});
  keep(db::Rect{h.x1, h.y0, c.x1, h.y1});
  return out;
}

// The -hide 0 chunk C: of the label's type, else of the first connected
// type in type order (selected on its own type), else the label.
db::Rect chunk_pin(const MacroState& m, db::CellDef& sel, const db::Rect& labr, tech::TileType type) {
  const auto select = [&](const db::Rect& c, tech::TileType t) {
    (void)db::copy_paint(*m.flat, c, db::kIdentity, sel, m.tech(), tech::TypeMask::of(t));
  };
  db::Rect c = chunk_rect(m.tech(), *m.flat, labr, type);
  if (!c.is_null()) {
    select(c, type);
    return c;
  }
  const std::size_t end = std::min(m.tech().type_count(), tech::kMaxTileTypes);
  for (std::size_t i = tech::kFirstUserType; i < end; ++i) {
    const auto tt = static_cast<tech::TileType>(i);
    if (tt == type || !m.tech().connects_to(tt).test(type)) {
      continue;
    }
    c = chunk_rect(m.tech(), *m.flat, labr, tt);
    if (!c.is_null()) {
      select(c, tt);
      return c;
    }
  }
  return labr;
}

// -hide with setback 0: the pin is one rectangle C painted with the
// label's type, remembered for the obstruction cut-out. With -pinonly p >
// 0 and magic-compat OFF, C minus B shrunk by p plus the label rectangle
// (Q-LEFDEF-51).
void hide_zero_pin(MacroState& m, db::CellDef& sel, const db::Label& lab, const db::Rect& labr) {
  const db::Coord p = m.options.pin_only;
  db::Rect carea = labr;
  if (p != 0) {
    carea = chunk_pin(m, sel, labr, lab.type);
    if (p > 0 && !carea.is_null()) {
      remove_area(sel, shrunk(m.boundary, p));
    }
  }
  std::vector<db::Rect> pieces{carea};
  if (!m.ctx.magic_compat && p > 0) {
    pieces = subtract(carea, shrunk(m.boundary, p));
    pieces.push_back(labr);
  }
  for (const db::Rect& piece : pieces) {
    paint_label_type(m, sel, piece, lab.type);
    m.cutouts.push_back(Cutout{lab.type, piece});
  }
}

// -hide d > 0: the net within d of the boundary, plus the label. -pinonly
// p limits it further: with magic-compat ON only when p > d (no effect);
// OFF when 0 <= p < d (Q-LEFDEF-37).
void hide_setback_pin(const MacroState& m, db::CellDef& sel, const db::Label& lab, const db::Rect& labr) {
  const db::Coord d = m.options.setback;
  const db::Coord p = m.options.pin_only;
  remove_area(sel, shrunk(m.boundary, d));
  const bool limit = m.ctx.magic_compat ? p > d : (p >= 0 && p < d);
  if (limit) {
    remove_area(sel, shrunk(m.boundary, p));
  }
  paint_label_type(m, sel, labr, lab.type);
}

// -pinonly p without -hide: the net within p of the boundary, plus the
// label.
void pin_only_pin(const MacroState& m, db::CellDef& sel, const db::Label& lab, const db::Rect& labr) {
  if (m.options.pin_only < 0) {
    return;
  }
  remove_area(sel, shrunk(m.boundary, m.options.pin_only));
  paint_label_type(m, sel, labr, lab.type);
}

std::string_view direction_word(db::PortClass c) noexcept {
  switch (c) {
    case db::PortClass::input:
      return "INPUT";
    case db::PortClass::output:
      return "OUTPUT";
    case db::PortClass::tristate:
      return "OUTPUT TRISTATE";
    case db::PortClass::bidirectional:
      return "INOUT";
    case db::PortClass::feedthrough:
      return "FEEDTHRU";
    case db::PortClass::default_class:
      break;
  }
  return "";
}

// The USE keyword; reset, scan and tie-off have none in Magic (an empty
// keyword with magic-compat ON) and are SIGNAL with it OFF (Q-LEFDEF-36).
std::string_view use_word(db::PortUse u, bool magic_compat) noexcept {
  switch (u) {
    case db::PortUse::signal:
      return "SIGNAL";
    case db::PortUse::analog:
      return "ANALOG";
    case db::PortUse::power:
      return "POWER";
    case db::PortUse::ground:
      return "GROUND";
    case db::PortUse::clock:
      return "CLOCK";
    case db::PortUse::reset:
    case db::PortUse::scan:
    case db::PortUse::tieoff:
      return magic_compat ? "" : "SIGNAL";
    case db::PortUse::default_use:
      break;
  }
  return "";
}

std::string_view shape_word(db::PortShape s) noexcept {
  switch (s) {
    case db::PortShape::abutment:
      return "ABUTMENT";
    case db::PortShape::ring:
      return "RING";
    case db::PortShape::feedthrough:
      return "FEEDTHRU";
    case db::PortShape::default_shape:
      break;
  }
  return "";
}

/// The PIN records of one port index.
class PinWriter {
 public:
  PinWriter(MacroState& m, std::uint32_t index, std::vector<bool>& visited)
      : m_(m), labels_(m.cell.labels()), index_(index), visited_(visited) {}

  void run();

 private:
  [[nodiscard]] std::optional<std::size_t> next_label(std::size_t from) const;
  void process(std::size_t i);
  void select_pin(db::CellDef& sel, const db::Label& lab, const db::Rect& labr);
  void emit(std::size_t i, db::CellDef& sel, std::int64_t gate, std::int64_t diff);
  [[nodiscard]] bool plane_written(std::size_t p, const std::optional<std::size_t>& top) const;
  void write_header(const db::Label& lab, std::int64_t gate, std::int64_t diff);
  [[nodiscard]] bool power_name(const db::Label& lab, std::string& lines) const;
  void mark_covered(std::size_t i, const db::CellDef& yank, const db::CellDef& sel);
  [[nodiscard]] tech::TypeMask coverage_mask(const db::Label& lab) const;
  void report_duplicates(std::size_t first) const;

  MacroState& m_;
  const std::vector<db::Label>& labels_;
  std::uint32_t index_;
  std::vector<bool>& visited_;
  bool header_needed_ = true;
};

// The first unvisited port label with this index at or after `from`.
std::optional<std::size_t> PinWriter::next_label(std::size_t from) const {
  for (std::size_t i = from; i < labels_.size(); ++i) {
    if (labels_[i].is_port() && !visited_[i] && labels_[i].port_index == index_) {
      return i;
    }
  }
  return std::nullopt;
}

void PinWriter::run() {
  const std::optional<std::size_t> first = next_label(0);
  if (!first) {
    return;
  }
  for (std::optional<std::size_t> i = first; i; i = next_label(*i + 1)) {
    process(*i);
    visited_[*i] = true;
  }
  const db::Label& reference = labels_[*first];
  if (!header_needed_) {
    *m_.out += std::string{kIn0} + "END " + reference.text + "\n";
  } else {
    m_.message("LEF-WRITE-005",
               "Port \"" + reference.text + "\" (index " + std::to_string(index_) +
                   ") has no geometry on a LEF layer; no PIN is written (Q-LEFDEF-46)",
               true);
  }
  report_duplicates(*first);
}

void PinWriter::process(std::size_t i) {
  const db::Label& lab = labels_[i];
  const db::Rect labr = grown_label(lab.rect);
  auto sel = scratch_cell(m_.tech(), "__SELECT__");
  const bool ignored = antenna_ignored(m_.ctx, lab);
  if (!ignored || m_.options.setback != 0) {
    select_net(m_, *sel, labr, lab.type);
  }
  std::int64_t gate = 0;
  std::int64_t diff = 0;
  if (!ignored) {
    gate = antenna_area(*sel, m_.sets.gate);
    diff = antenna_area(*sel, m_.sets.diffusion);
  }
  if (m_.options.setback == 0) {
    sel = scratch_cell(m_.tech(), "__SELECT__");  // SelectClear: the net was for the antenna only
  }
  select_pin(*sel, lab, labr);
  emit(i, *sel, gate, diff);
}

void PinWriter::select_pin(db::CellDef& sel, const db::Label& lab, const db::Rect& labr) {
  if (m_.options.setback == 0) {
    hide_zero_pin(m_, sel, lab, labr);
  } else if (m_.options.setback > 0) {
    hide_setback_pin(m_, sel, lab, labr);
  } else {
    pin_only_pin(m_, sel, lab, labr);
  }
}

// The highest plane holding any of the selection (lefFindTopmost).
std::optional<std::size_t> top_plane(const db::CellDef& sel) {
  for (std::size_t p = sel.plane_count(); p-- > tech::kFirstUserPlane;) {
    const bool empty = sel.plane(static_cast<tech::PlaneId>(p))
                           .enumerate(db::whole_plane(), all_but_space_and_drc(),
                                      [](const db::TileView&) { return false; });
    if (!empty) {
      return p;
    }
  }
  return std::nullopt;
}

// -toplayer writes the top plane, and with domaster the masterslice
// planes.
bool PinWriter::plane_written(std::size_t p, const std::optional<std::size_t>& top) const {
  if (!m_.options.top_layer || (top && *top == p)) {
    return true;
  }
  return m_.options.do_master && m_.sets.master_planes != 0 &&
         (m_.sets.master_planes & tech::plane_bit(static_cast<tech::PlaneId>(p))) != 0;
}

// The PORT block of one label: per plane, the routing round, the erase of
// the selection from the flat copy, then the cut round.
void PinWriter::emit(std::size_t i, db::CellDef& sel, std::int64_t gate, std::int64_t diff) {
  const db::Label& lab = labels_[i];
  RecordWriter w = m_.writer();
  w.begin(Block::port);
  if (header_needed_) {
    w.header = [this, &lab, gate, diff] { write_header(lab, gate, diff); };
  }
  const std::optional<std::size_t> top = m_.options.top_layer ? top_plane(sel) : std::nullopt;
  auto yank = scratch_cell(m_.tech(), "__lefYank__");
  const Yank y = m_.yank();
  for (std::size_t p = tech::kFirstUserPlane; p < sel.plane_count(); ++p) {
    if (!plane_written(p, top)) {
      continue;
    }
    const auto plane = static_cast<tech::PlaneId>(p);
    yank_routing(y, sel.plane(plane), plane, db::whole_plane(), yank->plane(plane));
    w.write_plane(m_.tech(), yank->plane(plane), m_.sets.route, db::whole_plane(), Round::routing);
    erase_from(y, sel.plane(plane), plane, m_.flat->plane(plane));
    yank_contacts(y, sel.plane(plane), plane, db::whole_plane(), yank->plane(plane));
    w.write_plane(m_.tech(), yank->plane(plane), m_.sets.route, db::whole_plane(), Round::cut);
  }
  mark_covered(i, *yank, sel);
  if (w.writes() > 0) {
    *m_.out += std::string{kIn1} + "END\n";
  }
}

// lefWritePinHeader, then the antenna lines of a pin that is no power rail.
void PinWriter::write_header(const db::Label& lab, std::int64_t gate, std::int64_t diff) {
  header_needed_ = false;
  warn_name(m_, "Pin", lab.text);
  std::string lines = std::string{kIn0} + "PIN " + lab.text + "\n";
  if (lab.port_class != db::PortClass::default_class) {
    lines += std::string{kIn1} + "DIRECTION " + std::string{direction_word(lab.port_class)} + " ;\n";
  }
  bool rail = false;
  if (lab.port_use != db::PortUse::default_use) {
    lines += std::string{kIn1} + "USE " + std::string{use_word(lab.port_use, m_.ctx.magic_compat)} + " ;\n";
    rail = power_use(lab.port_use);
  } else {
    rail = power_name(lab, lines);
  }
  if (lab.port_shape != db::PortShape::default_shape) {
    lines += std::string{kIn1} + "SHAPE " + std::string{shape_word(lab.port_shape)} + " ;\n";
  }
  if (!rail) {
    const float square = m_.ctx.oscale * m_.ctx.oscale;
    if (gate > 0) {
      lines += std::string{kIn1} + "ANTENNAGATEAREA " +
               format_real(static_cast<double>(square * static_cast<float>(gate))) + " ;\n";
    }
    if (diff > 0) {
      lines += std::string{kIn1} + "ANTENNADIFFAREA " +
               format_real(static_cast<double>(square * static_cast<float>(diff))) + " ;\n";
    }
  }
  *m_.out += lines;
}

// A default-use pin named like the configured power or ground net
// (Q-LEFDEF-44).
bool PinWriter::power_name(const db::Label& lab, std::string& lines) const {
  bool rail = false;
  if (m_.ctx.vdd && lab.text == *m_.ctx.vdd) {
    rail = true;
    lines += std::string{kIn1} + "USE POWER ;\n";
  }
  if (m_.ctx.gnd && lab.text == *m_.ctx.gnd) {
    rail = true;
    lines += std::string{kIn1} + "USE GROUND ;\n";
  }
  return rail;
}

// A sticky label also covers the contacts holding its type.
tech::TypeMask PinWriter::coverage_mask(const db::Label& lab) const {
  tech::TypeMask mask = tech::TypeMask::of(lab.type);
  if (!lab.sticky) {
    return mask;
  }
  const std::size_t end = std::min(m_.tech().type_count(), tech::kMaxTileTypes);
  for (std::size_t t = tech::kFirstUserType; t < end; ++t) {
    const auto tt = static_cast<tech::TileType>(t);
    if (m_.tech().is_contact(tt) && m_.tech().full_residues(tt).test(lab.type)) {
      mask.set(tt);
    }
  }
  return mask;
}

bool has_paint(const db::Plane& plane, const db::Rect& area, const tech::TypeMask& mask) {
  return !plane.enumerate(area, mask, [](const db::TileView&) { return false; });
}

// Later labels of this index whose rectangle the yanked pin (or, with
// -toplayer, the selection) already covers produce no PORT.
void PinWriter::mark_covered(std::size_t i, const db::CellDef& yank, const db::CellDef& sel) {
  for (std::size_t j = i + 1; j < labels_.size(); ++j) {
    const db::Label& other = labels_[j];
    if (!other.is_port() || visited_[j] || other.port_index != index_) {
      continue;
    }
    const auto home = m_.tech().home_plane(other.type);
    if (other.type == tech::kSpace || !home || *home >= yank.plane_count()) {
      continue;  // Magic indexes its planes with -1 here
    }
    const tech::TypeMask mask = coverage_mask(other);
    if (has_paint(yank.plane(*home), other.rect, mask) ||
        (m_.options.top_layer && has_paint(sel.plane(*home), other.rect, mask))) {
      visited_[j] = true;
    }
  }
}

// One error per later label of this index with another text (Q-LEFDEF-50).
void PinWriter::report_duplicates(std::size_t first) const {
  const db::Label& reference = labels_[first];
  for (std::size_t j = first + 1; j < labels_.size(); ++j) {
    const db::Label& other = labels_[j];
    if (other.is_port() && other.port_index == index_ && other.text != reference.text) {
      m_.message("LEF-WRITE-004", "Index " + std::to_string(index_) + " is used for ports \"" + reference.text +
                                      "\" and \"" + other.text + "\"");
    }
  }
}

}  // namespace

// The pins in port-index order (indices without a label are skipped).
diag::Status write_pins(MacroState& m) {
  const std::vector<db::Label>& labels = m.cell.labels();
  std::set<std::uint32_t> indices;
  for (const db::Label& lab : labels) {
    if (lab.is_port()) {
      indices.insert(lab.port_index);
    }
  }
  std::vector<bool> visited(labels.size(), false);
  for (const std::uint32_t index : indices) {
    PinWriter(m, index, visited).run();
  }
  return diag::Done{};
}

}  // namespace hefesto::lef::write_detail
