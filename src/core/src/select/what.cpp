// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `what` and `identify`/`id` (cmd-selection "What command", "Identify
// command"). Owner: SELECT.
// (Magic ref: commands/CmdTZ.c:1092-1689 CmdWhat, commands/CmdFI.c:1553-1627
// CmdIdentify, commands/CmdSubrs.c:1343-1375 CmdIllegalChars,
// database/DBcellname.c:383-436 dbGetUseName)

#include <algorithm>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/cell/hier.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/use_index.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/select/diag_codes.hpp"
#include "select/commands_internal.hpp"
#include "select/ops.hpp"

namespace hefesto::select {

namespace cu = db::cmdutil;
using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;
using cmd::Value;
using ops::Context;

namespace {

/// The user types of the selected paint, stacked types replaced by their
/// residues.
tech::TypeMask selected_layers(const Context& c) {
  tech::TypeMask m;
  const db::CellDef& sel = *c.ed->selection.def();
  for (std::size_t p = tech::kFirstUserPlane; p < sel.plane_count(); ++p) {
    sel.plane(static_cast<tech::PlaneId>(p)).enumerate(db::whole_plane(), ops::paint_types(*c.tech),
                                                       [&](const db::TileView& t) {
                                                         m.set(t.type);
                                                         return true;
                                                       });
  }
  for (std::size_t t = c.tech->user_type_end(); t < c.tech->type_count(); ++t) {
    if (m.test(static_cast<tech::TileType>(t))) {
      m |= c.tech->full_residues(static_cast<tech::TileType>(t));
      m.clear_type(static_cast<tech::TileType>(t));
    }
  }
  return m;
}

/// The last component of an instance path, without its array subscript.
std::string use_id_of(const std::string& path) {
  std::string last = path.substr(path.rfind('/') == std::string::npos ? 0 : path.rfind('/') + 1);
  const std::size_t bracket = last.find('[');
  return bracket == std::string::npos ? last : last.substr(0, bracket);
}

/// cmdFindWhatTileFunc: the cells holding `type` under the selected tiles
/// (use identifiers, the top cell by its root use's identifier), most
/// recent first.
std::vector<std::string> cells_with(const Context& c, tech::TileType type) {
  std::vector<std::string> names;
  tech::TypeMask mask = tech::TypeMask::of(type);
  const db::CellDef& sel = *c.ed->selection.def();
  for (std::size_t p = tech::kFirstUserPlane; p < sel.plane_count(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    if (!c.tech->plane_types(plane).test(type)) {
      continue;
    }
    sel.plane(plane).enumerate(db::whole_plane(), mask, [&](const db::TileView& t) {
      (void)connect::search_tree_tiles(*c.root, t.area, mask, {}, c.s.magic_compat, [&](const connect::TreeTile& found) {
        const std::string name = found.path->empty() ? std::string{cell::kTopUseId} : use_id_of(*found.path);
        if (std::find(names.begin(), names.end(), name) == names.end()) {
          names.insert(names.begin(), name);
        }
        return true;
      });
      return true;
    });
  }
  return names;
}

struct LabelEntry {
  std::string text;
  tech::TileType type = tech::kSpace;
  std::optional<std::string> cell;  // nullopt: the root (a "def")
};

/// selEnumLFunc (exact: same area, text and justification) or, when
/// `relaxed`, selEnumLFunc2 (same area, text or justification may differ):
/// the layout label of a selected one, with the text the layout has and the
/// use holding it (none for the top cell or the edit root, cmdWhatLabelFunc).
/// The selection keeps hierarchical names ("child_0/VDD") of labels copied
/// from children, which only the relaxed search finds (MODEL-G-053).
std::optional<LabelEntry> layout_label(const Context& c, const db::Label& want, bool relaxed) {
  std::optional<LabelEntry> hit;
  const db::CellDef* edit_root = c.ed->edit_root != nullptr ? c.ed->edit_root : c.ed->edit_cell;
  (void)connect::search_tree_labels(*c.root, want.rect, tech::TypeMask::all(), {}, c.s.magic_compat, [&](const connect::TreeLabel& l) {
    if (db::apply(l.to_root, l.label->rect) != want.rect) {
      return true;
    }
    const bool same_text = l.label->text == want.text;
    const bool same_just = db::transform_position(l.to_root, l.label->justification) == want.justification;
    if (relaxed ? !same_text && !same_just : !same_text || !same_just) {
      return true;
    }
    hit = LabelEntry{l.label->text, want.type, std::nullopt};
    if (!l.path->empty() && l.def != edit_root) {
      hit->cell = use_id_of(*l.path);
    }
    return false;
  });
  return hit;
}

/// SelEnumLabels(FALSE): each selected label with the use holding it.
std::vector<LabelEntry> selected_labels(const Context& c) {
  std::vector<LabelEntry> out;
  for (const db::Label& want : c.ed->selection.def()->labels()) {
    std::optional<LabelEntry> e = layout_label(c, want, false);
    if (!e) {
      e = layout_label(c, want, true);
    }
    out.push_back(e ? std::move(*e) : LabelEntry{want.text, want.type, std::nullopt});
  }
  // orderLabelFunc: text, then cell (when both have one), then type
  // descending.
  std::stable_sort(out.begin(), out.end(), [](const LabelEntry& a, const LabelEntry& b) {
    if (a.text != b.text) {
      return a.text < b.text;
    }
    if (a.cell && b.cell && *a.cell != *b.cell) {
      return *a.cell < *b.cell;
    }
    return a.type > b.type;
  });
  return out;
}

std::string label_report(const Context& c, const std::vector<LabelEntry>& labels) {
  std::string out = "Selected label(s):";
  const LabelEntry* last = nullptr;
  int count = 0;
  const auto flush = [&] {
    if (count > 1) {
      out += " (" + std::to_string(count) + " instances)";
    }
  };
  for (const LabelEntry& e : labels) {
    const std::string cell = e.cell.value_or(c.ed->selection.root()->name());
    if (last != nullptr && last->text == e.text && last->cell.value_or(c.ed->selection.root()->name()) == cell &&
        last->type == e.type) {
      ++count;
      continue;
    }
    flush();
    out += "\n    \"" + e.text + "\" is attached to " + std::string{c.tech->type_name(e.type)} + " in cell " +
           (e.cell ? "use " : "def ") + cell;
    last = &e;
    count = 1;
  }
  flush();
  return out;
}

/// dbGetUseName: the identifier with the low array indices.
std::string use_name(const db::CellUse& u) {
  const db::ArrayInfo& a = u.array;
  const bool x = a.xlo != a.xhi;
  const bool y = a.ylo != a.yhi;
  if (!x && !y) {
    return u.id;
  }
  std::string out = u.id + "\\[";
  if (x) {
    out += std::to_string(a.xlo);
  }
  if (x && y) {
    out += ",";
  }
  if (y) {
    out += std::to_string(a.ylo);
  }
  return out + "\\]";
}

/// The selected uses as the layout has them (SelEnumCells(FALSE) order).
std::vector<const db::CellUse*> selected_uses(const Context& c) {
  std::vector<const db::CellUse*> out;
  for (const db::CellUse* u : db::uses_in_search_order(*c.ed->selection.def(), compat::CompatSettings{c.s.magic_compat, true})) {
    const auto found = ops::layout_use(c, *u);
    out.push_back(found ? found->use : u);
  }
  return out;
}

Value::List layer_list(const Context& c, const tech::TypeMask& layers, bool all) {
  Value::List out;
  for (std::size_t t = tech::kFirstUserType; t < c.tech->user_type_end(); ++t) {
    const auto type = static_cast<tech::TileType>(t);
    if (!layers.test(type)) {
      continue;
    }
    const std::string name{c.tech->type_name(type)};
    if (!all) {
      out.emplace_back(name);
      continue;
    }
    Value::List cells;
    for (const std::string& n : cells_with(c, type)) {
      cells.emplace_back(n);
    }
    out.emplace_back(Value::List{Value{name}, Value{cells}});
  }
  return out;
}

void print_layers(const Context& c, const tech::TypeMask& layers) {
  const db::CellDef* edit_root = c.ed->edit_root != nullptr ? c.ed->edit_root : c.ed->edit_cell;
  const bool with_cells = c.ed->selection.root() == c.root && (edit_root == c.root || edit_root == nullptr);
  std::string out = "Selected mask layers:";
  for (std::size_t t = tech::kFirstUserType; t < c.tech->user_type_end(); ++t) {
    const auto type = static_cast<tech::TileType>(t);
    if (!layers.test(type)) {
      continue;
    }
    const std::string name{c.tech->type_name(type)};
    if (!with_cells) {
      out += "\n    " + name;
      continue;
    }
    char padded[64];
    std::snprintf(padded, sizeof padded, "%-8s", name.c_str());
    out += "\n    " + std::string{padded} + " (";
    for (const std::string& n : cells_with(c, type)) {
      out += " " + n + " ";
    }
    out += ")";
  }
  cu::print(c.s, std::move(out));
}

CommandResult what_list(const Context& c, bool all) {
  Value::List labels;
  for (const LabelEntry& e : selected_labels(c)) {
    labels.emplace_back(Value::List{Value{e.text}, Value{std::string{c.tech->type_name(e.type)}},
                                    e.cell ? Value{*e.cell} : Value{Value::List{}}});
  }
  Value::List cells;
  for (const db::CellUse* u : selected_uses(c)) {
    cells.emplace_back(Value::List{Value{use_name(*u)}, Value{u->def->name()}});
  }
  return CommandResult::success(
      Value{Value::List{Value{layer_list(c, selected_layers(c), all)}, Value{labels}, Value{cells}}});
}

CommandResult what_print(const Context& c) {
  const tech::TypeMask layers = selected_layers(c);
  if (!layers.empty()) {
    print_layers(c, layers);
  }
  const std::vector<LabelEntry> labels = selected_labels(c);
  if (!labels.empty()) {
    cu::print(c.s, label_report(c, labels));
  }
  const std::vector<const db::CellUse*> uses = selected_uses(c);
  if (!uses.empty()) {
    std::string out = "Selected subcell(s):";
    for (const db::CellUse* u : uses) {
      out += "\n    Instance \"" + use_name(*u) + "\" of cell \"" + u->def->name() + "\"";
    }
    cu::print(c.s, std::move(out));
  }
  return CommandResult::success();
}

}  // namespace

CommandResult cmd_what(CommandContext& ctx, const Invocation& inv) {
  auto cx = ops::context(ctx);
  if (!cx.ok()) {
    return cu::fail(cx.error());
  }
  const Context& c = cx.value();
  const bool list = inv.args.size() == 1 && inv.args[0].rfind("-list", 0) == 0;
  const bool all = list && inv.args[0].rfind("-listall", 0) == 0;
  if (inv.args.size() > (list ? 1U : 0U)) {
    return fail_text(codes::kUsage, "Usage: what [-list]");
  }
  if (c.ed->selection.def() == nullptr || c.ed->selection.root() == nullptr) {
    return list ? CommandResult::success(Value{Value::List{Value{Value::List{}}, Value{Value::List{}},
                                                           Value{Value::List{}}}})
                : CommandResult::success();
  }
  return list ? what_list(c, all) : what_print(c);
}

CommandResult cmd_identify(CommandContext& ctx, const Invocation& inv) {
  auto cx = ops::context(ctx);
  if (!cx.ok()) {
    return cu::fail(cx.error());
  }
  const Context& c = cx.value();
  if (inv.args.size() != 1) {
    return fail_text(codes::kUsage, "Usage: identify use_id");
  }
  const std::string& id = inv.args[0];
  if (const auto bad = cell::illegal_use_id(id)) {  // CmdIllegalChars(id, ",/", "Cell use id")
    return fail_text(codes::kIdIllegal, *bad);
  }
  db::CellDef* sel = c.ed->selection.def();
  const std::vector<db::CellUse*> selected = sel != nullptr ? db::uses_in_search_order(*sel, compat::CompatSettings{c.s.magic_compat, true})
                                                            : std::vector<db::CellUse*>{};
  if (selected.empty()) {
    return fail_text(codes::kIdNoSubcell, "There isn't a selected subcell;  can't change ids.");
  }
  db::CellUse& sel_use = *selected.front();
  const auto found = ops::layout_use(c, sel_use);
  if (c.ed->edit_cell == nullptr) {
    return fail_text(codes::kNotEditable, "Top-level cell is not editable---cannot change identifier of child cell " +
                                              sel_use.id + ".");
  }
  if (!found || found->use->parent != c.ed->edit_cell) {
    return fail_text(codes::kIdNotChild, "Cell " + sel_use.id + " (" + sel_use.def->name() +
                                             ") isn't a child of the edit cell.\n    Cell identifier not changed.");
  }
  if (!db::rename_use(*found->use, id).ok()) {
    return fail_text(codes::kIdNotUnique,
                     "New name isn't unique within its parent definition.\n    Cell identifier not changed.");
  }
  sel_use.id = id;
  c.ed->windows.area_changed(*c.ed->edit_cell, found->use->bbox, std::nullopt);
  return CommandResult::success();
}

}  // namespace hefesto::select
