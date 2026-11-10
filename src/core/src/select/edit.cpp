// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Editing the layout through the selection (cmd-selection): the edit-cell
// restriction, delete, copy, keep, transforms, stretch and array. Every
// operation builds a scratch cell in root coordinates (Magic's Select2Def),
// erases the edit-cell parts of the selection and paints the scratch back
// into the edit cell, which becomes the new selection. Owner: SELECT.
// (Magic ref: select/selOps.c, select/selEnum.c, select/selCreate.c
// SelectAndCopy1/2)

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/drc/engine/marking.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/drc/engine/marking.hpp"
#include "hefesto/editor/edit_frame.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "select/ops.hpp"

namespace hefesto::select::ops {

namespace cu = db::cmdutil;

namespace {

[[nodiscard]] db::Rect clip(const db::Rect& a, const db::Rect& b) noexcept {
  return db::Rect{std::max(a.x0, b.x0), std::max(a.y0, b.y0), std::min(a.x1, b.x1), std::min(a.y1, b.y1)};
}

db::PaintOptions paint_options(const Context& c) {
  db::PaintOptions o;
  o.locked = c.ed->locked_types;
  o.magic_compat = c.s.magic_compat;
  return o;
}

db::Transform root_to_edit(const Context& c) {
  return db::invert(c.ed->edit_transform);
}

/// One piece of selected paint (root coordinates). `triangle` is set for a
/// split tile; `area` is then the tile's bounding rectangle.
struct PaintPiece {
  tech::PlaneId plane = 0;
  db::Rect area;
  tech::TileType type = tech::kSpace;
  std::optional<db::TriangleSide> triangle;
};

/// Every tile of the selection (contacts only on their home plane, as
/// SelEnumPaint reports each piece once).
std::vector<PaintPiece> selected_paint(const Context& c) {
  std::vector<PaintPiece> out;
  const db::CellDef& sel = *c.ed->selection.def();
  for (std::size_t p = tech::kFirstUserPlane; p < sel.plane_count(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    sel.plane(plane).enumerate(db::whole_plane(), paint_types(*c.tech), [&](const db::TileView& t) {
      const auto home = c.tech->home_plane(t.type);
      if (home && *home != plane) {
        return true;
      }
      PaintPiece piece{plane, t.area, t.type, std::nullopt};
      if (t.split) {
        piece.triangle = db::TriangleSide{t.diagonal, t.right_side};
      }
      out.push_back(piece);
      return true;
    });
  }
  return out;
}

/// Magic's DBResidueMask: the residues of a contact (the component contacts
/// of a stacked type), the type itself otherwise.
tech::TypeMask residue_mask(const tech::Technology& tech, tech::TileType type) {
  const tech::ContactInfo* info = tech.contact(type);
  return info != nullptr ? info->residues : tech::TypeMask::of(type);
}

/// The residue of the stacked type `type` whose home plane is `plane`.
std::optional<tech::TileType> stacked_residue(const tech::Technology& tech, tech::TileType type, tech::PlaneId plane) {
  const tech::TypeMask residues = residue_mask(tech, type);
  for (std::size_t r = tech::kFirstUserType; r < tech.user_type_end(); ++r) {
    const auto rt = static_cast<tech::TileType>(r);
    if (residues.test(rt) && tech.home_plane(rt) == plane) {
      return rt;
    }
  }
  return std::nullopt;
}

/// True when the edit-cell type `have` under a selected `want` is a real
/// conflict (selEnumPFunc2's compatibility test: painting `have` over
/// `want` must give `want`, or a stacked type whose residues are both).
bool conflicts(const tech::Technology& tech, tech::PlaneId plane, tech::TileType have, tech::TileType want) {
  const tech::TileType chk = tech.paint_result(plane, want, have);
  const tech::TypeMask cmask = residue_mask(tech, chk);
  return chk < tech.user_type_end() || (have != chk && !cmask.test(have)) || !cmask.test(want);
}

/// The type selEnumPFunc2 reports for the edit-cell tile type `have` under
/// a selected piece of `want` on `plane`, or nothing when the tile is
/// skipped; sets `non_edit` when the skipped tile conflicts with `want`.
std::optional<tech::TileType> matched_type(const Context& c, tech::PlaneId plane, tech::TileType have,
                                           tech::TileType want, bool& non_edit) {
  const tech::Technology& tech = *c.tech;
  if (have != want && have >= tech.user_type_end()) {
    have = stacked_residue(tech, have, plane).value_or(have);
  }
  const tech::TypeMask& asked = c.ed->selection.area_types;
  if (have != want && !asked.empty() && !asked.test(have)) {
    return std::nullopt;  // `select area <types>`: other types are not part of it
  }
  const bool space_under = have != want && have == tech::kSpace && tech.home_plane_types(plane).test(want);
  if (space_under || tech.paint_result(plane, want, have) != want) {
    non_edit = non_edit || space_under || conflicts(tech, plane, have, want);
    return std::nullopt;
  }
  if (!tech.is_contact(have)) {
    return have;
  }
  if (have < tech.user_type_end()) {
    return tech.home_plane(have) == plane ? std::optional<tech::TileType>{have} : std::nullopt;
  }
  return stacked_residue(tech, have, plane);
}

/// The parts of `piece` present in the edit cell, in root coordinates
/// (selEnumPFunc1/2); sets `non_edit` when some of it is not.
std::vector<PaintPiece> edit_pieces(const Context& c, const PaintPiece& piece, bool& non_edit) {
  std::vector<PaintPiece> out;
  const db::CellDef& edit = *c.ed->edit_cell;
  const db::Transform to_edit = root_to_edit(c);
  const db::Rect er = db::apply(to_edit, piece.area);
  if (piece.triangle) {
    const tech::TypeMask mask = tech::TypeMask::of(piece.type);
    const db::TriangleSide et = db::transform_triangle(to_edit, piece.triangle->diagonal, piece.triangle->right_side);
    const bool found = !edit.plane(piece.plane).enumerate_triangle(er, et.diagonal, et.right_side, mask,
                                                                   [](const db::TileView&) { return false; });
    if (found) {
      out.push_back(piece);
    } else {
      non_edit = true;
    }
    return out;
  }
  edit.plane(piece.plane).enumerate(er, tech::TypeMask::all(), [&](const db::TileView& t) {
    if (t.split) {
      non_edit = true;  // a triangle under a Manhattan piece is left alone
      return true;
    }
    if (const auto type = matched_type(c, piece.plane, t.type, piece.type, non_edit)) {
      out.push_back(PaintPiece{piece.plane, db::apply(c.ed->edit_transform, clip(t.area, er)), *type, std::nullopt});
    }
    return true;
  });
  return out;
}

/// The edit-cell label matching a selected one (same rectangle and text).
const db::Label* edit_label(const Context& c, const db::Label& selected) {
  const db::Rect er = db::apply(root_to_edit(c), selected.rect);
  for (const db::Label& l : c.ed->edit_cell->labels()) {
    if (l.rect == er && l.text == selected.text) {
      return &l;
    }
  }
  return nullptr;
}

/// Adds a use to a scratch cell (not linked into the child's instances).
db::CellUse* add_scratch_use(db::CellDef& dest, db::CellDef& child, const std::string& id, const db::Transform& t,
                             const db::ArrayInfo& array, bool locked) {
  for (const auto& u : dest.uses()) {
    if (u->def == &child && u->transform == t && u->array == array) {
      return nullptr;  // DBCellFindDup
    }
  }
  auto use = std::make_unique<db::CellUse>();
  use->id = id;
  use->def = &child;
  use->parent = &dest;
  use->transform = t;
  use->array = array;
  use->locked = locked;
  db::compute_use_bbox(*use);
  db::CellUse* raw = use.get();
  dest.uses().push_back(std::move(use));
  return raw;
}

void paint_piece(db::CellDef& dest, const tech::Technology& tech, const PaintPiece& piece, const db::Transform& t) {
  std::optional<db::TriangleSpec> tri;
  if (piece.triangle) {
    const db::TriangleSide s = db::transform_triangle(t, piece.triangle->diagonal, piece.triangle->right_side);
    tri = db::TriangleSpec{s.diagonal, s.right_side};
  }
  (void)db::paint_type(dest, tech, db::apply(t, piece.area), piece.type, tri);
}

/// The edit-cell uses of the selection: (selected use, layout use).
struct UsePair {
  const db::CellUse* selected = nullptr;
  UseRef layout;
};

std::vector<UsePair> edit_uses(const Context& c, NonEdit* non_edit) {
  std::vector<UsePair> out;
  for (const auto& u : c.ed->selection.def()->uses()) {
    auto found = layout_use(c, *u);
    if (!found || found->use->parent != c.ed->edit_cell) {
      if (non_edit != nullptr) {
        non_edit->cells = true;
      }
      continue;
    }
    out.push_back(UsePair{u.get(), std::move(*found)});
  }
  return out;
}

bool editable(const Context& c) {
  if (c.ed->edit_cell == nullptr || c.ed->edit_cell->flags().no_edit) {
    cu::warn(c.s, "The current cell is not editable.");
    return false;
  }
  return true;
}

void finish_edit(const Context& c, const db::Rect& root_area) {
  db::CellDef& edit = *c.ed->edit_cell;
  const db::Rect area = db::apply(root_to_edit(c), root_area);
  // SelectAndCopy1/2 call DBAdjustLabels, which prints its "Moving label"
  // and "Adjusting size of label" notes (CMDEDIT-SEL-16).
  std::vector<std::string> notes;
  (void)db::adjust_labels(edit, *c.tech, area, &notes);
  for (std::string& n : notes) {
    cu::print(c.s, std::move(n));
  }
  db::recompute_bbox(edit, c.s.magic_compat);
  c.ed->windows.area_changed(edit, area, std::nullopt);
}

// ---- array labels -------------------------------------------------------------

/// nmGetNums: the first two numbers in `text` (-1 when absent).
std::pair<int, int> label_numbers(std::string_view text) {
  int nums[2] = {-1, -1};
  int found = 0;
  std::size_t i = 0;
  while (i < text.size() && found < 2) {
    if (std::isdigit(static_cast<unsigned char>(text[i])) == 0) {
      ++i;
      continue;
    }
    int n = 0;
    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])) != 0) {
      n = 10 * n + (text[i] - '0');
      ++i;
    }
    nums[found++] = n;
  }
  return {nums[0], nums[1]};
}

/// nmPutNums: `text` with its first two numbers replaced (a negative
/// number keeps the original digits).
std::string put_numbers(std::string_view text, int n1, int n2) {
  std::string out;
  std::size_t i = 0;
  for (const int n : {n1, n2}) {
    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])) == 0) {
      out += text[i++];
    }
    if (i == text.size()) {
      return out;
    }
    const std::size_t start = i;
    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])) != 0) {
      ++i;
    }
    out += n < 0 ? std::string{text.substr(start, i - start)} : std::to_string(n);
  }
  out += text.substr(i);
  return out;
}

void array_label(db::CellDef& dest, const db::Label& root_label, const db::ArrayInfo& info) {
  const int nx = std::abs(info.xhi - info.xlo);
  const int ny = std::abs(info.yhi - info.ylo);
  const auto [labx, laby] = label_numbers(root_label.text);
  const int only1 = nx > 0 && ny > 0 ? 0 : 1;
  int xi = 0;
  for (int i = 0; i <= nx; ++i) {
    for (int j = 0; j <= ny; ++j) {
      db::Label copy = root_label;
      copy.rect = db::apply(db::translation(i * info.xsep, j * info.ysep), root_label.rect);
      copy.text = put_numbers(root_label.text, labx + xi, laby + j);
      (void)cu::erase_labels_by_content(dest, copy.rect, copy.text);
      dest.labels().push_back(copy);
      xi += only1;
    }
    xi += 1 - only1;
  }
}

}  // namespace

// ---- shared ---------------------------------------------------------------------

void warn_non_edit(const Context& c, const NonEdit& n, std::string_view verb) {
  const std::string v{verb};
  if (n.paint) {
    cu::warn(c.s, "You selected paint outside the edit cell.  Only\n    the paint in the edit cell was " + v + ".");
  }
  if (n.cells) {
    cu::warn(c.s, "You selected one or more subcells that aren't children\n    of the edit cell.  Only those in "
                  "the edit cell were\n    " + v + ".");
  }
  if (n.labels) {
    cu::warn(c.s, "You selected one or more labels that aren't in the\n    edit cell.  Only the label(s) in the "
                  "edit cell\n    were " + v + ".");
  }
}

std::unique_ptr<db::CellDef> scratch(const Context& c) {
  auto def = std::make_unique<db::CellDef>("__SELECT2__", c.tech->plane_count());
  def->flags().internal = true;
  return def;
}

std::optional<UseRef> layout_use(const Context& c, const db::CellUse& selected) {
  std::optional<UseRef> found;
  const auto visit = [&](const auto& self, const db::CellDef& def, const db::Transform& to_root,
                         const std::string& path) -> bool {
    for (db::CellUse* u : db::uses_in_search_order(def)) {
      if (u->def == nullptr) {
        continue;
      }
      if (u->def == selected.def && db::compose(to_root, u->transform) == selected.transform &&
          u->array == selected.array) {
        found = UseRef{u, to_root, connect::child_path(path, *u, u->array.xlo, u->array.ylo),
                       db::Point{u->array.xlo, u->array.ylo}};
        return false;
      }
      const db::Transform et = db::element_transform(*u, u->array.xlo, u->array.ylo);
      if (!self(self, *u->def, db::compose(to_root, et), connect::child_path(path, *u, u->array.xlo, u->array.ylo))) {
        return false;
      }
    }
    return true;
  };
  (void)visit(visit, *c.root, db::kIdentity, std::string{});
  return found;
}

std::unique_ptr<db::CellDef> edit_parts(const Context& c, const db::Transform& t, NonEdit* non_edit) {
  auto dest = scratch(c);
  NonEdit local;
  for (const PaintPiece& piece : selected_paint(c)) {
    for (const PaintPiece& part : edit_pieces(c, piece, local.paint)) {
      paint_piece(*dest, *c.tech, part, t);
    }
  }
  for (const UsePair& p : edit_uses(c, &local)) {
    if (p.selected->locked) {
      continue;
    }
    (void)add_scratch_use(*dest, *p.selected->def, p.layout.use->id, db::compose(t, p.selected->transform),
                          p.selected->array, p.selected->locked);
  }
  for (const db::Label& l : c.ed->selection.def()->labels()) {
    if (edit_label(c, l) == nullptr) {
      local.labels = true;
      continue;
    }
    dest->labels().push_back(connect::transform_label(l, t, c.s.magic_compat));
  }
  db::recompute_bbox(*dest);
  if (non_edit != nullptr) {
    *non_edit = local;
  }
  return dest;
}

std::unique_ptr<db::CellDef> whole_selection(const Context& c, const db::Transform& t) {
  auto dest = scratch(c);
  const Selection& sel = c.ed->selection;
  const auto& off = sel.floating_offset();
  const db::Transform full = off ? db::compose(t, db::translation(off->x, off->y)) : t;
  (void)ops::copy_paint(*sel.def(), db::whole_plane(), full, *dest, *c.tech, paint_types(*c.tech));
  for (const db::Label& l : sel.def()->labels()) {
    db::Label copy = connect::transform_label(l, full, c.s.magic_compat);
    (void)cu::erase_labels_by_content(*dest, copy.rect, copy.text);
    dest->labels().push_back(copy);
  }
  for (const auto& u : sel.def()->uses()) {
    (void)add_scratch_use(*dest, *u->def, u->id, db::compose(full, u->transform), u->array, u->locked);
  }
  db::recompute_bbox(*dest);
  return dest;
}

void delete_selection(const Context& c, std::string_view verb, bool clear) {
  if (!editable(c)) {
    return;
  }
  db::CellDef& edit = *c.ed->edit_cell;
  NonEdit n;
  const db::Transform to_edit = root_to_edit(c);
  std::vector<PaintPiece> doomed;
  for (const PaintPiece& piece : selected_paint(c)) {
    const auto parts = edit_pieces(c, piece, n.paint);
    doomed.insert(doomed.end(), parts.begin(), parts.end());
  }
  for (const PaintPiece& part : doomed) {
    const db::Rect er = db::apply(to_edit, part.area);
    if (part.triangle) {
      const db::TriangleSide s = db::transform_triangle(to_edit, part.triangle->diagonal, part.triangle->right_side);
      (void)db::erase_type(edit, *c.tech, er, part.type, db::TriangleSpec{s.diagonal, s.right_side});
    } else {
      (void)db::erase(edit, *c.tech, er, tech::TypeMask::of(part.type), paint_options(c));
    }
  }
  for (const UsePair& p : edit_uses(c, &n)) {
    if (!p.layout.use->locked) {
      (void)db::remove_use(*p.layout.use);
    }
  }
  for (const db::Label& l : c.ed->selection.def()->labels()) {
    if (edit_label(c, l) == nullptr) {
      n.labels = true;
    } else {
      (void)cu::erase_labels_by_content(edit, db::apply(to_edit, l.rect), l.text);
    }
  }
  warn_non_edit(c, n, verb);
  db::CellDef& sel = *c.ed->selection.def();
  db::recompute_bbox(sel);
  db::recompute_bbox(edit, c.s.magic_compat);
  c.ed->windows.area_changed(edit, db::apply(to_edit, sel.extended_bbox()), std::nullopt);
  drc::check_this(*c.ed, edit, db::apply(to_edit, sel.extended_bbox()));  // SelectDelete
  if (clear) {
    c.ed->selection.clear();
  }
}

namespace {

/// Paints the scratch cell's paint, labels and uses into the edit cell.
/// Returns the placed uses with their root transforms.
std::vector<std::pair<db::CellUse*, db::Transform>> paint_into_edit(const Context& c, const db::CellDef& src) {
  db::CellDef& edit = *c.ed->edit_cell;
  const db::Transform to_edit = root_to_edit(c);
  for (std::size_t p = tech::kFirstUserPlane; p < src.plane_count(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    src.plane(plane).enumerate(db::whole_plane(), paint_types(*c.tech), [&](const db::TileView& t) {
      const auto home = c.tech->home_plane(t.type);
      if (home && *home != plane) {
        return true;
      }
      const db::Rect er = db::apply(to_edit, t.area);
      if (t.split) {
        const db::TriangleSide s = db::transform_triangle(to_edit, t.diagonal, t.right_side);
        (void)db::paint_triangle(edit, *c.tech, er, s.diagonal, s.right_side, t.type, paint_options(c));
      } else {
        (void)db::paint(edit, *c.tech, er, t.type, paint_options(c));
      }
      return true;
    });
  }
  for (const db::Label& l : src.labels()) {
    db::Label copy = connect::transform_label(l, to_edit, c.s.magic_compat);
    (void)cu::erase_labels_by_content(edit, copy.rect, copy.text);
    (void)db::put_label(edit, copy);
  }
  std::vector<std::pair<db::CellUse*, db::Transform>> placed;
  for (const auto& u : src.uses()) {
    db::PlaceOptions o;
    if (db::find_use(edit, u->id) == nullptr) {
      o.id = u->id;  // a clash gets a generated identifier (DBSelectionUniqueIds)
    }
    o.transform = db::compose(to_edit, u->transform);
    o.array = u->array;
    o.locked = u->locked;
    auto r = db::place(edit, *u->def, o);
    if (r.ok()) {
      placed.emplace_back(r.value(), u->transform);
    } else {
      cu::warn(c.s, r.error().text);
    }
  }
  return placed;
}

}  // namespace

void copy_into_edit(const Context& c, const db::CellDef& src) {
  db::CellDef& edit = *c.ed->edit_cell;
  const auto placed = paint_into_edit(c, src);
  finish_edit(c, src.extended_bbox());
  drc::check_this(*c.ed, edit, db::apply(root_to_edit(c), src.bbox()));  // SelectAndCopy2
  // The new selection: the edit cell's paint under the scratch paint
  // (selACPaintFunc), the scratch labels and the placed uses.
  Selection& sel = c.ed->selection;
  sel.set_root(c.ed->edit_root != nullptr ? c.ed->edit_root : &edit, c.tech->plane_count());
  sel.clear();
  db::CellDef& dest = *sel.def();
  const db::Transform to_edit = root_to_edit(c);
  for (std::size_t p = tech::kFirstUserPlane; p < src.plane_count(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    src.plane(plane).enumerate(db::whole_plane(), paint_types(*c.tech), [&](const db::TileView& t) {
      if (t.split) {
        (void)dest.plane(plane).paint_triangle(t.area, t.diagonal, t.right_side,
                                               db::paint_row(*c.tech, plane, t.type));
        return true;
      }
      const db::Rect er = db::apply(to_edit, t.area);
      edit.plane(plane).enumerate(er, paint_types(*c.tech), [&](const db::TileView& e) {
        if (!e.split) {
          const db::Rect part = db::apply(c.ed->edit_transform, clip(e.area, er));
          (void)dest.plane(plane).paint(part, db::paint_row(*c.tech, plane, e.type));
        }
        return true;
      });
      return true;
    });
  }
  for (const db::Label& l : src.labels()) {
    (void)cu::erase_labels_by_content(dest, l.rect, l.text);
    dest.labels().push_back(l);
  }
  for (const auto& [use, root_t] : placed) {
    (void)sel.add_use(*use->def, use->id, root_t, use->array, use->locked);
  }
  db::recompute_bbox(dest);
}

void keep_selection(const Context& c) {
  if (!editable(c)) {
    return;
  }
  auto all = whole_selection(c, db::kIdentity);
  (void)paint_into_edit(c, *all);
  finish_edit(c, all->extended_bbox());
  drc::check_this(*c.ed, *c.ed->edit_cell, db::apply(root_to_edit(c), all->bbox()));  // SelectAndCopy1
}

void transform_selection(const Context& c, const db::Transform& t) {
  if (!editable(c)) {
    return;
  }
  auto moved = edit_parts(c, t, nullptr);
  delete_selection(c, "modified", true);
  copy_into_edit(c, *moved);
}

void array_selection(const Context& c, const db::ArrayInfo& info) {
  if (!editable(c)) {
    return;
  }
  auto dest = scratch(c);
  bool ignore = false;
  const int nx = std::abs(info.xhi - info.xlo);
  const int ny = std::abs(info.yhi - info.ylo);
  for (const PaintPiece& piece : selected_paint(c)) {
    for (const PaintPiece& part : edit_pieces(c, piece, ignore)) {
      for (int i = 0; i <= nx; ++i) {
        for (int j = 0; j <= ny; ++j) {
          paint_piece(*dest, *c.tech, part, db::translation(i * info.xsep, j * info.ysep));
        }
      }
    }
  }
  for (const UsePair& p : edit_uses(c, nullptr)) {
    const db::Transform t = db::compose(p.layout.parent_to_root, p.layout.use->transform);
    db::CellUse* nu = add_scratch_use(*dest, *p.layout.use->def, p.layout.use->id, t, {}, p.layout.use->locked);
    if (nu == nullptr) {
      continue;
    }
    db::make_array(*nu, db::invert(t), info);
    const db::Rect old_bbox = db::apply(p.layout.parent_to_root, p.layout.use->bbox);
    nu->transform = db::compose(db::translation(old_bbox.x0 - nu->bbox.x0, old_bbox.y0 - nu->bbox.y0), nu->transform);
    db::compute_use_bbox(*nu);
  }
  for (const db::Label& l : c.ed->selection.def()->labels()) {
    if (edit_label(c, l) != nullptr) {
      array_label(*dest, l, info);
    }
  }
  db::recompute_bbox(*dest);
  delete_selection(c, "arrayed", true);
  copy_into_edit(c, *dest);
}

// ---- stretch ----------------------------------------------------------------------

namespace {

/// The material that `piece` (displaced) sweeps through is cleared.
void stretch_erase(const Context& c, const PaintPiece& piece, db::Coord dx, db::Coord dy) {
  db::Rect area = piece.area;
  (dx > 0 ? area.x0 : area.x1) -= dx;
  (dy > 0 ? area.y0 : area.y1) -= dy;
  db::CellDef& edit = *c.ed->edit_cell;
  const db::Rect er = db::apply(root_to_edit(c), area);
  const tech::TileType type = piece.type;
  // Non-contact types of the plane are erased; the same contact is erased;
  // other contacts are reduced to their residues.
  std::vector<tech::TileType> residues;
  for (std::size_t u = tech::kFirstUserType; u < c.tech->user_type_end(); ++u) {
    const auto t = static_cast<tech::TileType>(u);
    if (!c.tech->plane_types(piece.plane).test(t) || c.ed->locked_types.test(t)) {
      continue;
    }
    if (!c.tech->is_contact(t)) {
      (void)db::erase_type(edit, *c.tech, er, t);
    } else if (t == type) {
      (void)db::erase_type(edit, *c.tech, er, t);
    } else {
      const bool present = !edit.plane(piece.plane).enumerate(er, tech::TypeMask::of(t),
                                                              [](const db::TileView&) { return false; });
      if (present) {
        residues.push_back(c.tech->residue_on(t, piece.plane));
      }
    }
  }
  for (const tech::TileType r : residues) {
    (void)db::erase_type(edit, *c.tech, er, r);
  }
}

struct StretchFill {
  db::Rect area;  // edit coordinates
  tech::TileType type = tech::kSpace;
};

/// The 1-unit strip behind a displaced piece's trailing edge.
db::Rect trailing_strip(const db::Rect& r, db::Coord dx, db::Coord dy) {
  db::Rect s = r;
  if (dx > 0) {
    s.x1 = s.x0;
    s.x0 -= 1;
  } else if (dx < 0) {
    s.x0 = s.x1;
    s.x1 += 1;
  } else if (dy > 0) {
    s.y1 = s.y0;
    s.y0 -= 1;
  } else {
    s.y0 = s.y1;
    s.y1 += 1;
  }
  return s;
}

/// The gap between the old trailing edge and the new position.
db::Rect gap_behind(db::Rect r, db::Coord dx, db::Coord dy) {
  if (dx > 0) {
    r.x0 = r.x1;
    r.x1 += dx;
  } else if (dx < 0) {
    r.x1 = r.x0;
    r.x0 += dx;
  } else if (dy > 0) {
    r.y0 = r.y1;
    r.y1 += dy;
  } else {
    r.y1 = r.y0;
    r.y0 += dy;
  }
  return r;
}

void stretch_fill(const Context& c, const db::CellDef& moved, const PaintPiece& piece, db::Coord dx, db::Coord dy,
                  std::vector<StretchFill>& fills) {
  const db::Rect strip = trailing_strip(piece.area, dx, dy);
  const db::CellDef& edit = *c.ed->edit_cell;
  tech::TypeMask active = paint_types(*c.tech);
  active &= ~c.ed->locked_types;
  moved.plane(piece.plane).enumerate(strip, tech::TypeMask::of(tech::kSpace), [&](const db::TileView& space) {
    const db::Rect behind = db::apply(db::translation(-dx, -dy), clip(space.area, strip));
    const db::Rect eb = db::apply(root_to_edit(c), behind);
    for (std::size_t p = tech::kFirstUserPlane; p < edit.plane_count(); ++p) {
      const auto plane = static_cast<tech::PlaneId>(p);
      if ((c.tech->type_planes(piece.type) & tech::plane_bit(plane)) == 0) {
        continue;
      }
      edit.plane(plane).enumerate(eb, active, [&](const db::TileView& t) {
        if (t.split) {
          return true;
        }
        const db::Rect root = clip(db::apply(c.ed->edit_transform, t.area), behind);
        tech::TileType type = t.type;
        if (!c.tech->full_residues(piece.type).test(type)) {
          type = piece.type;
        }
        fills.push_back(StretchFill{db::apply(root_to_edit(c), gap_behind(root, dx, dy)), type});
        return true;
      });
    }
    return true;
  });
}

}  // namespace

void stretch_selection(const Context& c, db::Coord dx, db::Coord dy) {
  if ((dx == 0 && dy == 0) || !editable(c)) {
    return;
  }
  auto moved = edit_parts(c, db::translation(dx, dy), nullptr);
  // SelectStretch: everything between the old and new selection changes.
  const db::Rect modified =
      db::apply(root_to_edit(c), db::include(moved->extended_bbox(), c.ed->selection.def()->extended_bbox()));
  delete_selection(c, "stretched", true);
  std::vector<PaintPiece> pieces;
  for (std::size_t p = tech::kFirstUserPlane; p < moved->plane_count(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    moved->plane(plane).enumerate(db::whole_plane(), paint_types(*c.tech), [&](const db::TileView& t) {
      if (!t.split) {
        pieces.push_back(PaintPiece{plane, t.area, t.type, std::nullopt});
      }
      return true;
    });
  }
  for (const PaintPiece& piece : pieces) {
    stretch_erase(c, piece, dx, dy);
  }
  std::vector<StretchFill> fills;
  for (const PaintPiece& piece : pieces) {
    stretch_fill(c, *moved, piece, dx, dy, fills);
  }
  db::CellDef& edit = *c.ed->edit_cell;
  for (auto it = fills.rbegin(); it != fills.rend(); ++it) {
    (void)db::paint(edit, *c.tech, it->area, it->type, paint_options(c));
  }
  copy_into_edit(c, *moved);
  drc::check_this(*c.ed, edit, modified);
}

}  // namespace hefesto::select::ops
