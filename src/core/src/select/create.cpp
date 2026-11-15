// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Creating the selection (cmd-selection): the command context, area and
// label selection, removal, chunk, region and net growth, intersection,
// flattening and cell selection. Owner: SELECT.
// (Magic ref: select/selCreate.c, select/selUnselect.c, commands/CmdRS.c
// cmdSelectArea, database/DBcellcopy.c DBCellCopyAllPaint/Labels/Cells)

#include <algorithm>
#include <climits>
#include <cstdint>
#include <set>
#include <string>
#include <utility>

#include "hefesto/db/diag_codes.hpp"
#include "hefesto/cell/hier.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/use_index.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/layer_list.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/edit_frame.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/tech/commands.hpp"
#include "select/ops.hpp"

namespace hefesto::select::ops {

namespace cu = db::cmdutil;

namespace {

[[nodiscard]] db::Rect clip(const db::Rect& a, const db::Rect& b) noexcept {
  return db::Rect{std::max(a.x0, b.x0), std::max(a.y0, b.y0), std::min(a.x1, b.x1), std::min(a.y1, b.y1)};
}

/// Puts `label` into `def` unless an identical one (rectangle and text) is
/// there: Magic's DBEraseLabelsByContent followed by DBPutFontLabel.
void put_unique_label(db::CellDef& def, const db::Label& label) {
  (void)cu::erase_labels_by_content(def, label.rect, label.text);
  def.labels().push_back(label);
}

/// Copies the paint of `types` under `area` (root coordinates) from the
/// root and the descended cells into `dest` (DBCellCopyAllPaint).
void copy_tree_paint(const Context& c, const db::CellDef& root, const db::Rect& area, const tech::TypeMask& types,
                     const connect::Descend& descend, db::CellDef& dest, std::vector<Provenance>* provenance) {
  (void)connect::search_tree_defs(root, area, descend, c.s.magic_compat, [&](const db::CellDef& def, const db::Transform& t,
                                                           const std::string& path) {
    const db::Rect local = connect::to_local(t, area);
    bool any = false;
    for (std::size_t p = tech::kFirstUserPlane; p < def.plane_count() && !any; ++p) {
      any = !def.plane(static_cast<tech::PlaneId>(p)).enumerate(local, types, [](const db::TileView&) {
        return false;
      });
    }
    if (any) {
      (void)ops::copy_paint(def, local, t, dest, *c.tech, types);
      if (provenance != nullptr) {
        provenance->push_back(Provenance{Provenance::Kind::paint, &def, path, clip(db::apply(t, def.bbox()), area)});
      }
    }
    return true;
  });
}

/// The label types SelectArea copies: the layer types, or every type when
/// none or all of the paint types were named.
tech::TypeMask label_mask(const Context& c, const tech::TypeMask& types) {
  const tech::TypeMask paint = paint_types(*c.tech) & types;
  if (paint.empty() || paint == paint_types(*c.tech)) {
    return tech::TypeMask::all();
  }
  return paint;
}

void copy_tree_labels(const Context& c, const db::Rect& area, const tech::TypeMask& types, const std::string* glob,
                      db::CellDef& dest, std::vector<Provenance>* provenance) {
  (void)connect::search_tree_labels(c.root ? *c.root : dest, area, types, c.descend, c.s.magic_compat, [&](const connect::TreeLabel& l) {
    const db::Label copy = connect::transform_label(*l.label, l.to_root, c.s.magic_compat);
    if (!db::label_in_area(copy.rect, area) || (glob != nullptr && !db::glob_match(*glob, l.label->text))) {
      return true;
    }
    put_unique_label(dest, copy);
    if (provenance != nullptr) {
      provenance->push_back(Provenance{Provenance::Kind::label, l.def, *l.path, copy.rect});
    }
    return true;
  });
}

void copy_tree_cells(const Context& c, const db::Rect& area) {
  Selection& sel = c.ed->selection;
  (void)connect::search_tree_cells(*c.root, area, c.descend, c.s.magic_compat, [&](const connect::TreeCell& found) {
    const db::CellUse& u = *found.use;
    const db::Transform t = db::compose(found.parent_to_root, u.transform);
    if (sel.add_use(*u.def, u.id, t, u.array, u.locked) != nullptr) {
      sel.provenance().push_back(Provenance{Provenance::Kind::use, u.parent, *found.path,
                                            db::apply(found.parent_to_root, u.bbox)});
    }
    return true;
  });
}

/// Stacked contacts containing `type` (DBMaskAddStacking).
tech::TypeMask with_stacking(const tech::Technology& tech, tech::TileType type) {
  tech::TypeMask m = tech::TypeMask::of(type);
  if (!tech.is_contact(type)) {
    return m;
  }
  for (const tech::ContactInfo& ci : tech.contacts()) {
    if (ci.stacked && (ci.component_a == type || ci.component_b == type)) {
      m.set(ci.type);
    }
  }
  return m;
}

// ---- chunk --------------------------------------------------------------------

struct ChunkSearch {
  const db::Plane* plane = nullptr;
  tech::TypeMask wrong;
  db::Rect contained;
  std::int64_t best_min = 0;
  std::int64_t best_max = 0;
  db::Rect best{0, 0, 0, 0};
};

constexpr int kMaxChunkRecursion = 22;

/// selFindChunk: the largest rectangle inside `search` containing the
/// reference area and free of wrong types.
void find_chunk(ChunkSearch& s, const db::Rect& search, int level) {
  if (level == kMaxChunkRecursion) {
    return;
  }
  std::int64_t lo = search.width();
  std::int64_t hi = search.height();
  if (lo > hi) {
    std::swap(lo, hi);
  }
  if (lo < s.best_min || (lo == s.best_min && hi <= s.best_max)) {
    return;
  }
  std::optional<db::Rect> wrong;
  (void)s.plane->enumerate(search, s.wrong, [&](const db::TileView& t) {
    wrong = t.area;
    return false;
  });
  if (!wrong) {
    s.best_min = lo;
    s.best_max = hi;
    s.best = search;
    return;
  }
  db::Rect smaller = search;
  bool try_x = true;
  if (wrong->x0 >= s.contained.x1) {
    smaller.x1 = wrong->x0;
  } else if (wrong->x1 <= s.contained.x0) {
    smaller.x0 = wrong->x1;
  } else {
    try_x = false;
  }
  if (try_x) {
    find_chunk(s, smaller, level + 1);
  }
  smaller = search;
  if (wrong->y0 >= s.contained.y1) {
    smaller.y1 = wrong->y0;
  } else if (wrong->y1 <= s.contained.y0) {
    smaller.y0 = wrong->y1;
  } else {
    return;
  }
  find_chunk(s, smaller, level + 1);
}

/// Grows the chunk search window past the sides the best rectangle touches.
db::Rect grow_window(const db::Rect& w, const db::Rect& best) {
  const db::Coord width = w.x1 - w.x0;
  const db::Coord height = w.y1 - w.y0;
  db::Rect n = w;
  n.x0 = best.x0 == w.x0 ? w.x0 - width : best.x0 - 1;
  n.y0 = best.y0 == w.y0 ? w.y0 - height : best.y0 - 1;
  n.x1 = best.x1 == w.x1 ? w.x1 + width : best.x1 + 1;
  n.y1 = best.y1 == w.y1 ? w.y1 + height : best.y1 + 1;
  return n;
}

/// DBCellCopyManhattanPaint: the Manhattan tiles of exactly `mask`'s types
/// under `area`, clipped, into `dest` (no residues, unlike copy_paint).
void copy_tree_manhattan(const Context& c, const db::Rect& area, const tech::TypeMask& mask, db::CellDef& dest) {
  (void)connect::search_tree_tiles(*c.root, area, mask, c.descend, c.s.magic_compat, [&](const connect::TreeTile& t) {
    if (!t.tile.split) {
      const db::Rect r = clip(db::apply(t.to_root, t.tile.area), area);
      (void)dest.plane(t.plane).paint(r, db::paint_row(*c.tech, t.plane, t.tile.type));
    }
    return true;
  });
}

/// The chunk rectangle (null when no rectangle contains the area).
db::Rect chunk_rect(const Context& c, const db::Rect& area, tech::TileType type, const tech::TypeMask& mask) {
  std::optional<db::Rect> split;
  (void)connect::search_tree_tiles(*c.root, area, mask, {}, c.s.magic_compat, [&](const connect::TreeTile& t) {
    if (t.tile.split) {
      split = db::apply(t.to_root, t.tile.area);
      return false;
    }
    return true;
  });
  if (split) {
    return *split;
  }
  const auto home = c.tech->home_plane(type);
  if (!home) {
    return db::Rect{};
  }
  ChunkSearch s;
  s.wrong = ~mask;
  s.contained = area;
  db::Rect window = db::expand(area, 10);
  for (;;) {
    auto scratch_def = scratch(c);
    copy_tree_manhattan(c, window, mask, *scratch_def);
    s.plane = &scratch_def->plane(*home);
    find_chunk(s, window, 0);
    if (s.best.is_null() || db::strong_surround(window, s.best)) {
      return s.best;
    }
    window = grow_window(window, s.best);
  }
}

// ---- region and net -------------------------------------------------------------

/// Collects a hierarchical net into a scratch cell (Magic's Select2Def).
struct NetCollector final : connect::NetVisitor {
  db::CellDef& dest;
  const tech::Technology& tech;
  NetCollector(db::CellDef& d, const tech::Technology& t) : dest(d), tech(t) {}
  bool on_tile(const connect::NetTile& t) override {
    const db::PaintRow row = db::paint_row(tech, t.plane, t.tile.type);
    if (t.tile.split) {
      (void)dest.plane(t.plane).paint_triangle(t.tile.area, t.tile.diagonal, t.tile.right_side, row);
    } else {
      (void)dest.plane(t.plane).paint(t.tile.area, row);
    }
    return true;
  }
  bool on_label(const connect::NetLabel& l) override {
    put_unique_label(dest, l.copy);
    return true;
  }
};

/// SelRemoveSel2: removes the scratch net's paint, and the selected labels
/// attached to it, from the selection.
void remove_scratch(const Context& c, const db::CellDef& net) {
  db::CellDef& sel = *c.ed->selection.def();
  for (std::size_t p = tech::kFirstUserPlane; p < net.plane_count(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    net.plane(plane).enumerate(db::whole_plane(), paint_types(*c.tech), [&](const db::TileView& t) {
      if (!t.split && t.type < c.tech->user_type_end()) {
        (void)db::erase_type(sel, *c.tech, t.area, t.type);
      }
      return true;
    });
  }
  std::vector<std::pair<db::Rect, std::string>> doomed;
  for (const db::Label& l : sel.labels()) {
    const auto home = c.tech->home_plane(l.type);
    if (l.type == tech::kSpace || !home) {
      continue;
    }
    const bool attached = !net.plane(*home).enumerate(db::expand(l.rect, 1), c.tech->connects_to(l.type),
                                                      [](const db::TileView&) { return false; });
    if (attached) {
      doomed.emplace_back(l.rect, l.text);
    }
  }
  for (const auto& [rect, text] : doomed) {
    (void)cu::erase_labels_by_content(sel, rect, text);
  }
}

/// Copies the scratch net into the selection.
void add_scratch(const Context& c, const db::CellDef& net) {
  db::CellDef& sel = *c.ed->selection.def();
  (void)ops::copy_paint(net, db::whole_plane(), db::kIdentity, sel, *c.tech, paint_types(*c.tech));
  for (const db::Label& l : net.labels()) {
    put_unique_label(sel, l);
  }
}

db::Rect grow_net(const Context& c, const db::Rect& area, const tech::TypeMask& start_types,
                  const connect::ConnectTable& table, bool less) {
  auto net = scratch(c);
  connect::SearchStart start;
  start.area = area;
  start.types = start_types;
  connect::SearchOptions o;
  o.magic_compat = c.s.magic_compat;
  o.labels = c.ed->selection.label_policy;
  o.descend = c.descend;
  o.connect = table;
  NetCollector collect(*net, *c.tech);
  (void)connect::search_hierarchical(*c.root, *c.tech, start, o, collect);
  if (less) {
    remove_scratch(c, *net);
  } else {
    add_scratch(c, *net);
  }
  db::recompute_bbox(*net);
  return net->extended_bbox();
}

}  // namespace

// ---- context ----------------------------------------------------------------------

diag::Outcome<Context> context(cmd::CommandContext& ctx) {
  auto s = cu::session(ctx);
  if (!s.ok()) {
    return s.error();
  }
  Context c;
  c.s = s.value();
  c.ed = c.s.editor;
  c.tech = c.s.tech;
  c.ed->selection.prune(c.ed->cells);
  // The edit operations walk the selection cell: before the first selection it must exist
  // (empty), or `delete`, `move`, `array`, ... dereferenced a null cell (SCRIPT-PCELL-11).
  c.ed->selection.ensure_cell(c.tech->plane_count());
  if (const view::LayoutWindow* w = c.ed->windows.current(); w != nullptr && w->root() != nullptr) {
    c.root = w->root();
    c.descend = [w](const db::CellUse& u) { return w->is_expanded(u); };
    c.visible = w->visible_types();
    c.see_labels = w->labels_visible();
    c.see_cells = w->cells_visible();
  } else {
    // No window: the edit root, with nothing expanded (Magic's state after
    // `load`, where subcells start unexpanded).
    c.root = c.ed->edit_root != nullptr ? c.ed->edit_root : c.ed->edit_cell;
    c.descend = [](const db::CellUse&) { return false; };
  }
  if (c.root == nullptr) {
    return diag::make(db::codes::kNoEditCell, "Put the cursor in a layout window");
  }
  return c;
}

std::optional<db::Rect> root_box(const Context& c) {
  const editor::EditorState& ed = *c.ed;
  if (!ed.box) {
    return std::nullopt;
  }
  if (ed.box_root == nullptr || ed.box_root == ed.edit_cell) {
    return editor::to_root(ed, *ed.box);
  }
  if (ed.box_root == c.root) {
    return *ed.box;
  }
  return std::nullopt;
}

void set_root_box(const Context& c, const db::Rect& r) {
  editor::EditorState& ed = *c.ed;
  if (c.root == ed.edit_cell || c.root == ed.edit_root || ed.edit_root == nullptr) {
    ed.box = editor::to_edit(ed, r);
    ed.box_root = ed.edit_cell;
  } else {
    ed.box = r;
    ed.box_root = c.root;
  }
}

tech::TypeMask paint_types(const tech::Technology& tech) {
  tech::TypeMask m;
  for (std::size_t t = tech::kFirstUserType; t < tech.type_count(); ++t) {
    m.set(static_cast<tech::TileType>(t));
  }
  return m;
}

diag::Outcome<Layers> parse_layers(const Context& c, std::string_view text) {
  auto parsed = db::parse_layer_list(text, *c.tech);
  if (!parsed.ok()) {
    if (db::is_bad_layer_name(parsed.error())) {
      db::cmdutil::print_block(c.s, tech::layer_names_text(*c.tech));
    }
    return parsed.error();
  }
  Layers out;
  out.types = parsed.value().types;
  out.labels = parsed.value().labels;
  out.cells = parsed.value().subcells;
  if (out.types == tech::TypeMask::of(tech::kSpace) && !out.labels && !out.cells) {
    out.types = paint_types(*c.tech);  // `space` means "*,labels"
    out.labels = true;
  }
  out.types.clear_type(tech::kSpace);
  return out;
}

diag::Status copy_paint(const db::CellDef& source, const db::Rect& area, const db::Transform& t, db::CellDef& target,
                        const tech::Technology& tech, const tech::TypeMask& types) {
  const db::Rect inside = clip(area, connect::to_local(t, db::whole_plane()));
  if (inside.is_null()) {
    return diag::Done{};
  }
  return db::copy_paint(source, inside, t, target, tech, types);
}

Snapshot begin(const Context& c) {
  Selection& sel = c.ed->selection;
  Snapshot before = sel.snapshot();
  sel.set_root(c.root, c.tech->plane_count());
  return before;
}

void commit(const Context& c, Snapshot before) {
  if (db::CellDef* def = c.ed->selection.def()) {
    db::recompute_bbox(*def);
  }
  record_change(*c.ed, std::move(before));
}

// ---- area -----------------------------------------------------------------------------

void select_area(const Context& c, const db::Rect& area, const Layers& layers, const std::string* glob) {
  Selection& sel = c.ed->selection;
  db::CellDef& dest = *sel.def();
  if (!layers.types.empty()) {
    copy_tree_paint(c, *c.root, area, layers.types, c.descend, dest, &sel.provenance());
  }
  sel.area_types = layers.types;  // SelectArea: "remember what types were requested"
  if (layers.labels) {
    copy_tree_labels(c, area, label_mask(c, layers.types), glob, dest, &sel.provenance());
  }
  if (layers.cells) {
    copy_tree_cells(c, area);
  }
}

void remove_area(const Context& c, const db::Rect& area, const Layers& layers, const std::string* glob) {
  Selection& sel = c.ed->selection;
  db::CellDef& def = *sel.def();
  tech::TypeMask label_types = layers.types;
  if (layers.labels) {
    label_types = tech::TypeMask::all();
    label_types.set(db::kLabelsPseudoType);
  }
  (void)db::erase_labels(def, *c.tech, area, label_types,
                         glob != nullptr ? std::optional<std::string_view>(*glob) : std::nullopt);
  for (std::size_t t = tech::kFirstUserType; t < c.tech->type_count(); ++t) {
    if (layers.types.test(static_cast<tech::TileType>(t))) {
      (void)db::erase_type(def, *c.tech, area, static_cast<tech::TileType>(t));
    }
  }
  if (layers.cells) {
    std::vector<const db::CellUse*> doomed;
    for (const auto& u : def.uses()) {
      if (db::overlap(u->bbox, area)) {
        doomed.push_back(u.get());
      }
    }
    for (const db::CellUse* u : doomed) {
      sel.remove_use(*u);
    }
  }
}

tech::TypeMask types_under(const Context& c, const db::Rect& area) {
  tech::TypeMask m;
  (void)connect::search_tree_tiles(*c.root, area, tech::TypeMask::all(), c.descend, c.s.magic_compat, [&](const connect::TreeTile& t) {
    m.set(t.tile.type);
    return true;
  });
  return m;
}

// ---- chunk, region, net ----------------------------------------------------------------

db::Rect select_chunk(const Context& c, const db::Rect& area, tech::TileType type, bool less) {
  if (type == tech::kSpace) {
    return db::Rect{};
  }
  tech::TypeMask mask = with_stacking(*c.tech, type);
  const db::Rect best = chunk_rect(c, area, type, mask);
  if (best.is_null()) {
    return best;
  }
  if (less) {
    remove_area(c, best, Layers{mask, false, false}, nullptr);
    return best;
  }
  if (c.tech->is_contact(type)) {
    mask = tech::TypeMask::of(type);
  }
  select_area(c, best, Layers{mask, true, false}, nullptr);
  return best;
}

db::Rect select_region(const Context& c, const db::Rect& area, tech::TileType type, bool less) {
  const connect::ConnectTable same = [type](tech::TileType t) {
    return t == type ? tech::TypeMask::of(type) : tech::TypeMask{};
  };
  return grow_net(c, area, tech::TypeMask::of(type), same, less);
}

db::Rect select_net(const Context& c, const db::Rect& area, tech::TileType type, bool less) {
  return grow_net(c, area, c.tech->connects_to(type), {}, less);
}

// ---- intersection and flat ---------------------------------------------------------------

void select_intersection(const Context& c, const db::Rect& area, tech::TileType type, bool negate) {
  Selection& sel = c.ed->selection;
  if (sel.root() != c.root || sel.def() == nullptr) {
    return;
  }
  auto old = scratch(c);
  copy_tree_paint(c, *sel.def(), db::whole_plane(), paint_types(*c.tech), {}, *old, nullptr);
  // Where LAYER is present under the box, or (negated) where it is absent.
  auto layer = scratch(c);
  copy_tree_paint(c, *c.root, area, tech::TypeMask::of(type), c.descend, *layer, nullptr);
  const auto home = c.tech->home_plane(type);
  sel.clear();
  if (!home) {
    return;
  }
  const tech::TypeMask where = negate ? tech::TypeMask::of(tech::kSpace) : tech::TypeMask::of(type);
  db::CellDef& dest = *sel.def();
  layer->plane(*home).enumerate(area, where, [&](const db::TileView& region) {
    const db::Rect r = negate ? clip(region.area, area) : region.area;
    for (std::size_t p = tech::kFirstUserPlane; p < old->plane_count(); ++p) {
      old->plane(static_cast<tech::PlaneId>(p)).enumerate(r, paint_types(*c.tech), [&](const db::TileView& t) {
        if (!t.split) {
          (void)db::paint_type(dest, *c.tech, clip(t.area, r), t.type);
        }
        return true;
      });
    }
    return true;
  });
  (void)db::erase_type(dest, *c.tech, db::whole_plane(), type);
}

void select_flat(const Context& c) {
  Selection& sel = c.ed->selection;
  if (sel.def() == nullptr) {
    return;
  }
  auto flat = scratch(c);
  copy_tree_paint(c, *sel.def(), db::whole_plane(), paint_types(*c.tech), {}, *flat, nullptr);
  (void)connect::search_tree_labels(*sel.def(), db::whole_plane(), tech::TypeMask::all(), {}, c.s.magic_compat,
                                    [&](const connect::TreeLabel& l) {
                                      db::Label copy = connect::transform_label(*l.label, l.to_root, c.s.magic_compat);
                                      if (!l.path->empty()) {
                                        // FlatCopyAllLabels: the instance path, the last separator a '/'
                                        const std::string& path = *l.path;
                                        const std::size_t slash = path.find('/');
                                        copy.text = (slash == std::string::npos ? std::string{} : path.substr(slash + 1) + "/") +
                                                    l.label->text;
                                      }
                                      put_unique_label(*flat, copy);
                                      return true;
                                    });
  sel.clear();
  add_scratch(c, *flat);
}

// ---- cells ---------------------------------------------------------------------------------

namespace {

/// The instance path components of `path` ("a/b[1,2]" -> "a", "b[1,2]").
std::vector<std::string_view> path_components(std::string_view path) {
  std::vector<std::string_view> out;
  std::size_t pos = 0;
  while (pos <= path.size()) {
    const std::size_t end = std::min(path.find('/', pos), path.size());
    if (end > pos) {
      out.push_back(path.substr(pos, end - pos));
    }
    pos = end + 1;
  }
  return out;
}

/// "X1[2,0]" / "X1[3]" -> the element indices of `use`, or its low corner.
db::Point element_of(const db::CellUse& use, std::string_view name) {
  db::Point p{use.array.xlo, use.array.ylo};
  const std::size_t open = name.rfind('[');
  if (open == std::string_view::npos || name.back() != ']') {
    return p;
  }
  const std::string inner{name.substr(open + 1, name.size() - open - 2)};
  const std::size_t comma = inner.find(',');
  if (comma != std::string::npos) {
    p.y = cu::c_atoi(inner.substr(0, comma));
    p.x = cu::c_atoi(inner.substr(comma + 1));
  } else if (use.array.xlo == use.array.xhi) {
    p.y = cu::c_atoi(inner);
  } else {
    p.x = cu::c_atoi(inner);
  }
  return p;
}

}  // namespace

namespace {

/// DBCellRead of a definition not read yet, through the windows' reader.
void read_if_unavailable(const Context& c, db::CellDef* def) {
  if (def == nullptr || def->flags().available || c.s.ctx == nullptr) {
    return;
  }
  if (const auto read = c.ed->windows.cell_reader()) {
    (void)read(*c.s.ctx, *def);
  }
}

}  // namespace

std::optional<UseRef> find_use_path(const Context& c, std::string_view path) {
  const db::CellDef* def = c.root;
  db::Transform to_root = db::kIdentity;
  std::optional<UseRef> found;
  for (const std::string_view name : path_components(path)) {
    if (def == nullptr) {
      return std::nullopt;
    }
    db::CellUse* use = db::find_use(*def, name);
    if (use == nullptr) {
      return std::nullopt;
    }
    UseRef ref{use, to_root, found ? found->path + "/" + std::string{name} : std::string{name}, element_of(*use, name)};
    to_root = db::compose(to_root, db::element_transform(*use, ref.element.x, ref.element.y));
    read_if_unavailable(c, use->def);  // DBTreeFindUse reads each cell on the path (UI-G-033)
    def = use->def;
    found = std::move(ref);
  }
  return found;
}

std::vector<UseRef> uses_at(const Context& c, const db::Rect& area) {
  std::vector<UseRef> out;
  // Uses whose element contains the area: descended uses and the first
  // unexpanded level below them.
  const auto visit = [&](const auto& self, const db::CellDef& def, const db::Transform& to_root,
                         const std::string& path) -> void {
    const db::Rect local = connect::to_local(to_root, area);
    for (db::CellUse* u : db::uses_in_search_order(def, compat::CompatSettings{c.s.magic_compat, true})) {
      if (u->def == nullptr || !db::overlap(u->bbox, local)) {
        continue;
      }
      const db::ArrayInfo& a = u->array;
      for (std::int32_t y = std::min(a.ylo, a.yhi); y <= std::max(a.ylo, a.yhi); ++y) {
        for (std::int32_t x = std::min(a.xlo, a.xhi); x <= std::max(a.xlo, a.xhi); ++x) {
          const db::Transform et = db::element_transform(*u, x, y);
          if (!db::surround(db::apply(et, u->def->bbox()), local)) {
            continue;
          }
          const std::string p = connect::child_path(path, *u, x, y);
          out.push_back(UseRef{u, to_root, p, db::Point{x, y}});
          if (!c.descend || c.descend(*u)) {
            self(self, *u->def, db::compose(to_root, et), p);
          }
        }
      }
    }
  };
  visit(visit, *c.root, db::kIdentity, std::string{});
  std::stable_sort(out.begin(), out.end(), [](const UseRef& a, const UseRef& b) {
    const db::Rect ra = a.use->def->bbox();
    const db::Rect rb = b.use->def->bbox();
    return ra.width() * ra.height() < rb.width() * rb.height();
  });
  return out;
}

void select_cell(const Context& c, const UseRef& ref, bool replace) {
  Selection& sel = c.ed->selection;
  if (replace && sel.cycle.last_selected_use != nullptr) {
    sel.remove_use(*sel.cycle.last_selected_use);
  }
  db::CellUse* added = nullptr;
  if (ref.use == nullptr) {
    // The window's top cell itself (Magic selects the root use).
    added = sel.add_use(*c.root, std::string{cell::kTopUseId}, db::kIdentity, db::ArrayInfo{});
  } else {
    const db::CellUse& u = *ref.use;
    added = sel.add_use(*u.def, u.id, db::compose(ref.parent_to_root, u.transform), u.array, u.locked);
    if (added != nullptr) {
      sel.provenance().push_back(
          Provenance{Provenance::Kind::use, u.parent, ref.path, db::apply(ref.parent_to_root, u.bbox)});
    }
  }
  sel.cycle.last_selected_use = added;
}

void unselect_cell(const Context& c, const UseRef& ref) {
  Selection& sel = c.ed->selection;
  const db::CellDef* def = ref.use != nullptr ? ref.use->def : c.root;
  const db::Transform t = ref.use != nullptr ? db::compose(ref.parent_to_root, ref.use->transform) : db::kIdentity;
  const db::ArrayInfo array = ref.use != nullptr ? ref.use->array : db::ArrayInfo{};
  const db::CellUse* doomed = nullptr;
  for (const auto& u : sel.def()->uses()) {
    if (u->def == def && u->transform == t && u->array == array) {
      doomed = u.get();
    }
  }
  if (doomed != nullptr) {
    sel.remove_use(*doomed);
  }
}

}  // namespace hefesto::select::ops
