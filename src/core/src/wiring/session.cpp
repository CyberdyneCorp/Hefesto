// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The wiring operations on the editor session (wiring-tool "Wire type
// selection", "Wire legs", "Contact placement", "wire show, values and
// width"). Ported from Magic VLSI 8.3.694 (fc5daa9e) wiring/wireOps.c and the
// chunk selection of select/selCreate.c:734-886 (SelectChunk), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md). Owner: WIRE.
//
// Not yet reproduced (other milestones or workstreams):
//  - the selection goes through select::Selection's storage directly; its
//    undo events and label selection come with SELECT's area/chunk API.

#include "wiring/session.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <numeric>
#include <utility>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/drc/engine/marking.hpp"
#include "hefesto/editor/coordinate_units.hpp"
#include "hefesto/editor/edit_frame.hpp"
#include "hefesto/wiring/diag_codes.hpp"

namespace hefesto::wiring {

namespace {

// --- hierarchy views -------------------------------------------------------------

// Where material is looked for: a root definition and the window whose
// expansion state applies (nullptr: every instance counts as expanded,
// Magic's xMask 0; also the fallback when no window shows the root).
struct View {
  db::CellDef* root = nullptr;
  const view::LayoutWindow* window = nullptr;
};

using DefVisitor = std::function<void(const db::CellDef& def, const db::Transform& to_root)>;

// The root definition, then every expanded instance element whose bounding
// box overlaps `area` (DBTreeSrTiles' traversal).
void for_each_def(const View& v, const db::Rect& area, const DefVisitor& visit) {
  visit(*v.root, db::kIdentity);
  db::walk_hierarchy(*v.root, [&](const db::CellUse& use, const db::Transform& to_root, int) {
    if (use.def == nullptr || (v.window != nullptr && !v.window->is_expanded(use)) ||
        !db::overlap(db::apply(to_root, use.def->bbox()), area)) {
      return false;
    }
    visit(*use.def, to_root);
    return true;
  });
}

// DBSeeTypesAll: the types of the paint under `area`.
tech::TypeMask types_under(const View& v, const db::Rect& area) {
  tech::TypeMask mask;
  for_each_def(v, area, [&](const db::CellDef& def, const db::Transform& to_root) {
    const db::Rect local = db::apply(db::invert(to_root), area);
    for (tech::PlaneId p = tech::kFirstUserPlane; p < def.plane_count(); ++p) {
      def.plane(p).enumerate(local, tech::TypeMask::all(), [&](const db::TileView& t) {
        mask.set(t.type);
        return true;
      });
    }
  });
  return mask;
}

// Copies the paint of `types` under `area` (root coordinates) into `target`.
void copy_area(const WireSession& s, const View& v, const db::Rect& area, const tech::TypeMask& types,
               db::CellDef& target) {
  for_each_def(v, area, [&](const db::CellDef& def, const db::Transform& to_root) {
    const db::Rect local = db::apply(db::invert(to_root), area);
    (void)db::copy_paint(def, local, to_root, target, *s.tech, types);
  });
}

// SelectChunk's split-tile shortcut: the first split tile of `types` under
// `area` (every instance expanded).
std::optional<db::Rect> split_chunk(const View& v, const db::Rect& area, const tech::TypeMask& types) {
  std::optional<db::Rect> found;
  for_each_def(View{v.root, nullptr}, area, [&](const db::CellDef& def, const db::Transform& to_root) {
    const db::Rect local = db::apply(db::invert(to_root), area);
    for (tech::PlaneId p = tech::kFirstUserPlane; p < def.plane_count() && !found; ++p) {
      def.plane(p).enumerate(local, types, [&](const db::TileView& t) {
        if (t.split) {
          found = db::apply(to_root, t.area);
        }
        return !t.split;
      });
    }
  });
  return found;
}

// `type` plus the stacked types containing it (DBMaskAddStacking).
tech::TypeMask with_stacking(const tech::Technology& tech, tech::TileType type) {
  tech::TypeMask mask = tech::TypeMask::of(type);
  if (tech.is_contact(type)) {
    for (const tech::ContactInfo& c : tech.contacts()) {
      if (c.stacked && (c.component_a == type || c.component_b == type)) {
        mask.set(c.type);
      }
    }
  }
  return mask;
}

// SelectChunk's growing search: extract the material around the area, find
// the best chunk, and grow the extracted area where the chunk touches it.
std::optional<db::Rect> search_chunk(const WireSession& s, const View& v, const db::Rect& area, tech::TileType type,
                                     const tech::TypeMask& types) {
  const auto plane = s.tech->home_plane(type);
  if (!plane) {
    return std::nullopt;
  }
  constexpr db::Coord kInitialSize = 10;
  db::Rect search = db::expand(area, kInitialSize);
  std::optional<db::Rect> best;
  while (true) {
    db::CellDef scratch{"__wire_chunk__", s.tech->plane_count()};
    copy_area(s, v, search, types, scratch);
    best = find_chunk(scratch.plane(*plane), ~types, search, area, best);
    if (!best || db::strong_surround(search, *best)) {
      return best;
    }
    const db::Coord w = search.x1 - search.x0;
    const db::Coord h = search.y1 - search.y0;
    search.x0 = best->x0 == search.x0 ? search.x0 - w : best->x0 - 1;
    search.y0 = best->y0 == search.y0 ? search.y0 - h : best->y0 - 1;
    search.x1 = best->x1 == search.x1 ? search.x1 + w : best->x1 + 1;
    search.y1 = best->y1 == search.y1 ? search.y1 + h : best->y1 + 1;
  }
}

// SelectArea for paint: adds the paint of `types` under `area` to the
// selection rooted at the view's root.
void select_area(const WireSession& s, const View& v, const db::Rect& area, const tech::TypeMask& types) {
  select::Selection& sel = s.ed->selection;
  sel.set_root(v.root, s.tech->plane_count());
  copy_area(s, v, area, types, *sel.def());
  sel.provenance().push_back(select::Provenance{select::Provenance::Kind::paint, v.root, {}, area});
  s.ed->highlights.request(v.root, area, true);
}

// SelectChunk: selects the chunk of `type` containing `area`; its area.
std::optional<db::Rect> select_chunk(const WireSession& s, const View& v, const db::Rect& area, tech::TileType type) {
  const tech::TypeMask types = with_stacking(*s.tech, type);
  std::optional<db::Rect> chunk = split_chunk(v, area, types);
  if (!chunk) {
    chunk = search_chunk(s, v, area, type, types);
  }
  if (chunk) {
    select_area(s, v, *chunk, tech::TypeMask::of(type));
  }
  return chunk;
}

const view::LayoutWindow* root_window(const editor::EditorState& ed, const db::CellDef* root) {
  const view::LayoutWindow* found = nullptr;
  ed.windows.for_each([&](view::LayoutWindow& w) {
    if (found == nullptr && w.root() == root) {
      found = &w;
    }
  });
  return found;
}

db::PaintOptions paint_options(const WireSession& s) {
  db::PaintOptions o;
  o.locked = s.ed->locked_types;
  o.magic_compat = s.magic_compat;
  return o;
}

// DBAdjustLabels, DBWAreaChanged and DBReComputeBbox after painting.
void finish_paint(const WireSession& s, const db::Rect& edit_area, const tech::TypeMask& layers) {
  db::CellDef& def = *s.ed->edit_cell;
  std::vector<std::string> notes;
  const bool labels = db::adjust_labels(def, *s.tech, edit_area, &notes);
  for (std::string& n : notes) {
    s.report(std::move(n));
  }
  db::recompute_bbox(def, s.magic_compat);
  s.ed->windows.area_changed(def, edit_area, labels ? std::nullopt : std::optional<tech::TypeMask>(layers));
  drc::check_this(*s.ed, def, edit_area);  // WireAddLeg, WireAddContact
}

// The next type present in `mask` after `current`, cycling through the user
// types (WirePickType's loop; stacked types are never picked).
std::optional<tech::TileType> next_type(const tech::Technology& tech, std::optional<tech::TileType> current,
                                        const tech::TypeMask& mask) {
  const std::size_t first = tech::kFirstUserType;
  const std::size_t end = tech.user_type_end();
  if (end <= first) {
    return std::nullopt;
  }
  std::size_t t = current ? std::size_t{*current} + 1 : first;
  for (std::size_t n = 0; n < end - first; ++n, ++t) {
    if (t >= end || t < first) {
      t = first;
    }
    if (mask.test(static_cast<tech::TileType>(t))) {
      return static_cast<tech::TileType>(t);
    }
  }
  return std::nullopt;
}

tech::TypeMask user_types(const tech::Technology& tech) {
  tech::TypeMask m;
  for (std::size_t t = tech::kFirstUserType; t < tech.user_type_end(); ++t) {
    m.set(static_cast<tech::TileType>(t));
  }
  return m;
}

std::string type_name(const WireSession& s, std::optional<tech::TileType> t) {
  return std::string{s.tech->type_name(t.value_or(tech::kSpace))};
}

// The box checks shared by legs and contacts; the message lines differ.
std::optional<cmd::CommandResult> check_box(const WireSession& s, std::string_view no_box, std::string_view elsewhere) {
  if (s.ed->edit_cell == nullptr) {
    return fail(codes::kNoBox, "No cell being edited");
  }
  if (!s.ed->box) {
    return fail(codes::kNoBox, std::string{no_box});
  }
  if (s.box_root() != s.edit_root()) {
    return fail(codes::kNoBox, std::string{elsewhere});
  }
  return std::nullopt;
}

std::optional<WiringContact> contact_between(const WiringSection& section, tech::TileType from, tech::TileType to) {
  for (const WiringContact& c : section.contacts) {
    if ((c.layer1 == from && c.layer2 == to) || (c.layer2 == from && c.layer1 == to)) {
      return section.scaled(c);
    }
  }
  return std::nullopt;
}

}  // namespace

// --- WireSession -------------------------------------------------------------------

void WireSession::report(std::string text) const {
  emit(codes::kReport, std::move(text));
}

void WireSession::emit(std::string_view code, std::string text) const {
  ctx->emit(diag::make(code, std::move(text)));
}

db::CellDef* WireSession::edit_root() const {
  return ed->edit_root != nullptr ? ed->edit_root : ed->edit_cell;
}

db::CellDef* WireSession::box_root() const {
  return ed->box_root != nullptr ? ed->box_root : ed->edit_cell;
}

cmd::CoordinateUnits WireSession::units() const {
  return editor::coordinate_units(*ed, magic_compat);
}

db::Coord WireSession::parse_coord(std::string_view text, bool relative, bool is_x) const {
  // Kept inside the usable plane so that the leg arithmetic cannot overflow;
  // geometry outside it is rejected by the paint primitives.
  return std::clamp(scale_coord(text, units(), relative, is_x, 1, &ctx->sink()), db::kPlaneMin, db::kPlaneMax);
}

std::string WireSession::format_length(db::Coord value) const {
  return cmd::format_value(value, ed->units, units(), cmd::ValueKind::length, cmd::Axis::x);
}

WiringSection WireSession::section() const {
  auto parsed = parse_wiring_section(*tech, ed->grid);
  return parsed.ok() ? std::move(parsed).value() : WiringSection{};
}

diag::Outcome<WireSession> wire_session(cmd::CommandContext& ctx) {
  auto ed = cmd::require_editor(ctx);
  if (!ed.ok()) {
    return ed.error();
  }
  WireSession s;
  s.ctx = &ctx;
  s.ed = ed.value();
  s.tech = s.ed->technology.get();
  s.magic_compat = ctx.project().compat.magic_compat;
  if (s.tech == nullptr) {
    return diag::make(diag::codes::kCmdSession, "the session has no technology");
  }
  return s;
}

cmd::CommandResult fail(std::string_view code, std::string text) {
  return cmd::CommandResult::failure(diag::make(code, std::move(text)));
}

std::optional<Cursor> cursor(const editor::EditorState& ed) {
  if (!ed.command_point) {
    return std::nullopt;
  }
  const view::LayoutWindow* w = ed.windows.find(ed.command_point->window);
  if (w == nullptr || w->root() == nullptr) {
    return std::nullopt;
  }
  const db::Point p = ed.command_point->point;
  db::Rect area = ed.command_point->pixel_area;
  if (area.is_null()) {
    area = db::Rect{p.x, p.y, p.x + 1, p.y + 1};
  }
  return Cursor{w, p, area};
}

void clear_selection(const WireSession& s) {
  select::Selection& sel = s.ed->selection;
  if (sel.root() != nullptr && sel.def() != nullptr && !sel.empty()) {
    s.ed->highlights.request(sel.root(), sel.def()->bbox(), true);
  }
  sel.clear();
}

void pick_type(const WireSession& s, std::optional<tech::TileType> type, db::Coord width) {
  s.ed->wire.type = type;
  s.ed->wire.width = width;
  s.ed->wire.last_direction = LegDirection::none;
  remember_for_undo(*s.ed);
}

cmd::CommandResult pick_type_at(const WireSession& s, std::optional<db::Point> root_point) {
  View v;
  db::Rect area;
  if (root_point) {
    v.root = s.edit_root();
    v.window = root_window(*s.ed, v.root);
    area = db::Rect{root_point->x, root_point->y, root_point->x + 1, root_point->y + 1};
  } else if (const auto c = cursor(*s.ed)) {
    v = View{c->window->root(), c->window};
    area = c->area;
  }
  if (v.root == nullptr) {
    return fail(codes::kNoMaterial,
                "Can't use cursor to select wiring material unless\n    cursor is in a layout window.");
  }
  tech::TypeMask mask = types_under(v, area) & user_types(*s.tech);
  if (v.window != nullptr) {
    mask &= v.window->visible_types();
  }
  const auto type = next_type(*s.tech, s.ed->wire.type, mask);
  if (!type) {
    return fail(codes::kNoMaterial, "There's no material visible underneath the cursor.");
  }
  clear_selection(s);
  const db::Rect chunk = select_chunk(s, v, area, *type).value_or(area);
  const auto width = static_cast<db::Coord>(std::min(chunk.width(), chunk.height()));
  const db::Rect box = pick_box(area, chunk, width);
  clear_selection(s);
  select_area(s, v, box, tech::TypeMask::of(*type));
  s.ed->box = box;
  s.ed->box_root = v.root;
  s.report("Using " + type_name(s, type) + " wires " + std::to_string(width) + " units wide.");
  pick_type(s, type, width);
  return cmd::CommandResult::success();
}

cmd::CommandResult add_leg(const WireSession& s, std::optional<db::Point> root_point, LegMode mode) {
  WireState& w = s.ed->wire;
  if (!w.type) {
    return fail(codes::kNoMaterial, "Can't add a new wire leg:  no wiring material selected.");
  }
  if (auto bad = check_box(s, "No box!  Before wiring a leg, you must set the box\n    to indicate where the leg starts.",
                           "The box must be on the edit cell so it can be used\n    as the starting point for a wire leg.")) {
    return *bad;
  }
  if (!root_point) {
    const auto c = cursor(*s.ed);
    if (!c || c->window->root() != s.edit_root()) {
      return fail(codes::kNoMaterial,
                  "Before wiring, must place cursor over edit cell to\n    indicate endpoint of new wire leg.");
    }
    root_point = c->point;
  }
  const auto leg = plan_leg(*s.ed->box, *root_point, w.width, mode);
  if (!leg) {
    return cmd::CommandResult::success();  // nothing to paint
  }
  w.last_direction = leg->direction;
  const db::Rect edit_area = editor::to_edit(*s.ed, leg->area);
  if (auto st = db::paint(*s.ed->edit_cell, *s.tech, edit_area, *w.type, paint_options(s)); !st.ok()) {
    return cmd::CommandResult::failure(st.error());
  }
  finish_paint(s, edit_area, tech::TypeMask::of(*w.type));
  clear_selection(s);
  const View v{s.edit_root(), nullptr};
  const db::Rect chunk = select_chunk(s, v, leg->area, *w.type).value_or(leg->area);
  s.ed->box = leg_tip_box(*leg, chunk, w.width);
  s.ed->box_root = s.edit_root();
  remember_for_undo(*s.ed);
  return cmd::CommandResult::success();
}

void show_leg(const WireSession& s) {
  WireState& w = s.ed->wire;
  const auto c = cursor(*s.ed);
  if (!w.type || !s.ed->box || s.edit_root() == nullptr || s.box_root() != s.edit_root() || !c ||
      c->window->root() != s.edit_root()) {
    return;
  }
  const auto leg = plan_leg(*s.ed->box, c->point, w.width, LegMode::choose);
  if (!leg) {
    return;
  }
  w.last_direction = leg->direction;  // as WireShowLeg does (not recorded)
  clear_selection(s);
  select::Selection& sel = s.ed->selection;
  sel.set_root(s.edit_root(), s.tech->plane_count());
  (void)db::paint(*sel.def(), *s.tech, leg->area, *w.type, paint_options(s));
  db::recompute_bbox(*sel.def(), s.magic_compat);
  sel.provenance().push_back(select::Provenance{select::Provenance::Kind::paint, s.ed->edit_cell, {}, leg->area});
  s.ed->highlights.request(s.edit_root(), leg->area, true);
}

cmd::CommandResult add_contact(const WireSession& s, std::optional<tech::TileType> type, db::Coord width) {
  if (auto bad = check_box(s,
                           "No box!  To place a contact, you must first use\n    the box to mark the previous leg of "
                           "the wire,\n    at the end of which the contact will be placed.",
                           "The box must be on the edit cell; it marks the wire\n    leg at the end of which a contact "
                           "will be placed.")) {
    return *bad;
  }
  WireState& w = s.ed->wire;
  const db::Rect old_leg = *s.ed->box;
  const std::optional<tech::TileType> old_type = w.type;
  const LegDirection old_dir = w.last_direction;
  if (type) {
    pick_type(s, type, width);
  } else if (auto picked = pick_type_at(s, std::nullopt); !picked.ok()) {
    for (const diag::Diagnostic& d : picked.diagnostics) {
      s.ctx->emit(d);  // reported, then the type comparison below fails
    }
  }
  if (w.type == old_type) {
    return fail(codes::kNoContact,
                "The new wiring layer is the same as the old one, so\n    there's no need for a contact.");
  }
  const auto contact = old_type ? contact_between(s.section(), *old_type, *w.type) : std::nullopt;
  const auto plan = contact ? plan_contact(*contact, *old_type, old_leg, old_dir, w.width,
                                           db::invert(s.ed->edit_transform))
                            : std::nullopt;
  if (!plan) {
    return fail(codes::kNoContact, "The technology file doesn't define a contact\n    between \"" +
                                       type_name(s, old_type) + "\" and \"" + type_name(s, w.type) + "\".");
  }
  tech::TypeMask layers;
  for (const ContactPaint& p : plan->paint) {
    if (auto st = db::paint(*s.ed->edit_cell, *s.tech, p.area, p.type, paint_options(s)); !st.ok()) {
      return cmd::CommandResult::failure(st.error());
    }
    layers.set(p.type);
  }
  finish_paint(s, plan->changed, layers);
  clear_selection(s);
  const View v{s.edit_root(), nullptr};
  for (const ContactPaint& p : plan->select) {
    select_area(s, v, p.area, tech::TypeMask::of(p.type));
  }
  s.ed->box = plan->box;
  s.ed->box_root = s.edit_root();
  return cmd::CommandResult::success();
}

}  // namespace hefesto::wiring
