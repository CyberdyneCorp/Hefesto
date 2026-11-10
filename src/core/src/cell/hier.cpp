// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Hierarchy editing helpers (hefesto/cell/hier.hpp).
// Ported from Magic VLSI 8.3.694 (fc5daa9e) database/DBlabel2.c
// (DBTreeFindUse, dbParseArray), commands/CmdE.c (cmdEditEnumFunc),
// select/selEnum.c (SelEnumCells, SelEnumLabelsBase, selEnumLFunc,
// selEnumLFunc2), dbwind/DBWundo.c (DBWUndoOldEdit, DBWUndoNewEdit) and
// commands/CmdSubrs.c (CmdIllegalChars). Magic is Copyright (C) 1985, 1990
// Regents of the University of California (permissive license,
// THIRD_PARTY_NOTICES.md).

#include "hefesto/cell/hier.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <memory>
#include <utility>

#include "hefesto/db/hierarchy.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/undo/undo_log.hpp"

namespace hefesto::cell {


// CmdIllegalChars: control and non-ASCII characters, then the listed ones.
std::optional<std::string> illegal_chars(std::string_view text, std::string_view bad, std::string_view what) {
  for (const char ch : text) {
    const auto c = static_cast<unsigned char>(ch);
    if (c > 127 || std::iscntrl(c) != 0) {
      std::array<char, 8> hex{};
      (void)std::snprintf(hex.data(), hex.size(), "%x", static_cast<unsigned>(c));
      return std::string{what} + " contains illegal control character 0x" + hex.data();
    }
    if (bad.find(ch) != std::string_view::npos) {
      return std::string{what} + " contains illegal character \"" + ch + "\"";
    }
  }
  return std::nullopt;
}

namespace {

// --- DBTreeFindUse ---------------------------------------------------------------

struct Subscript {
  int count = 0;  // 0, 1 or 2 indices given
  std::int32_t first = 0;
  std::int32_t second = 0;
  std::size_t length = 0;  // characters consumed
};

// One "[%d]" group at the head of `text`; nullopt when it is not one.
std::optional<std::pair<std::int32_t, std::size_t>> bracket_int(std::string_view text) {
  if (text.empty() || text.front() != '[') {
    return std::nullopt;
  }
  const std::size_t close = text.find(']');
  if (close == std::string_view::npos || close == 1) {
    return std::nullopt;
  }
  const std::string inner{text.substr(1, close - 1)};
  int value = 0;
  int used = 0;
  if (std::sscanf(inner.c_str(), "%d%n", &value, &used) != 1 || static_cast<std::size_t>(used) != inner.size()) {
    return std::nullopt;
  }
  return std::pair{value, close + 1};
}

// dbParseArray's subscript forms "[i][j]", "[i,j]" and "[i]".
Subscript parse_subscript(std::string_view text) {
  Subscript s;
  if (text.empty() || text.front() != '[') {
    return s;
  }
  if (const auto a = bracket_int(text)) {
    s = Subscript{1, a->first, 0, a->second};
    if (const auto b = bracket_int(text.substr(a->second))) {
      s = Subscript{2, a->first, b->first, a->second + b->second};
    }
    return s;
  }
  int i1 = 0;
  int i2 = 0;
  int used = 0;
  const std::string head{text.substr(0, text.find(']') == std::string_view::npos ? text.size() : text.find(']') + 1)};
  if (std::sscanf(head.c_str(), "[%d,%d]%n", &i1, &i2, &used) == 2 && used > 0) {
    s = Subscript{2, i1, i2, static_cast<std::size_t>(used)};
  }
  return s;
}

bool in_span(std::int32_t lo, std::int32_t hi, std::int32_t v) {
  return lo <= hi ? (v >= lo && v <= hi) : (v <= lo && v >= hi);
}

// dbParseArray: the element of `use` that `sub` names; nullopt when the
// number of indices does not fit the array or an index is out of range.
std::optional<std::pair<std::int32_t, std::int32_t>> array_element(const db::CellUse& use, const Subscript& sub) {
  const db::ArrayInfo& a = use.array;
  std::int32_t x = a.xlo;
  std::int32_t y = a.ylo;
  if (sub.count == 0 && a.is_array()) {
    return std::nullopt;
  }
  if (sub.count == 1) {
    if (a.xlo == a.xhi) {
      y = sub.first;
    } else if (a.ylo == a.yhi) {
      x = sub.first;
    } else {
      return std::nullopt;
    }
  }
  if (sub.count == 2) {
    if (a.xlo == a.xhi || a.ylo == a.yhi) {
      return std::nullopt;
    }
    y = sub.first;
    x = sub.second;
  }
  if (!in_span(a.xlo, a.xhi, x) || !in_span(a.ylo, a.yhi, y)) {
    return std::nullopt;
  }
  return std::pair{x, y};
}

// The use of `def` the component at the head of `name` names: the whole
// remaining name as an identifier, else the part before '[' or '/'.
db::CellUse* component_use(const db::CellDef& def, std::string_view name, std::size_t& consumed) {
  for (const auto& u : def.uses()) {
    if (u->id == name) {
      consumed = name.size();
      return u.get();
    }
  }
  consumed = std::min(name.find_first_of("[/"), name.size());
  const std::string_view id = name.substr(0, consumed);
  for (const auto& u : def.uses()) {
    if (u->id == id) {
      return u.get();
    }
  }
  return nullptr;
}

void read_if_needed(db::CellDef& def, const CellReader& read) {
  if (!def.flags().available && read) {
    read(def);
  }
}

// One step of DBTreeFindUse: resolves the component at the head of `name`
// below `ref`, advancing `name` past it and its '/'. False when not found.
bool descend_component(InstanceRef& ref, db::CellDef*& def, std::string_view& name) {
  std::size_t consumed = 0;
  db::CellUse* use = component_use(*def, name, consumed);
  if (use == nullptr) {
    return false;
  }
  std::string_view rest = name.substr(consumed);
  const Subscript sub = parse_subscript(rest);
  const bool valid = sub.count == 0 || rest.size() == sub.length || rest[sub.length] == '/';
  auto element = valid ? array_element(*use, sub) : std::nullopt;
  if (!element) {
    // Allow a non-indexed match of an array: its first element.
    if (name != use->id) {
      return false;
    }
    element = std::pair{use->array.xlo, use->array.ylo};
    rest = {};
  } else {
    rest.remove_prefix(sub.length);
  }
  const std::size_t slash = rest.find('/');
  name = slash == std::string_view::npos ? std::string_view{} : rest.substr(slash + 1);
  ref.parent_to_root = ref.to_root;
  ref.to_root = db::compose(ref.to_root, db::element_transform(*use, element->first, element->second));
  ref.use = use;
  ref.x = element->first;
  ref.y = element->second;
  def = use->def;
  return true;
}

// --- SelEnumCells ------------------------------------------------------------------

// The layout use below `def` (whose coordinates map to the root by
// `to_root`) that the selection's copy `sel` stands for.
std::optional<InstanceRef> find_layout_use(db::CellDef& def, const db::Transform& to_root, const db::CellUse& sel,
                                           const std::string& prefix) {
  for (const auto& u : def.uses()) {
    if (u->def == sel.def && u->id == sel.id && u->array == sel.array &&
        db::compose(to_root, u->transform) == sel.transform) {
      return InstanceRef{u.get(), db::compose(to_root, u->transform), to_root, u->array.xlo, u->array.ylo,
                         prefix + u->id};
    }
  }
  for (const auto& u : def.uses()) {
    if (u->def == nullptr || u->array.is_array()) {
      continue;  // selected subcells are reached through single instances
    }
    if (auto found = find_layout_use(*u->def, db::compose(to_root, u->transform), sel, prefix + u->id + "/")) {
      return found;
    }
  }
  return std::nullopt;
}

// --- SelEnumLabels -----------------------------------------------------------------

std::optional<std::size_t> label_index(const db::CellDef& def, const db::Label* label) {
  for (std::size_t i = 0; i < def.labels().size(); ++i) {
    if (&def.labels()[i] == label) {
      return i;
    }
  }
  return std::nullopt;
}

// selEnumLFunc (`loose` false) / selEnumLFunc2 (`loose` true).
bool label_matches(const db::LabelHit& hit, const db::Label& want, bool loose) {
  if (db::apply(hit.to_root, hit.label->rect) != want.rect) {
    return false;
  }
  int mismatch = 0;
  mismatch += db::transform_position(hit.to_root, hit.label->justification) != want.justification ? 1 : 0;
  mismatch += hit.label->text != want.text ? 1 : 0;
  return loose ? mismatch < 2 : mismatch == 0;
}

std::optional<db::LabelHit> find_layout_label(const db::CellDef& root, const db::Label& want) {
  for (const bool loose : {false, true}) {
    std::optional<db::LabelHit> found;
    db::search_labels(root, db::expand(want.rect, 1), [&](const db::LabelHit& hit) {
      if (label_matches(hit, want, loose)) {
        found = hit;
        return false;
      }
      return true;
    });
    if (found) {
      return found;
    }
  }
  return std::nullopt;
}

// --- edit cell undo -----------------------------------------------------------------

void apply_frame(editor::EditorState& ed, const EditFrame& f) {
  ed.edit_cell = f.cell;
  ed.edit_root = f.root;
  ed.edit_transform = f.to_root;
}

/// DBWUndoOldEdit + DBWUndoNewEdit as one event.
class EditCellEvent final : public undo::Event {
 public:
  EditCellEvent(EditFrame before, EditFrame after) : before_(before), after_(after) {}
  void undo(editor::EditorState& editor) override { apply_frame(editor, before_); }
  void redo(editor::EditorState& editor) override { apply_frame(editor, after_); }
  [[nodiscard]] std::string describe() const override {
    const auto name = [](const db::CellDef* d) { return d != nullptr ? d->name() : std::string{"(none)"}; };
    return "edit cell " + name(before_.cell) + " -> " + name(after_.cell);
  }

 private:
  EditFrame before_;
  EditFrame after_;
};

}  // namespace

std::optional<std::string> illegal_cell_name(std::string_view name) {
  return illegal_chars(name, "[],", "Cell name");
}

std::optional<std::string> illegal_use_id(std::string_view id) {
  return illegal_chars(id, ",/", "Cell use id");
}

std::optional<InstanceRef> find_instance(db::CellDef& root, std::string_view path, const CellReader& read) {
  InstanceRef ref;
  ref.path = std::string{path};
  db::CellDef* def = &root;
  std::string_view name = path;
  while (!name.empty()) {
    read_if_needed(*def, read);
    if (!descend_component(ref, def, name)) {
      return std::nullopt;
    }
  }
  if (ref.use == nullptr) {
    return std::nullopt;
  }
  read_if_needed(*ref.use->def, read);
  return ref;
}

std::vector<InstanceRef> selected_instances(editor::EditorState& editor) {
  std::vector<InstanceRef> out;
  db::CellDef* root = editor.selection.root();
  const db::CellDef* sel = editor.selection.def();
  if (root == nullptr || sel == nullptr) {
    return out;
  }
  for (const auto& u : sel->uses()) {
    if (auto found = find_layout_use(*root, db::kIdentity, *u, {})) {
      out.push_back(std::move(*found));
    }
  }
  return out;
}

std::optional<InstanceRef> choose_instance(const std::vector<InstanceRef>& candidates,
                                           const std::optional<db::Rect>& point_area) {
  if (candidates.empty()) {
    return std::nullopt;
  }
  if (point_area) {
    for (const InstanceRef& c : candidates) {
      // The point in the parent's coordinates against the use's area.
      const db::Rect in_parent = db::apply(db::invert(c.parent_to_root), *point_area);
      if (!db::overlap(in_parent, c.use->bbox)) {
        continue;
      }
      InstanceRef chosen = c;
      const db::ArrayInfo& a = c.use->array;
      for (std::int32_t y = std::min(a.ylo, a.yhi); y <= std::max(a.ylo, a.yhi); ++y) {
        for (std::int32_t x = std::min(a.xlo, a.xhi); x <= std::max(a.xlo, a.xhi); ++x) {
          const db::Transform et = db::element_transform(*c.use, x, y);
          if (db::overlap(in_parent, db::apply(et, c.use->def->bbox()))) {
            chosen.x = x;
            chosen.y = y;
            chosen.to_root = db::compose(c.parent_to_root, et);
            return chosen;
          }
        }
      }
      return chosen;
    }
  }
  return candidates.back();
}

std::vector<SelectedLabel> selected_edit_labels(editor::EditorState& editor, std::vector<std::string>* missing) {
  std::vector<SelectedLabel> out;
  db::CellDef* root = editor.selection.root();
  db::CellDef* sel = editor.selection.def();
  if (root == nullptr || sel == nullptr || editor.edit_cell == nullptr) {
    return out;
  }
  for (db::Label& want : sel->labels()) {
    const auto hit = find_layout_label(*root, want);
    if (!hit) {
      if (missing != nullptr) {
        missing->push_back(want.text);
      }
      continue;
    }
    if (const auto index = label_index(*editor.edit_cell, hit->label)) {
      out.push_back(SelectedLabel{editor.edit_cell, *index, hit->to_root, &want});
    }
  }
  return out;
}

EditFrame edit_frame(const editor::EditorState& editor) {
  return EditFrame{editor.edit_cell, editor.edit_root, editor.edit_transform};
}

void set_edit_frame(editor::EditorState& editor, const EditFrame& frame) {
  const EditFrame before = edit_frame(editor);
  if (before == frame) {
    return;
  }
  apply_frame(editor, frame);
  editor.undo.record(std::make_unique<EditCellEvent>(before, frame));
}

std::optional<std::string> edit_transform_warning(const editor::EditorState& editor, bool magic_compat) {
  const db::CellDef* cell = editor.edit_cell;
  const db::CellDef* root = editor.edit_root;
  if (!magic_compat || cell == nullptr || root == nullptr || root == cell) {
    return std::nullopt;
  }
  std::optional<std::string> first;
  bool matched = false;
  std::vector<std::string> path;
  db::walk_hierarchy(*root, [&](const db::CellUse& use, const db::Transform& to_root, int depth) {
    path.resize(static_cast<std::size_t>(depth));
    path.push_back(use.id);
    if (use.def == cell) {
      matched = matched || to_root == editor.edit_transform;
      if (!first) {
        std::string joined;
        for (const std::string& p : path) {
          joined += (joined.empty() ? "" : "/") + p;
        }
        first = joined;
      }
    }
    return !matched;
  });
  if (matched || !first) {
    return std::nullopt;
  }
  return "Warning: edit transform does not match instance " + *first +
         "; the box is mapped through the previous edit transform";
}

}  // namespace hefesto::cell
