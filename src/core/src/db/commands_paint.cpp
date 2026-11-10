// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The paint commands of capability cmd-paint-erase: `paint`, `erase` (with
// paint and erase by example), `fill`, `corner`, `contact`, `polygon`,
// `splitpaint`, `spliterase` and `drop`. Ported from Magic VLSI 8.3.694
// (fc5daa9e) commands/CmdLQ.c (CmdPaint, CmdPolygon), commands/CmdE.c
// (CmdErase), commands/CmdFI.c (CmdFill), commands/CmdCD.c (CmdCorner,
// cmdBevelFunc, CmdContact, CmdDrop) and commands/CmdRS.c (CmdSplitPaint,
// CmdSplitErase), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md). Hefesto's decisions
// Q-CMDEDIT-14..21 are noted where they apply. Owner: EDIT.

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "db/command_util.hpp"
#include "db/edit_util.hpp"
#include "db/tech_masks.hpp"
#include "hefesto/cmd/keywords.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/layer_list.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/polygon.hpp"
#include "hefesto/editor/edit_frame.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::db {

namespace cu = cmdutil;
using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;

namespace {

// --- shared -------------------------------------------------------------------

constexpr std::string_view kNoEditDef = "No cell def being edited!";

LayerListContext layer_context(const cu::Session& s) {
  LayerListContext c;
  const editor::EditorState& ed = *s.editor;
  if (!ed.command_point) {
    return c;
  }
  const view::LayoutWindow* w = ed.windows.find(ed.command_point->window);
  if (w == nullptr || w->root() == nullptr) {
    return c;
  }
  // `$`: the types under the pointer point in that window, visible ones only.
  tech::TypeMask under;
  const Point p = ed.command_point->point;
  edit::search_tree(ed, *w->root(), Rect{p.x, p.y, p.x + 1, p.y + 1}, tech::TypeMask::all(), edit::Depth::expanded,
                    [&](const edit::FoundTile& t) { under.set(t.type); });
  c.under_pointer = under & (w->visible_types() | tech::TypeMask::of(tech::kSpace));
  return c;
}

diag::Outcome<LayerList> layers(const cu::Session& s, std::string_view text) {
  return parse_layer_list(text, *s.tech, layer_context(s));
}

// Types that may be painted (active in the editor: not locked).
tech::TypeMask active(const cu::Session& s) {
  return ~s.editor->locked_types;
}

tech::TypeMask paintable(const cu::Session& s, const tech::TypeMask& types) {
  tech::TypeMask out;
  for (std::size_t t = 1; t < s.tech->type_count(); ++t) {
    if (types.test(static_cast<tech::TileType>(t))) {
      out.set(static_cast<tech::TileType>(t));
    }
  }
  return out;
}

// DBPaintValid for each type of `types`, in increasing type order.
diag::Status paint_all(const edit::Target& t, const Rect& area, const tech::TypeMask& types) {
  for (std::size_t ty = 1; ty < t.s.tech->type_count(); ++ty) {
    if (!types.test(static_cast<tech::TileType>(ty))) {
      continue;
    }
    if (auto st = paint(*t.def, *t.s.tech, area, static_cast<tech::TileType>(ty), edit::paint_options(t.s)); !st.ok()) {
      return st;
    }
  }
  return diag::Done{};
}

// --- paint / erase -------------------------------------------------------------

// The step of `erase` that removes paint, error layers, labels and subcells
// (cmd-paint-erase "Erase command", steps 3-6). Returns whether labels went.
diag::Outcome<bool> erase_set(const edit::Target& t, const LayerList& list) {
  const tech::Technology& tech = *t.s.tech;
  tech::TypeMask user;
  for (std::size_t ty = tech::kFirstUserType; ty < tech.type_count(); ++ty) {
    user.set(static_cast<tech::TileType>(ty));
  }
  // Q-CMDEDIT-16: db::erase ignores named stacked types with magic-compat ON.
  if (auto st = erase(*t.def, tech, t.area, list.types & user, edit::paint_options(t.s)); !st.ok()) {
    return st.error();
  }
  for (const tech::TileType err : {tech::kErrorPS, tech::kErrorS, tech::kErrorP}) {
    if (list.types.test(err)) {
      if (auto st = erase_type(*t.def, tech, t.area, err); !st.ok()) {
        return st.error();
      }
    }
  }
  const bool labels = erase_labels(*t.def, tech, t.area, list.types) > 0;
  if (list.subcells) {
    std::vector<CellUse*> doomed;
    for (const auto& u : t.def->uses()) {
      if (overlap(u->bbox, t.area)) {
        doomed.push_back(u.get());
      }
    }
    for (CellUse* u : doomed) {
      (void)remove_use(*u);
    }
  }
  return labels;
}

// The reference types of paint/erase by example: active, visible, non-space
// types of the tiles overlapping `area` (root coordinates) in the window.
tech::TypeMask example_types(const edit::Target& t, const Rect& area, tech::TypeMask& visible) {
  const editor::EditorState& ed = *t.s.editor;
  const CellDef* root = edit::box_root(ed);
  const view::LayoutWindow* w = edit::window_on(ed, root);
  visible = w != nullptr ? w->visible_types() : tech::TypeMask::all();
  tech::TypeMask found;
  edit::search_tree(ed, *root, area, tech::TypeMask::all(), edit::Depth::expanded,
                    [&](const edit::FoundTile& f) { found.set(f.type); });
  found.clear_type(tech::kSpace);
  return found & visible & active(t.s);
}

diag::Outcome<Rect> example_area(const cu::Session& s, const Invocation& inv) {
  if (inv.args[0] == "pick") {
    const auto x = cu::parse_distance(s, inv.args[1], cmd::Axis::x, false);
    const auto y = cu::parse_distance(s, inv.args[2], cmd::Axis::y, false);
    if (!x.ok() || !y.ok()) {
      return x.ok() ? y.error() : x.error();
    }
    const Point p = editor::to_root(*s.editor, Point{x.value(), y.value()});
    return Rect{p.x, p.y, p.x + 1, p.y + 1};
  }
  if (!s.editor->command_point) {
    return cu::bad_argument("Put the cursor in a layout window");
  }
  return s.editor->command_point->pixel_area;
}

// `paint|erase cursor` and `paint|erase pick X Y` ("Paint or erase by
// example"). The cursor form is logged as `paint|erase pick Xi Yi` with the
// pointer in edit-cell coordinates (CmdPaint/CmdErase TxRebuildCommand).
CommandResult by_example(CommandContext& ctx, const Invocation& inv, bool erasing) {
  auto t = edit::paint_target(ctx, erasing ? kNoEditDef : "The cell is not editable.");
  if (!t.ok()) {
    return cu::fail(t.error());
  }
  const edit::Target& e = t.value();
  const auto area = example_area(e.s, inv);
  if (!area.ok()) {
    return cu::fail(area.error());
  }
  tech::TypeMask visible;
  const tech::TypeMask types = example_types(e, area.value(), visible);
  LayerList list;
  list.types = types.empty() ? paintable(e.s, visible & active(e.s)) : types;
  list.types.clear_type(tech::kSpace);
  bool labels = false;
  if (erasing || types.empty()) {
    if (e.area.is_null() && types.empty()) {
      list.types.set(kLabelsPseudoType);  // a point box takes the labels it touches
    }
    auto r = erase_set(e, list);
    if (!r.ok()) {
      return cu::fail(r.error());
    }
    labels = r.value();
  } else if (auto st = paint_all(e, e.area, list.types); !st.ok()) {
    return cu::fail(st.error());
  }
  edit::finish(e, e.area, list.types, edit::Effects{true, true, labels});
  CommandResult result = CommandResult::success();
  if (inv.args[0] != "pick" && e.s.editor->command_point) {
    const Point p = editor::to_edit(*e.s.editor, e.s.editor->command_point->point);
    result.logged_as = Invocation{inv.name, {"pick", std::to_string(p.x) + "i", std::to_string(p.y) + "i"}, cmd::Origin::script, std::nullopt};
  }
  return result;
}

bool is_example_form(const Invocation& inv) {
  return (inv.args.size() == 1 && inv.args[0].starts_with("cursor")) ||
         (inv.args.size() == 3 && inv.args[0] == "pick");
}

CommandResult cmd_paint(CommandContext& ctx, const Invocation& inv) {
  if (is_example_form(inv)) {
    return by_example(ctx, inv, false);
  }
  if (inv.args.size() != 1) {
    return edit::usage("Usage: paint <layers> | cursor | pick x y");
  }
  auto t = edit::paint_target(ctx, "The cell is not editable.");
  if (!t.ok()) {
    return cu::fail(t.error());
  }
  const edit::Target& e = t.value();
  auto list = layers(e.s, inv.args[0]);
  if (!list.ok()) {
    return cu::fail(list.error());
  }
  if (list.value().labels) {
    return cu::fail(cu::bad_argument("Label layer cannot be painted. Use the \"label\" command"));
  }
  if (list.value().subcells) {
    return cu::fail(cu::bad_argument("Subcell layer cannot be painted. Use \"getcell\"."));
  }
  tech::TypeMask types = list.value().types;
  types.clear_type(tech::kSpace);
  if (e.area.is_null() || types.empty()) {
    return CommandResult::success();
  }
  if (auto st = paint_all(e, e.area, types); !st.ok()) {
    return cu::fail(st.error());
  }
  edit::finish(e, e.area, types);
  return CommandResult::success();
}

CommandResult cmd_erase(CommandContext& ctx, const Invocation& inv) {
  if (is_example_form(inv)) {
    return by_example(ctx, inv, true);
  }
  if (inv.args.size() > 1) {
    return edit::usage("Usage: erase [<layers> | cursor | pick x y]");
  }
  auto t = edit::paint_target(ctx, kNoEditDef);
  if (!t.ok()) {
    return cu::fail(t.error());
  }
  const edit::Target& e = t.value();
  const bool all = inv.args.empty() || inv.args[0] == "space";
  auto list = layers(e.s, all ? std::string_view{"*,labels"} : std::string_view{inv.args[0]});
  if (!list.ok()) {
    return cu::fail(list.error());
  }
  LayerList l = list.value();
  l.types.clear_type(tech::kSpace);
  if (l.types.empty()) {
    return CommandResult::success();
  }
  auto labels = erase_set(e, l);
  if (!labels.ok()) {
    return cu::fail(labels.error());
  }
  edit::finish(e, e.area, l.types, edit::Effects{true, true, labels.value() || l.subcells});
  return CommandResult::success();
}

// --- fill and corner ------------------------------------------------------------

// The transform taking a Manhattan direction to north (and, for corner,
// its perpendicular second direction to east), so one rule serves every
// case: the cases of cmd-paint-erase "Fill command" and "Corner command"
// are the north (north-east) case mirrored or transposed.
Transform frame(cmd::Direction d1, cmd::Direction d2) {
  using D = cmd::Direction;
  const bool second_neg = d2 == D::west || d2 == D::south;
  switch (d1) {
    case D::south:
      return second_neg ? Transform{-1, 0, 0, 0, -1, 0} : Transform{1, 0, 0, 0, -1, 0};
    case D::east:
      return second_neg ? Transform{0, -1, 0, 1, 0, 0} : Transform{0, 1, 0, 1, 0, 0};
    case D::west:
      return second_neg ? Transform{0, -1, 0, -1, 0, 0} : Transform{0, 1, 0, -1, 0, 0};
    default:
      return second_neg ? Transform{-1, 0, 0, 0, 1, 0} : kIdentity;
  }
}

Rect clip(const Rect& a, const Rect& b) {
  return Rect{std::max(a.x0, b.x0), std::max(a.y0, b.y0), std::min(a.x1, b.x1), std::min(a.y1, b.y1)};
}

// The tiles entering the box through its entry side (the 2-unit strip),
// with the box, in the north-facing frame `f`.
struct Entering {
  Rect box;  // in the frame
  std::vector<edit::FoundTile> tiles;  // root coordinates
};

Entering entering(const edit::Target& t, const Transform& f, const tech::TypeMask& types) {
  Entering out{apply(f, t.root_area), {}};
  const Rect strip{out.box.x0, out.box.y0 - 1, out.box.x1, out.box.y0 + 1};
  const editor::EditorState& ed = *t.s.editor;
  edit::search_tree(ed, *edit::box_root(ed), apply(invert(f), strip), types, edit::Depth::expanded,
                    [&](const edit::FoundTile& tile) { out.tiles.push_back(tile); });
  std::reverse(out.tiles.begin(), out.tiles.end());  // Magic paints its list head first
  return out;
}

// Paints `r` (frame coordinates) into the edit cell with DBPaint.
diag::Status paint_frame_rect(const edit::Target& t, const Transform& f, const Rect& r, tech::TileType type) {
  if (r.x0 > r.x1 || r.y0 > r.y1) {
    return diag::Done{};
  }
  const Rect edit_r = editor::to_edit(*t.s.editor, apply(invert(f), r));
  return paint_type(*t.def, *t.s.tech, edit_r, type);
}

diag::Outcome<tech::TypeMask> fill_layers(const cu::Session& s, const std::vector<std::string>& args,
                                          std::size_t index) {
  if (args.size() <= index) {
    return all_but_space_and_drc();
  }
  auto l = layers(s, args[index]);
  if (!l.ok()) {
    return l.error();
  }
  return l.value().types;
}

CommandResult cmd_fill(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.empty() || inv.args.size() > 2) {
    return edit::usage("Usage: fill direction [layers]");
  }
  const auto dir = cmd::parse_direction(inv.args[0], true);
  if (!dir.ok()) {
    return cu::fail(dir.error());
  }
  auto t = edit::paint_target(ctx, kNoEditDef);
  if (!t.ok()) {
    return cu::fail(t.error());
  }
  const edit::Target& e = t.value();
  const auto types = fill_layers(e.s, inv.args, 1);
  if (!types.ok()) {
    return cu::fail(types.error());
  }
  const Transform f = frame(dir.value(), cmd::Direction::east);
  const Entering in = entering(e, f, types.value());
  for (const edit::FoundTile& tile : in.tiles) {
    Rect r = clip(apply(f, tile.area), in.box);
    r.y1 = in.box.y1;
    if (auto st = paint_frame_rect(e, f, r, tile.type); !st.ok()) {
      return cu::fail(st.error());
    }
  }
  edit::finish(e, e.area, types.value());
  return CommandResult::success();
}

// The two legs of one wire for `corner n e` in the frame (cmd-paint-erase
// "Corner command" table, first row); `room` is false when the second leg
// does not fit.
struct Legs {
  Rect first;
  Rect second;
  bool room = true;
};

Legs corner_legs(const Rect& tile, const Rect& box) {
  const Rect r = clip(tile, box);
  const Coord w = r.x1 - r.x0;
  Legs l{r, r, true};
  l.first.y1 = box.y1 - (r.x0 - box.x0);
  l.second = Rect{r.x0, l.first.y1 - w, box.x1, l.first.y1};
  l.room = l.second.y0 >= box.y0;
  l.first = clip(l.first, box);
  l.second = clip(l.second, box);
  return l;
}

// cmdBevelFunc for `corner n e ... bevel` in the frame: the wire as one
// polygon with a 45-degree mitre, in CIFPath order; empty when a leg is null.
std::vector<Point> bevel_path(const Rect& tile, const Rect& box, bool& room) {
  Rect r2 = clip(tile, box);
  Rect r3 = r2;
  const Coord wire = r2.x1 - r2.x0;
  r3.y1 = box.y1 - (r2.x0 - box.x0);
  r3.x1 = box.x1;
  r3.x0 = box.x1;
  const Coord boxwidth = r3.x1 - r2.x0;
  const Coord boxheight = r3.y1 - r3.y0;
  if (boxwidth > boxheight) {
    r3.x0 = r2.x0 + boxheight;
  }
  r3.x0 -= wire;
  r3.y0 = r3.y1 - wire;
  room = r3.y0 >= box.y0;
  r2.y1 = (boxheight > boxwidth ? r3.y1 - boxwidth : r2.y0) + wire;
  if (r2.is_null() || r3.is_null()) {
    return {};
  }
  std::vector<Point> added{{r2.x0, r2.y1}, {r2.x0, r2.y0}, {r2.x1, r2.y0}};
  if (boxheight > boxwidth) {
    added.push_back(Point{r2.x1, r2.y1 - wire});
  } else if (boxwidth > boxheight) {
    added.push_back(Point{r3.x0 + wire, r3.y0});
  }
  added.insert(added.end(), {Point{r3.x1, r3.y0}, Point{r3.x1, r3.y1}, Point{r3.x0, r3.y1}});
  std::reverse(added.begin(), added.end());  // AddNewPoint prepends
  for (Point& p : added) {
    p = Point{std::clamp(p.x, box.x0, box.x1), std::clamp(p.y, box.y0, box.y1)};
  }
  return added;
}

// Q-CMDEDIT-15: the bevel polygon goes into the edit cell through DBPaint.
diag::Status paint_bevel(const edit::Target& t, const Transform& f, const std::vector<Point>& frame_path,
                         tech::TileType type) {
  std::vector<Point> path;
  for (const Point& p : frame_path) {
    path.push_back(editor::to_edit(*t.s.editor, apply(invert(f), p)));
  }
  const auto pieces = decompose_polygon(path);
  if (!pieces.ok()) {
    return diag::Done{};
  }
  for (const PolygonPiece& piece : pieces.value()) {
    std::optional<TriangleSpec> tri;
    if (piece.triangle) {
      tri = TriangleSpec{piece.triangle->diagonal, piece.triangle->right_side};
    }
    if (auto st = paint_type(*t.def, *t.s.tech, piece.area, type, tri); !st.ok()) {
      return st;
    }
  }
  return diag::Done{};
}

diag::Status corner_wire(const edit::Target& t, const Transform& f, const Rect& box, const edit::FoundTile& tile,
                         bool bevel, bool& room) {
  const Rect r = apply(f, tile.area);
  if (bevel) {
    bool fits = true;
    const std::vector<Point> path = bevel_path(r, box, fits);
    room = room && fits;
    return path.empty() ? diag::Status{diag::Done{}} : paint_bevel(t, f, path, tile.type);
  }
  const Legs legs = corner_legs(r, box);
  room = room && legs.room;
  for (const Rect& leg : {legs.first, legs.second}) {
    if (!leg.is_null()) {
      if (auto st = paint_frame_rect(t, f, leg, tile.type); !st.ok()) {
        return st;
      }
    }
  }
  return diag::Done{};
}

bool perpendicular(cmd::Direction a, cmd::Direction b) {
  const auto vertical = [](cmd::Direction d) { return d == cmd::Direction::north || d == cmd::Direction::south; };
  return vertical(a) != vertical(b);
}

CommandResult cmd_corner(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.size() < 2 || inv.args.size() > 4) {
    return edit::usage("Usage: corner direction1 direction2 [layers]");
  }
  const auto d1 = cmd::parse_direction(inv.args[0], true);
  const auto d2 = d1.ok() ? cmd::parse_direction(inv.args[1], true) : d1;
  if (!d2.ok()) {
    return cu::fail(d2.error());
  }
  auto t = edit::paint_target(ctx, kNoEditDef);
  if (!t.ok()) {
    return cu::fail(t.error());
  }
  const edit::Target& e = t.value();
  if (!perpendicular(d1.value(), d2.value())) {
    cu::print(e.s, "Can't corner-fill " + inv.args[0] + " and then " + inv.args[1] + ".");
    return CommandResult::success();
  }
  std::vector<std::string> args = inv.args;
  const bool bevel = args.size() > 2 && args.back().starts_with("bevel");
  if (bevel) {
    args.pop_back();
  }
  const auto types = fill_layers(e.s, args, 2);
  if (!types.ok()) {
    return cu::fail(types.error());
  }
  const Transform f = frame(d1.value(), d2.value());
  const Entering in = entering(e, f, types.value());
  bool room = true;
  for (const edit::FoundTile& tile : in.tiles) {
    if (auto st = corner_wire(e, f, in.box, tile, bevel, room); !st.ok()) {
      return cu::fail(st.error());
    }
  }
  if (!room) {
    cu::warn(e.s, "There's not enough room in the box for all the wires.");
  }
  edit::finish(e, e.area, types.value());
  return CommandResult::success();
}

// --- contact ---------------------------------------------------------------------

std::vector<Rect> tiles_of(const CellDef& def, tech::PlaneId plane, const Rect& area, const tech::TypeMask& types) {
  std::vector<Rect> out;
  def.plane(plane).enumerate(area, types, [&](const TileView& t) {
    out.push_back(clip(t.area, area));
    return true;
  });
  return out;
}

// The two lowest-numbered residues of a contact.
std::array<tech::TileType, 2> lowest_residues(const tech::ContactInfo& c) {
  std::array<tech::TileType, 2> out{tech::kSpace, tech::kSpace};
  std::size_t n = 0;
  for (std::size_t t = 0; t < tech::kMaxTileTypes && n < 2; ++t) {
    if (c.residues.test(static_cast<tech::TileType>(t))) {
      out.at(n++) = static_cast<tech::TileType>(t);
    }
  }
  return out;
}

diag::Status contact_paint(const edit::Target& t, tech::TileType type, const tech::ContactInfo& c) {
  const tech::Technology& tech = *t.s.tech;
  const auto [r1, r2] = lowest_residues(c);
  const auto p1 = tech.home_plane(r1);
  const auto p2 = tech.home_plane(r2);
  if (!p1 || !p2) {
    return diag::Done{};
  }
  std::vector<Rect> targets;
  for (const Rect& a : tiles_of(*t.def, *p1, t.area, tech::TypeMask::of(r1))) {
    for (const Rect& b : tiles_of(*t.def, *p2, a, tech::TypeMask::of(r2))) {
      if (!b.is_null()) {
        targets.push_back(b);
      }
    }
  }
  for (const Rect& r : targets) {
    if (auto st = paint(*t.def, tech, r, type, edit::paint_options(t.s)); !st.ok()) {
      return st;
    }
  }
  return diag::Done{};
}

diag::Status contact_erase(const edit::Target& t, tech::TileType type, const tech::ContactInfo& c) {
  const tech::Technology& tech = *t.s.tech;
  tech::TypeMask holders = tech::TypeMask::of(type);
  for (const tech::ContactInfo& s : tech.contacts()) {
    if (s.stacked && (s.component_a == type || s.component_b == type)) {
      holders.set(s.type);
    }
  }
  const auto home = tech.home_plane(type);
  if (!home) {
    return diag::Done{};
  }
  for (const Rect& r : tiles_of(*t.def, *home, t.area, holders)) {
    if (auto st = erase_type(*t.def, tech, r, type); !st.ok()) {
      return st;
    }
    for (std::size_t res = tech::kFirstUserType; res < tech.user_type_end(); ++res) {
      if (c.residues.test(static_cast<tech::TileType>(res))) {
        if (auto st = paint_type(*t.def, tech, r, static_cast<tech::TileType>(res)); !st.ok()) {
          return st;
        }
      }
    }
  }
  return diag::Done{};
}

CommandResult cmd_contact(CommandContext& ctx, const Invocation& inv) {
  const bool erasing = inv.args.size() == 2 && inv.args[0] == "erase";
  if (inv.args.size() != 1 && !erasing) {
    return edit::usage(inv.args.size() == 2 ? "Usage: contact erase <contact_type>" : "Usage: contact <contact_type>");
  }
  auto t = edit::paint_target(ctx, "The cell in the window is not editable.");
  if (!t.ok()) {
    return cu::fail(t.error());
  }
  const edit::Target& e = t.value();
  const std::string& name = inv.args.back();
  const auto type = cu::parse_layer(*e.s.tech, name);
  if (!type.ok()) {
    return cu::fail(type.error());
  }
  const tech::ContactInfo* c = e.s.tech->contact(type.value());
  if (c == nullptr) {
    return cu::fail(cu::bad_argument("Error: tile type \"" + name + "\" is not a contact."));
  }
  const auto st = erasing ? contact_erase(e, type.value(), *c) : contact_paint(e, type.value(), *c);
  if (!st.ok()) {
    return cu::fail(st.error());
  }
  edit::finish(e, e.area, c->residues | tech::TypeMask::of(type.value()), edit::Effects{false, false, false});
  return CommandResult::success();
}

// --- polygon -----------------------------------------------------------------------

CommandResult cmd_polygon(CommandContext& ctx, const Invocation& inv) {
  auto s = cu::session(ctx);
  if (!s.ok()) {
    return cu::fail(s.error());
  }
  CellDef* def = s.value().editor->edit_cell;
  if (def == nullptr) {
    return cu::fail(diag::make(codes::kNoEditCell, "No cell being edited"));
  }
  if (inv.args.size() < 7) {
    return edit::usage("Usage:  polygon tiletype x1 y1 x2 y2 [x3 y3 ...] xn yn");
  }
  const auto type = cu::parse_layer(*s.value().tech, inv.args[0]);
  if (!type.ok()) {
    return cu::fail(type.error());
  }
  if (inv.args.size() % 2 == 0) {
    return edit::usage("Unpaired coordinate value");
  }
  std::vector<Point> points;  // reversed, as PaintPolygon builds its path
  for (std::size_t k = (inv.args.size() - 1) / 2; k > 0; --k) {
    const auto x = cu::parse_distance(s.value(), inv.args[2 * k - 1], cmd::Axis::x, false);
    const auto y = cu::parse_distance(s.value(), inv.args[2 * k], cmd::Axis::y, false);
    if (!x.ok() || !y.ok()) {
      return cu::fail(x.ok() ? y.error() : x.error());
    }
    points.push_back(Point{x.value(), y.value()});
  }
  const auto pieces = decompose_polygon(points);
  if (!pieces.ok()) {
    return cu::fail(pieces.error());
  }
  mark_modified(*def);
  Rect bbox = canonical(Rect{points[0].x, points[0].y, points[0].x, points[0].y});
  for (const Point& p : points) {
    bbox = Rect{std::min(bbox.x0, p.x), std::min(bbox.y0, p.y), std::max(bbox.x1, p.x), std::max(bbox.y1, p.y)};
  }
  for (const PolygonPiece& piece : pieces.value()) {
    std::optional<TriangleSpec> tri;
    if (piece.triangle) {
      tri = TriangleSpec{piece.triangle->diagonal, piece.triangle->right_side};
    }
    if (auto st = paint_type(*def, *s.value().tech, piece.area, type.value(), tri); !st.ok()) {
      return cu::fail(st.error());
    }
  }
  recompute_bbox(*def, s.value().magic_compat);
  s.value().editor->windows.area_changed(*def, bbox, tech::TypeMask::of(type.value()));
  return CommandResult::success();
}

// --- splitpaint / spliterase --------------------------------------------------------

std::string corner_name(TriangleCorner c) {
  switch (c) {
    case TriangleCorner::ne:
      return "ne";
    case TriangleCorner::se:
      return "se";
    case TriangleCorner::sw:
      return "sw";
    case TriangleCorner::nw:
      break;
  }
  return "nw";
}

// The corner a direction names; Manhattan names and center map as Magic
// does with magic-compat ON, and fail with it OFF (Q-CMDEDIT-21).
diag::Outcome<TriangleCorner> split_corner(const cu::Session& s, std::string_view word) {
  const auto d = cmd::parse_direction(word);
  if (!d.ok()) {
    return d.error();
  }
  using D = cmd::Direction;
  constexpr std::array<std::pair<D, TriangleCorner>, 9> kMap = {{{D::northeast, TriangleCorner::ne},
                                                                 {D::northwest, TriangleCorner::nw},
                                                                 {D::southeast, TriangleCorner::se},
                                                                 {D::southwest, TriangleCorner::sw},
                                                                 {D::north, TriangleCorner::nw},
                                                                 {D::east, TriangleCorner::ne},
                                                                 {D::south, TriangleCorner::se},
                                                                 {D::west, TriangleCorner::sw},
                                                                 {D::center, TriangleCorner::nw}}};
  const auto* it = std::find_if(kMap.begin(), kMap.end(), [&](const auto& m) { return m.first == d.value(); });
  if (cmd::is_manhattan(d.value()) || d.value() == D::center) {
    if (!s.magic_compat) {
      return cu::bad_argument("Direction \"" + std::string{word} + "\" is not a corner; use ne, nw, se or sw");
    }
    cu::warn(s, "Direction \"" + std::string{word} + "\" is not a corner; using " + corner_name(it->second));
  }
  return it->second;
}

// The triangle of the root box with its right angle at `corner`, in the
// edit cell's frame.
TriangleSide edit_side(const cu::Session& s, TriangleCorner corner) {
  const TriangleSide root = triangle_side(corner);
  return transform_triangle(invert(s.editor->edit_transform), root.diagonal, root.right_side);
}

diag::Outcome<tech::TypeMask> split_layers(const cu::Session& s, std::string_view text) {
  auto l = layers(s, text);
  if (!l.ok()) {
    return l.error();
  }
  tech::TypeMask m = paintable(s, l.value().types) & active(s);
  m.clear_type(tech::kSpace);
  return m;
}

// Paints each type of `types` as the triangle `right` of the box.
diag::Status paint_triangles(const edit::Target& e, Diagonal diagonal, bool right, const tech::TypeMask& types) {
  for (std::size_t ty = 1; ty < e.s.tech->type_count(); ++ty) {
    if (!types.test(static_cast<tech::TileType>(ty))) {
      continue;
    }
    if (auto st = paint_triangle(*e.def, *e.s.tech, e.area, diagonal, right, static_cast<tech::TileType>(ty),
                                 edit::paint_options(e.s));
        !st.ok()) {
      return st;
    }
  }
  return diag::Done{};
}

CommandResult cmd_splitpaint(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.size() < 2 || inv.args.size() > 3) {
    return edit::usage("Usage: splitpaint dir layer [layer2]");
  }
  auto t = edit::paint_target(ctx, kNoEditDef);
  if (!t.ok()) {
    return cu::fail(t.error());
  }
  const edit::Target& e = t.value();
  const auto corner = split_corner(e.s, inv.args[0]);
  if (!corner.ok()) {
    return cu::fail(corner.error());
  }
  const TriangleSide side = edit_side(e.s, corner.value());
  tech::TypeMask painted;
  for (std::size_t i = 1; i < inv.args.size() && !e.area.is_null(); ++i) {
    const auto types = split_layers(e.s, inv.args[i]);
    if (!types.ok()) {
      return cu::fail(types.error());
    }
    painted |= types.value();
    const bool right = i == 1 ? side.right_side : !side.right_side;
    if (auto st = paint_triangles(e, side.diagonal, right, types.value()); !st.ok()) {
      return cu::fail(st.error());
    }
  }
  edit::finish(e, e.area, painted, edit::Effects{false, true, false});
  return CommandResult::success();
}

CommandResult cmd_spliterase(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.empty() || inv.args.size() > 2) {
    return edit::usage("Usage: spliterase dir [layer]");
  }
  auto t = edit::paint_target(ctx, kNoEditDef);
  if (!t.ok()) {
    return cu::fail(t.error());
  }
  const edit::Target& e = t.value();
  const auto corner = split_corner(e.s, inv.args[0]);
  if (!corner.ok()) {
    return cu::fail(corner.error());
  }
  const std::string text = inv.args.size() < 2 ? "*" : (inv.args[1] == "space" ? "*,labels" : inv.args[1]);
  const auto types = split_layers(e.s, text);
  if (!types.ok()) {
    return cu::fail(types.error());
  }
  const TriangleSide side = edit_side(e.s, corner.value());
  for (std::size_t ty = 1; ty < e.s.tech->type_count() && !e.area.is_null(); ++ty) {
    if (types.value().test(static_cast<tech::TileType>(ty))) {
      if (auto st = erase_type(*e.def, *e.s.tech, e.area, static_cast<tech::TileType>(ty),
                               TriangleSpec{side.diagonal, side.right_side});
          !st.ok()) {
        return cu::fail(st.error());
      }
    }
  }
  edit::finish(e, e.area, types.value(), edit::Effects{false, true, false});
  return CommandResult::success();
}

// --- drop ----------------------------------------------------------------------------

// One place `drop` paints: the cell and the area in its coordinates.
struct DropSite {
  CellDef* def = nullptr;
  Rect area;
};

std::vector<DropSite> drop_sites(const edit::Target& t, const CellDef& selection) {
  std::vector<DropSite> sites;
  const Transform root_to_edit = invert(t.s.editor->edit_transform);
  for (std::size_t p = 1; p < selection.plane_count(); ++p) {
    selection.plane(static_cast<tech::PlaneId>(p)).enumerate(whole_plane(), all_but_space_and_drc(),
                                                              [&](const TileView& sel) {
      const Rect a = apply(root_to_edit, sel.area);
      edit::search_tree(*t.s.editor, *t.def, a, tech::TypeMask::of(sel.type), edit::Depth::all,
                        [&](const edit::FoundTile& f) {
        const Rect local = clip(apply(invert(f.to_root), f.area), apply(invert(f.to_root), a));
        if (!local.is_null()) {
          sites.push_back(DropSite{const_cast<CellDef*>(f.def), local});  // NOLINT: the edit hierarchy is mutable
        }
      });
      return true;
    });
  }
  return sites;
}

CommandResult cmd_drop(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.size() != 1) {
    return edit::usage("Usage: drop layers");
  }
  auto t = edit::paint_target(ctx, kNoEditDef);
  if (!t.ok()) {
    return cu::fail(t.error());
  }
  const edit::Target& e = t.value();
  auto list = layers(e.s, inv.args[0]);
  if (!list.ok()) {
    return cu::fail(list.error());
  }
  const select::Selection& sel = e.s.editor->selection;
  if (sel.root() != edit::edit_root(*e.s.editor)) {
    return cu::fail(diag::make(codes::kNotEditable, "The selection does not match the edit cell."));  // Q-CMDEDIT-17
  }
  if (sel.def() == nullptr || sel.empty()) {
    return CommandResult::success();
  }
  const std::vector<DropSite> sites = drop_sites(e, *sel.def());
  for (const DropSite& site : sites) {
    if (site.def->flags().no_edit) {
      return cu::fail(diag::make(codes::kNotEditable, "Cannot drop into non-editable cell " + site.def->name()));
    }
  }
  const tech::TypeMask types = paintable(e.s, list.value().types);
  for (const DropSite& site : sites) {
    edit::Target here{e.s, site.def, e.root_area, site.area};
    if (auto st = paint_all(here, site.area, types); !st.ok()) {
      return cu::fail(st.error());
    }
    recompute_bbox(*site.def, e.s.magic_compat);
  }
  edit::finish(e, e.area, types, edit::Effects{false, false, false});
  return CommandResult::success();
}

}  // namespace

namespace cmdutil {

bool register_paint_commands(cmd::CommandRegistry& registry) {
  bool ok = register_box_commands(registry);
  ok = add(registry, "paint", "paint layers | cursor | pick x y", "Paints the layers into the box area of the edit cell.",
           cmd_paint) && ok;
  ok = add(registry, "erase", "erase ?layers | cursor | pick x y?",
           "Erases the layers (default everything) from the box area.", cmd_erase) && ok;
  ok = add(registry, "fill", "fill direction ?layers?", "Extends paint across the box.", cmd_fill) && ok;
  ok = add(registry, "corner", "corner direction1 direction2 ?layers? ?bevel?", "Paints corners of wires in the box.",
           cmd_corner) && ok;
  ok = add(registry, "contact", "contact ?erase? type", "Paints or erases a contact where its layers overlap.",
           cmd_contact) && ok;
  ok = add(registry, "polygon", "polygon type x1 y1 x2 y2 x3 y3 ?xn yn ...?", "Paints a polygon in the edit cell.",
           cmd_polygon) && ok;
  ok = add(registry, "splitpaint", "splitpaint dir layer ?layer2?",
           "Paints a triangle of the box (right angle at dir) and optionally the other half.", cmd_splitpaint) && ok;
  ok = add(registry, "spliterase", "spliterase dir ?layer?", "Erases a triangle of the box.", cmd_spliterase) && ok;
  ok = add(registry, "drop", "drop layers", "Paints layers under the selected paint in its cells.", cmd_drop) && ok;
  return ok;
}

}  // namespace cmdutil

}  // namespace hefesto::db
