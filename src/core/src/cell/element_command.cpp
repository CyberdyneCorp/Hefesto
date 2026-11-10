// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `element` (cmd-labels-properties "Element command"): named display
// elements (lines, rectangles, text) kept with the window's root cell
// (db::CellDef::elements; persistent ones are written to the `.mag` file).
// Element names are global, as in Magic's element hash table. Elements are
// not undoable.
// Ported from Magic VLSI 8.3.694 (fc5daa9e) commands/CmdE.c (CmdElement)
// and dbwind/DBWelement.c (DBWElementAdd*, DBWElementDelete,
// DBWElementNames, DBWElementInbox, DBWElementText, DBWElementStyle,
// DBWElementPos, DBWElementParseFlags). Magic is Copyright (C) 1985, 1990
// Regents of the University of California (permissive license,
// THIRD_PARTY_NOTICES.md).

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "db/command_util.hpp"
#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/util/magic_hash_table.hpp"
#include "io_session.hpp"

namespace hefesto::cell::detail {

namespace {

using cmd::CommandResult;
using cmd::Value;
namespace cu = db::cmdutil;

constexpr std::array<std::string_view, 6> kOptions = {"add", "delete", "configure", "names", "inbox", "help"};
constexpr std::array<std::string_view, 6> kOptionHelp = {
    "add\t\t\tcreate a new element",           "delete\t\tdelete an existing element",
    "configure\t\tconfigure or query an existing element", "names\t\tprint names of all elements",
    "inbox\t\tprint name of element nearest the box", "help\t\tprint help information"};
constexpr std::array<std::string_view, 3> kTypes = {"line", "rectangle", "text"};
constexpr std::array<std::string_view, 3> kTypeHelp = {"line\t\tname style x1 y1 x2 y2",
                                                       "rectangle\t\tname style llx lly urx ury",
                                                       "text\t\tname style cx cy label"};
constexpr std::array<std::string_view, 4> kConfigure = {"text", "style", "position", "flags"};
constexpr std::array<std::string_view, 4> kConfigureHelp = {
    "text\t\tget (or) replace <string>", "style\t\tget (or) add <style> (or) remove <style>",
    "position\t\tget (or) <point> (or) <rect>", "flags\t\tset element flags"};

template <std::size_t N>
std::string listing(std::string head, const std::array<std::string_view, N>& lines) {
  for (const std::string_view l : lines) {
    head += "\n    " + std::string{l};
  }
  return head;
}

struct Call {
  IoSession& s;
  cu::Session cs;
  db::CellDef& root;
  const std::vector<std::string>& argv;  // Magic's argv without the command name
};

/// The element called `name` in any cell, with the cell holding it.
std::pair<db::CellDef*, db::Element*> find_element(const IoSession& s, std::string_view name) {
  std::pair<db::CellDef*, db::Element*> out{nullptr, nullptr};
  s.ed.cells.for_each([&](db::CellDef& def) {
    for (db::Element& e : def.elements()) {
      if (out.second == nullptr && e.name == name) {
        out = {&def, &e};
      }
    }
  });
  return out;
}

void mark_if_persistent(db::CellDef& def, const db::Element& e) {
  if (e.persistent) {
    def.flags().modified = true;
  }
}

// DBWElementDelete: silently nothing when there is no such element.
void delete_element(IoSession& s, std::string_view name) {
  auto [def, elem] = find_element(s, name);
  if (elem == nullptr) {
    return;
  }
  mark_if_persistent(*def, *elem);
  std::erase_if(def->elements(), [&](const db::Element& e) { return e.name == name; });
}

// A style number or long style name as the long name stored with the
// element; nullopt after "Unknown style".
std::optional<std::string> style_name(const Call& c, const std::string& word) {
  const auto& styles = c.s.ed.styles;
  if (cu::str_is_int(word)) {
    const auto index = cu::c_atoi(word);
    if (styles && index >= 0 && static_cast<std::size_t>(index) < styles->table.styles.size()) {
      if (const auto& name = styles->table.styles[static_cast<std::size_t>(index)].long_name) {
        return *name;
      }
    }
    return word;  // no style table (headless): the number stands for itself
  }
  if (styles && !display::style_from_name(styles->table, word)) {
    cu::warn(c.cs, "Unknown style \"" + word + "\".Use a number or one of the long names in the .dstyle file");
    return std::nullopt;
  }
  return word;
}

db::Coord coord(const Call& c, const std::string& word, bool is_x) {
  return cu::scaled_coord(c.cs, word, 1, is_x, false);
}

// --- add -------------------------------------------------------------------------

CommandResult element_add(const Call& c) {
  if (c.argv.size() < 2) {
    cu::warn(c.cs, "Usage:  element add <type> <name> <values...>");
    return CommandResult::success();
  }
  const int type = cu::lookup_in(c.argv[1], kTypes);
  if (type < 0) {
    cu::warn(c.cs, listing("Usage:  element add <type>..., where type is one of:", kTypeHelp));
    return CommandResult::success();
  }
  const std::size_t want = type == 2 ? 7 : 8;  // Magic's argc minus one
  const std::string usage = "Usage:  element add " + std::string{kTypeHelp[static_cast<std::size_t>(type)]};
  if (c.argv.size() < 6) {
    cu::warn(c.cs, usage);
    return CommandResult::success();
  }
  const auto style = style_name(c, c.argv[3]);
  if (!style) {
    return CommandResult::success();
  }
  if (c.argv.size() != want) {
    cu::warn(c.cs, usage);
    return CommandResult::success();
  }
  db::Element e;
  e.name = c.argv[2];
  e.styles = {*style};
  e.persistent = false;  // temporary by default
  const db::Coord x0 = coord(c, c.argv[4], true);
  const db::Coord y0 = coord(c, c.argv[5], false);
  if (type == 2) {
    e.kind = db::Element::Kind::text;
    e.area = db::Rect{x0, y0, x0, y0};
    e.text = c.argv[6];
    e.text_position = 0;  // centered
    e.text_size = 1;      // medium
  } else {
    e.kind = type == 0 ? db::Element::Kind::line : db::Element::Kind::rectangle;
    e.area = db::canonical(db::Rect{x0, y0, coord(c, c.argv[6], true), coord(c, c.argv[7], false)});
  }
  delete_element(c.s, e.name);  // an element of the same name is replaced
  c.root.elements().push_back(std::move(e));
  return CommandResult::success();
}

// --- configure -------------------------------------------------------------------

CommandResult configure_text(const Call& c, db::Element& e, bool get) {
  if (e.kind != db::Element::Kind::text) {
    cu::warn(c.cs, "Element " + e.name + " is not a text element");
    return CommandResult::success();
  }
  if (get) {
    return CommandResult::success(Value{e.text});
  }
  if (c.argv.size() > 5) {
    cu::warn(c.cs, "Usage:  element configure <name> text replace <string>");
    return CommandResult::success();
  }
  e.text = c.argv.back();
  return CommandResult::success();
}

CommandResult configure_style(const Call& c, db::CellDef& def, db::Element& e, bool get) {
  if (get) {
    return CommandResult::success(Value{Value::List{e.styles.begin(), e.styles.end()}});
  }
  if (c.argv.size() != 5) {
    cu::warn(c.cs, "Usage: element configure <name> style add|remove  <style>");
    return CommandResult::success();
  }
  const auto style = style_name(c, c.argv[4]);
  if (!style) {
    return CommandResult::success();
  }
  if (c.argv[3].starts_with("add")) {
    e.styles.push_back(*style);
  } else if (e.styles.size() > 1) {
    // DBWElementStyle refuses to remove the last style.
    if (auto it = std::find(e.styles.begin(), e.styles.end(), *style); it != e.styles.end()) {
      e.styles.erase(it);
    }
  }
  mark_if_persistent(def, e);
  return CommandResult::success();
}

CommandResult configure_position(const Call& c, db::CellDef& def, db::Element& e, bool get) {
  if (get) {
    Value::List v{Value{std::int64_t{e.area.x0}}, Value{std::int64_t{e.area.y0}}};
    if (e.kind != db::Element::Kind::text) {
      v.emplace_back(std::int64_t{e.area.x1});
      v.emplace_back(std::int64_t{e.area.y1});
    }
    return CommandResult::success(Value{std::move(v)});
  }
  if (c.argv.size() != 5 && c.argv.size() != 7) {
    cu::warn(c.cs, "Usage: element configure <name> position <x> <y> [<x2> <y2>]");
    return CommandResult::success();
  }
  db::Rect r{coord(c, c.argv[3], true), coord(c, c.argv[4], false), 0, 0};
  r.x1 = c.argv.size() == 7 ? coord(c, c.argv[5], true) : r.x0;
  r.y1 = c.argv.size() == 7 ? coord(c, c.argv[6], false) : r.y0;
  e.area = r;
  mark_if_persistent(def, e);
  return CommandResult::success();
}

// DBWElementParseFlags for a line.
bool parse_line_flag(db::Element& e, std::string_view word) {
  constexpr std::array<std::string_view, 12> kLine = {"halfx",     "halfy",       "exactx",     "exacty",
                                                      "arrowleft", "arrowbottom", "arrowright", "arrowtop",
                                                      "plainleft", "plainbottom", "plainright", "plaintop"};
  const int idx = cu::lookup_in(word, kLine);
  switch (idx) {
    case 0: case 2: e.half_x = idx == 0; return true;
    case 1: case 3: e.half_y = idx == 1; return true;
    case 4: case 5: e.arrow_left = true; return true;
    case 6: case 7: e.arrow_right = true; return true;
    case 8: case 9: e.arrow_left = false; return true;
    case 10: case 11: e.arrow_right = false; return true;
    default: return false;
  }
}

// DBWElementParseFlags for text: a size, else a position.
bool parse_text_flag(db::Element& e, std::string_view word) {
  constexpr std::array<std::string_view, 5> kSizes = {"small", "medium", "large", "xlarge", "default"};
  if (const int size = cu::lookup_in(word, kSizes); size >= 0) {
    e.text_size = static_cast<std::uint8_t>(size);
    return true;
  }
  const int pos = db::parse_position(word, false);
  if (pos < 0) {
    return false;
  }
  e.text_position = static_cast<std::uint8_t>(pos);
  return true;
}

CommandResult configure_flags(const Call& c, db::CellDef& def, db::Element& e, bool get) {
  if (get) {
    cu::warn(c.cs, "(unimplemented function)");
    return CommandResult::success();
  }
  const std::string& word = c.argv[3];
  const int general = cu::lookup(word, {"persistent", "temporary"});
  if (general >= 0) {
    // Persistent elements belong to the cell file: the cell changes either way.
    def.flags().modified = def.flags().modified || e.persistent || general == 0;
    e.persistent = general == 0;
    return CommandResult::success();
  }
  constexpr std::array<std::string_view, 3> kKinds = {"rect", "line", "text"};
  bool ok = false;
  if (e.kind == db::Element::Kind::line) {
    ok = parse_line_flag(e, word);
  } else if (e.kind == db::Element::Kind::text) {
    ok = parse_text_flag(e, word);
  }
  if (!ok) {
    cu::warn(c.cs, "No such " + std::string{kKinds[static_cast<std::size_t>(e.kind)]} + " element flag \"" + word +
                       "\"");
    return CommandResult::success();
  }
  mark_if_persistent(def, e);
  return CommandResult::success();
}

CommandResult element_configure(const Call& c) {
  const int type = c.argv.size() >= 3 ? cu::lookup_in(c.argv[2], kConfigure) : -1;
  if (c.argv.size() < 3) {
    cu::warn(c.cs, "Usage:  element configure <name> <option...>");
    return CommandResult::success();
  }
  if (type < 0) {
    cu::warn(c.cs, listing("Usage:  element configure <name> <option...>, where option is one of:", kConfigureHelp));
    return CommandResult::success();
  }
  auto [def, elem] = find_element(c.s, c.argv[1]);
  if (elem == nullptr) {
    cu::warn(c.cs, "No such element " + c.argv[1]);
    return CommandResult::success();
  }
  const bool get = c.argv.size() == 3 || c.argv[3].starts_with("get");
  switch (type) {
    case 0: return configure_text(c, *elem, get);
    case 1: return configure_style(c, *def, *elem, get);
    case 2: return configure_position(c, *def, *elem, get);
    default:
      if (!get && c.argv.size() < 4) {
        cu::warn(c.cs, "Usage: element configure <name> flags (flag)");
        return CommandResult::success();
      }
      return configure_flags(c, *def, *elem, get);
  }
}

// --- names, inbox ------------------------------------------------------------------

/// Every element name in the order of Magic's element hash table (10
/// initial buckets), cells visited in the library order.
std::vector<std::string> element_names(const IoSession& s, const std::optional<db::Rect>& inside) {
  util::MagicHashTable<bool> table{10};
  s.ed.cells.for_each([&](db::CellDef& def) {
    for (const db::Element& e : def.elements()) {
      if (!inside || db::surround(*inside, e.area)) {
        table.insert(e.name, true);
      }
    }
  });
  return table.keys();
}

CommandResult names_result(std::vector<std::string> names) {
  return CommandResult::success(Value{Value::List{names.begin(), names.end()}});
}

CommandResult element_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return with_session(ctx, [&](IoSession& s) {
    view::LayoutWindow* w = s.ed.windows.current();
    auto cs = cu::session(ctx);
    if (w == nullptr || w->root() == nullptr || !cs.ok()) {
      return CommandResult::success();  // Magic returns silently without a window
    }
    const Call c{s, cs.value(), *w->root(), inv.args};
    int option = inv.args.empty() ? 5 : cu::lookup_in(inv.args[0], kOptions);
    option = option < 0 ? 5 : option;
    switch (option) {
      case 0: return element_add(c);
      case 1:
        if (inv.args.size() != 2) {
          cu::print(c.cs, "Usage:  element delete <name>");
          return CommandResult::success();
        }
        delete_element(s, inv.args[1]);
        return CommandResult::success();
      case 2: return element_configure(c);
      case 3: return names_result(element_names(s, std::nullopt));
      case 4:
        if (!s.ed.box) {
          cu::warn(c.cs, "Box tool must be present");
          return CommandResult::success();
        }
        return names_result(element_names(s, s.ed.box));
      default:
        cu::print(c.cs, listing("", kOptionHelp).substr(1));
        return CommandResult::success();
    }
  });
}

}  // namespace

bool register_element_command(cmd::CommandRegistry& registry) {
  return !registry
              .add(cmd::CommandSpec{"element", "element add|delete|configure|names|inbox|help ?args?",
                                    "Adds, deletes, configures or lists the display elements (lines, rectangles, "
                                    "text) of the window's root cell.",
                                    element_cmd})
              .has_value();
}

}  // namespace hefesto::cell::detail
