// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `findlabel` and `goto` (cmd-labels-properties "Findlabel command", "Goto
// command"): move the box to a label found by its hierarchical name, or to
// a node named by an instance path and a label or an extractor node name.
// Ported from Magic VLSI 8.3.694 (fc5daa9e) commands/CmdFI.c
// (CmdFindLabel, dbListLabels, CmdFindNetProc, CmdGoto, findTile) and
// database/DBlabel2.c (DBSrLabelLoc). Magic is Copyright (C) 1985, 1990
// Regents of the University of California (permissive license,
// THIRD_PARTY_NOTICES.md).

#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/cell/hier.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "io_session.hpp"

namespace hefesto::cell::detail {

namespace {

using cmd::CommandResult;
using cmd::Value;

/// The box set by ToolMoveBox + ToolMoveCorner: `r` in `root`'s
/// coordinates, a zero width or height widened by one unit.
void move_box(editor::EditorState& ed, db::CellDef* root, db::Rect r) {
  if (r.x0 == r.x1) {
    ++r.x1;
  }
  if (r.y0 == r.y1) {
    ++r.y1;
  }
  ed.box = r;
  ed.box_root = root;
}

// --- findlabel ---------------------------------------------------------------------

struct FindArgs {
  bool glob = false;
  std::string name;
  int occurrence = -1;  // -1: none given (or not an integer)
};

std::optional<FindArgs> parse_find(const std::vector<std::string>& args) {
  FindArgs a;
  std::size_t plain = args.size() + 1;  // Magic's argc
  if (plain > 2 && args[0].starts_with("-glob")) {
    --plain;
    a.glob = true;
  }
  if (plain != 2 && plain != 3) {
    return std::nullopt;
  }
  if (plain == 3) {
    const std::string& last = args.back();
    int value = 0;
    int used = 0;
    if (std::sscanf(last.c_str(), "%d%n", &value, &used) == 1 && static_cast<std::size_t>(used) == last.size()) {
      a.occurrence = value;
    }
  }
  a.name = args[a.glob ? 1 : 0];
  return a;
}

// dbListLabels: every label whose hierarchical name matches; with an
// occurrence N only the N-th is listed and the box goes to it.
CommandResult find_glob(editor::EditorState& ed, db::CellDef& def, const FindArgs& a) {
  Value::List names;
  int occur = a.occurrence;
  db::search_labels(def, db::whole_plane(), [&](const db::LabelHit& hit) {
    const std::string full = hit.path + hit.label->text;
    if (!db::glob_match(a.name, full)) {
      return true;
    }
    if (occur == 0 || occur == -1) {
      names.emplace_back(full);
      if (occur == 0) {
        occur = -2;
        move_box(ed, &def, db::apply(hit.to_root, hit.label->rect));
      }
    }
    if (occur > 0) {
      --occur;
    }
    return true;
  });
  return CommandResult::success(Value{std::move(names)});
}

CommandResult find_exact(editor::EditorState& ed, db::CellDef& def, const FindArgs& a) {
  int occur = a.occurrence < 0 ? 0 : a.occurrence;
  std::optional<db::Rect> found;
  db::search_labels(def, db::whole_plane(), [&](const db::LabelHit& hit) {
    if (hit.path + hit.label->text != a.name) {
      return true;
    }
    if (occur-- == 0) {
      found = db::apply(hit.to_root, hit.label->rect);
      return false;
    }
    return true;
  });
  if (!found) {
    return fail(db::codes::kNoLabel, "Couldn't find label " + a.name);
  }
  move_box(ed, &def, *found);
  return CommandResult::success();
}

CommandResult findlabel_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  const auto a = parse_find(inv.args);
  if (!a) {
    return fail(codes::kBadUsage, "Usage: findlabel [-glob] label_name");
  }
  return with_session(ctx, [&](IoSession& s) {
    view::LayoutWindow* w = s.ed.windows.current();
    if (w == nullptr || w->root() == nullptr) {
      return fail(codes::kBadUsage, "Point to a window first.");
    }
    if (!s.ed.box) {
      return fail(db::codes::kNoBox, "Put the box in a window first.");
    }
    if (s.ed.box_root != nullptr && s.ed.box_root != w->root()) {
      return fail(db::codes::kNoBox, "The box is not in the same coordinate system as the window.");
    }
    // Labels are searched from the edit cell (else the window's root), and
    // the box lands in that cell's coordinates, as in Magic.
    db::CellDef& def = s.ed.edit_cell != nullptr ? *s.ed.edit_cell : *w->root();
    expand_all(s.io, def);
    return a->glob ? find_glob(s.ed, def, *a) : find_exact(s.ed, def, *a);
  });
}

// --- goto -------------------------------------------------------------------------

/// The node of CmdFindNetProc: its area in the window's root coordinates
/// and its type.
struct Node {
  db::Rect area;
  tech::TileType type = tech::kSpace;
};

// An integer with an optional leading 'n' meaning negative, as the
// extractor prints node coordinates; the characters used, 0 when none.
std::size_t scan_signed(std::string_view text, int& value) {
  const bool negative = !text.empty() && text.front() == 'n';
  const std::string rest{text.substr(negative ? 1 : 0)};
  int used = 0;
  if (std::sscanf(rest.c_str(), "%d%n", &value, &used) != 1) {
    return 0;
  }
  value = negative ? -value : value;
  return static_cast<std::size_t>(used) + (negative ? 1 : 0);
}

// "PLANE_X_Y" (extract/extBasic.c extMakeNodeNumPrint): the plane and the
// unit square at (X, Y).
std::optional<std::pair<tech::PlaneId, db::Rect>> generated_node(const tech::Technology& tech, std::string_view leaf) {
  const std::size_t us = leaf.find('_');
  if (us == std::string_view::npos) {
    return std::nullopt;
  }
  const std::string_view plane_name = leaf.substr(0, us);
  for (std::size_t p = tech::kFirstUserPlane; p < tech.plane_count(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    if (tech.plane_name(plane, tech::NameForm::short_name) != plane_name) {
      continue;
    }
    std::string_view rest = leaf.substr(us + 1);
    int x = 0;
    const std::size_t xn = scan_signed(rest, x);
    const std::size_t us2 = rest.find('_');
    int y = 0;
    if (xn == 0 || us2 == std::string_view::npos || scan_signed(rest.substr(us2 + 1), y) == 0) {
      return std::nullopt;
    }
    return std::pair{plane, db::Rect{x, y, x + 1, y + 1}};
  }
  return std::nullopt;
}

// The legacy "P_X_Y" form: X and Y carry the sign in the low bit.
std::optional<std::pair<tech::PlaneId, db::Rect>> legacy_node(std::string_view leaf) {
  int p = 0;
  int x = 0;
  int y = 0;
  const std::string text{leaf};
  if (std::sscanf(text.c_str(), "%d_%d_%d", &p, &x, &y) != 3 || p < 0) {
    return std::nullopt;
  }
  x = ((x & 1) != 0 ? -1 : 1) * x / 2;
  y = ((y & 1) != 0 ? -1 : 1) * y / 2;
  return std::pair{static_cast<tech::PlaneId>(p), db::Rect{x, y, x + 1, y + 1}};
}

// findTile: the type of the first tile of `plane` at `area` (the left side
// of a split tile).
tech::TileType type_at(const db::CellDef& def, tech::PlaneId plane, const db::Rect& area) {
  tech::TileType type = tech::kSpace;
  if (plane >= def.plane_count()) {
    return type;
  }
  def.plane(plane).enumerate(area, tech::TypeMask::all(), [&](const db::TileView& t) {
    type = t.split ? t.left_type : t.type;
    return false;
  });
  return type;
}

std::optional<Node> find_node(IoSession& s, db::CellDef& root, const std::string& name, bool& use_found) {
  use_found = true;
  db::CellDef* def = &root;
  db::Transform to_root;
  std::string_view leaf = name;
  if (const std::size_t slash = name.rfind('/'); slash != std::string::npos) {
    const auto inst = find_instance(root, std::string_view{name}.substr(0, slash), [&s](db::CellDef& d) {
      ReadOptions opts;
      opts.ignore_tech = true;
      (void)read_cell(s.io, d, opts);
    });
    if (inst) {
      def = inst->use->def;
      to_root = inst->to_root;
      leaf = std::string_view{name}.substr(slash + 1);
    } else {
      use_found = false;  // the slash is taken as part of the name
    }
  }
  auto located = generated_node(*s.ed.technology, leaf);
  if (!located) {
    located = legacy_node(leaf);
  }
  if (located) {
    return Node{db::apply(to_root, located->second), type_at(*def, located->first, located->second)};
  }
  for (const db::Label& l : def->labels()) {
    if (l.text == leaf) {
      return Node{db::apply(to_root, l.rect), l.type};
    }
  }
  return std::nullopt;
}

CommandResult goto_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  std::size_t argc = inv.args.size() + 1;
  bool nocomplain = false;
  if (argc == 3 && inv.args[1].starts_with("-noco")) {
    nocomplain = true;
    --argc;
  }
  if (argc != 2) {
    return fail(codes::kBadUsage, "usage: goto nodename [-nocomplain]");
  }
  return with_session(ctx, [&](IoSession& s) {
    view::LayoutWindow* w = s.ed.windows.current();
    if (w == nullptr || w->root() == nullptr) {
      return fail(codes::kBadUsage, "Put the cursor in a layout window");
    }
    bool use_found = true;
    const auto node = find_node(s, *w->root(), inv.args[0], use_found);
    if (!node) {
      if (nocomplain) {
        return CommandResult::success();
      }
      std::string text = "Couldn't find label " + inv.args[0];
      if (!use_found) {
        text += "\nCouldn't find use referenced in hierarchical name";
      }
      return fail(db::codes::kNoLabel, std::move(text));
    }
    s.ed.box = node->area;
    s.ed.box_root = w->root();
    return CommandResult::success(Value{std::string{s.ed.technology->type_name(node->type)}});
  });
}

}  // namespace

bool register_find_commands(cmd::CommandRegistry& registry) {
  bool ok = !registry
                 .add(cmd::CommandSpec{"findlabel", "findlabel [-glob] label_name [occurrence]",
                                       "Moves the box to a label by its hierarchical name (-glob: lists the matching "
                                       "names).",
                                       findlabel_cmd})
                 .has_value();
  ok = !registry
            .add(cmd::CommandSpec{"goto", "goto nodename [-nocomplain]",
                                  "Moves the box to a node (instance path, then a label or an extracted node name) "
                                  "and returns its type.",
                                  goto_cmd})
            .has_value() &&
       ok;
  return ok;
}

}  // namespace hefesto::cell::detail
