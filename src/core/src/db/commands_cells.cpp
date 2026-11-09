// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// MODEL commands, part 3: cellname, instance, array (minimal Magic forms;
// `array` acts on the instance the last `getcell` placed, standing in for
// the selection until cmd-selection lands in M2).
// (Magic ref: commands/CmdCD.c CmdCellname, commands/CmdAB.c CmdArray,
// select/selOps.c SelectArray)

#include <algorithm>
#include <optional>
#include <utility>

#include "db/command_util.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/util/record_order.hpp"

namespace hefesto::db {

namespace cu = cmdutil;
using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;
using cmd::Value;

namespace {

// --- cellname ------------------------------------------------------------------

bool is_top_cell(const CellDef& def) {
  for (const CellUse* u : def.instances()) {
    if (u->parent != nullptr && !u->parent->flags().internal) {
      return false;
    }
  }
  return true;
}

// CmdCellname's option table (Magic ref: commands/CmdCD.c:1140-1166).
enum CellnameOption {
  kChildren, kParents, kExists, kSelf, kInstances, kChildinst, kCelldef, kAllcells, kTopcells, kWindow,
  kCreate, kDelete, kDereference, kFilepath, kFlags, kTimestamp, kLock, kUnlock, kProperty, kAbutment,
  kOrientation, kPosition, kRename, kWriteable, kModified
};

int cellname_option(std::string_view word) {
  return cu::lookup(word, {"children", "parents", "exists", "self", "instances", "childinst", "celldef", "allcells",
                           "topcells", "window", "create", "delete", "dereference", "filepath", "flags", "timestamp",
                           "lock", "unlock", "property", "abutment", "orientation", "position", "rename",
                           "writeable", "modified"});
}

CommandResult cellname_usage() {
  return cu::fail(cu::bad_argument(
      "Usage: cellname [list] children|parents|self|exists|instances|celldef|delete [name]\n"
      "or:    cellname [list] allcells|topcells|window\n"
      "or:    cellname create name\n"
      "or:    cellname rename name newname [-force]\n"
      "or:    cellname [un]lock [name]\n"
      "or:    cellname writeable [name] [true|false]"));
}

// DBCellPrint for ALLCELLS, TOPCELLS and MODIFIED: natural name order
// (strcmpbynum); a list result with `list`, printed lines otherwise.
// (Magic ref: database/DBcellname.c:750-880)
CommandResult cellname_list(const cu::Session& s, int option, bool dolist) {
  std::vector<std::string> names;
  s.editor->cells.for_each([&](CellDef& def) {
    if (def.flags().internal) {
      return;
    }
    const bool take = option == kAllcells || (option == kModified && def.flags().modified) ||
                      (option == kTopcells && is_top_cell(def));
    if (take) {
      names.push_back(def.name());
    }
  });
  std::sort(names.begin(), names.end(),
            [](const std::string& a, const std::string& b) { return util::compare_natural(a, b) < 0; });
  if (dolist) {
    return CommandResult::success(Value{Value::List{names.begin(), names.end()}});
  }
  std::string text = option == kAllcells ? "Cell currently loaded:" : (option == kModified ? "Modified cells:" : "Top level cells are:");
  for (const std::string& n : names) {
    text += "\n    " + n;
  }
  cu::print(s, std::move(text));
  return CommandResult::success();
}

diag::Outcome<CellDef*> named_cell(const cu::Session& s, const std::string& name) {
  CellDef* def = s.editor->cells.find(name);
  if (def == nullptr) {
    return diag::make(codes::kCellNotFound, "No such cell \"" + name + "\"");
  }
  return def;
}

CommandResult cellname_rename(const cu::Session& s, const std::vector<std::string>& args, bool force) {
  if (args.size() != 3) {
    return cellname_usage();
  }
  auto def = named_cell(s, args[1]);
  if (!def.ok()) {
    return cu::fail(def.error());
  }
  const bool vendor = def.value()->flags().vendor_gds;
  auto st = s.editor->cells.rename(*def.value(), args[2], force);
  if (st.ok() && vendor) {
    cu::print(s, "Warning:  Renaming read-only cell \"" + args[1] + "\"\n"
                 "Read-only status will be revoked and GDS file pointer removed.");
  }
  return st.ok() ? CommandResult::success() : cu::fail(st.error());
}

// DBCellDelete: a window showing the cell shows "(UNNAMED)" afterwards
// (WindUnload). (Magic ref: database/DBcellname.c:228-341)
void unload_windows(const cu::Session& s, const CellDef& doomed) {
  s.editor->windows.for_each([&](view::LayoutWindow& w) {
    if (w.root() != &doomed) {
      return;
    }
    CellDef* unnamed = s.editor->cells.find("(UNNAMED)");
    if (unnamed == nullptr || unnamed == &doomed) {
      auto made = s.editor->cells.create("(UNNAMED)", s.tech->plane_count());
      unnamed = made.ok() ? made.value() : nullptr;
      if (unnamed != nullptr) {
        unnamed->flags().available = true;
      }
    }
    w.set_root(unnamed);
  });
}

CommandResult cellname_delete(const cu::Session& s, const std::vector<std::string>& args) {
  if (args.size() < 2 || (args.size() == 3 && args[2] != "-noprompt")) {
    return cu::fail(cu::bad_argument("Delete cell command missing cellname"));
  }
  auto def = named_cell(s, args[1]);
  if (!def.ok()) {
    return cu::fail(def.error());
  }
  CellDef* doomed = def.value();
  if (auto check = s.editor->cells.check_removable(*doomed); !check.ok()) {
    return cu::fail(check.error());
  }
  if (doomed->name() == "(UNNAMED)") {
    (void)s.editor->cells.rename(*doomed, "__UNNAMED__");
  }
  unload_windows(s, *doomed);
  if (auto st = s.editor->cells.remove(*doomed); !st.ok()) {
    return cu::fail(st.error());
  }
  if (s.editor->edit_cell == doomed) {
    s.editor->edit_cell = nullptr;
    s.editor->current_use.clear();
  }
  if (s.editor->box_root == doomed) {
    s.editor->box_root = nullptr;
    s.editor->box.reset();
  }
  return CommandResult::success();
}

// `cellname writeable [name] [true|false]` (Magic ref: commands/CmdCD.c IDX_READWRITE).
CommandResult cellname_writeable(const cu::Session& s, const std::vector<std::string>& args, bool dolist) {
  CellDef* def = args.size() >= 2 ? s.editor->cells.find(args[1]) : s.editor->edit_cell;
  if (def == nullptr) {
    return cu::fail(cu::bad_argument("Unknown cell " + (args.size() >= 2 ? args[1] : std::string{})));
  }
  if (args.size() < 3) {
    const char* state = def->flags().no_edit ? "read-only" : "writeable";
    if (dolist) {
      return CommandResult::success(Value{std::string{state}});
    }
    cu::print(s, state);
    return CommandResult::success();
  }
  const int yes = cu::lookup(args[2], {"no", "false", "off", "0", "yes", "true", "on", "1"});
  if (yes < 0) {
    return cellname_usage();
  }
  def->flags().no_edit = yes < 4;
  return CommandResult::success();
}

CommandResult cellname_exists(const cu::Session& s, const std::vector<std::string>& args) {
  if (args.size() < 2) {
    const CellDef* def = s.editor->edit_cell;
    return CommandResult::success(Value{def != nullptr ? def->name() : std::string{}});
  }
  return CommandResult::success(Value{std::int64_t{s.editor->cells.find(args[1]) != nullptr ? 1 : 0}});
}

CommandResult cellname_create(const cu::Session& s, const std::vector<std::string>& args) {
  if (args.size() != 2) {
    return cellname_usage();
  }
  auto made = s.editor->cells.create(args[1], s.tech->plane_count());
  if (!made.ok()) {
    return cu::fail(made.error());
  }
  made.value()->flags().available = true;
  return CommandResult::success();
}

struct CellnameArgs {
  std::vector<std::string> args;  // option first
  bool dolist = false;
  bool force = false;
};

std::optional<CellnameArgs> split_cellname(const std::vector<std::string>& in) {
  CellnameArgs c;
  c.args = in;
  if (!c.args.empty() && c.args.front() == "list") {
    c.dolist = true;
    c.args.erase(c.args.begin());
  }
  if (!c.args.empty() && c.args.back().starts_with('-')) {
    const std::string& last = c.args.back();
    if (last == "-list" || last == "-def" || last == "-force") {
      c.dolist = c.dolist || last == "-list";
      c.force = last == "-force";
      c.args.pop_back();
    }
  }
  if (c.args.empty() || c.args.size() > 4) {
    return std::nullopt;
  }
  return c;
}

bool takes_name_only(int option) {
  return option == kAllcells || option == kTopcells || option == kModified || option == kWindow;
}

CommandResult cellname_dispatch(const cu::Session& s, int option, const CellnameArgs& c) {
  switch (option) {
    case kAllcells:
    case kTopcells:
    case kModified:
      return cellname_list(s, option, c.dolist);
    case kExists:
    case kSelf:
      return cellname_exists(s, c.args);
    case kCreate:
      return cellname_create(s, c.args);
    case kDelete:
      return cellname_delete(s, c.args);
    case kRename:
      return cellname_rename(s, c.args, c.force);
    case kWriteable:
      return cellname_writeable(s, c.args, c.dolist);
    case kLock:
    case kUnlock:
      return cu::fail(cu::bad_argument("Cell definitions cannot be locked.  Use \"instance\"?\n"
                                       "  or do you mean \"cellname writeable\"?"));
    default:
      return cu::fail(cu::bad_argument("cellname " + c.args[0] + ": not available in this release"));
  }
}

CommandResult cmd_cellname(CommandContext& ctx, const Invocation& inv) {
  auto s = cu::session(ctx);
  if (!s.ok()) {
    return cu::fail(s.error());
  }
  const auto c = split_cellname(inv.args);
  if (!c) {
    return cellname_usage();
  }
  const int option = cellname_option(c->args[0]);
  const bool many = c->args.size() > 2 && option != kRename && option != kDelete && option != kDereference &&
                    option != kWriteable && option != kProperty && option != kFilepath && option != kOrientation &&
                    option != kTimestamp;
  if (option < 0 || many || (c->args.size() > 3 && option != kProperty) ||
      (c->args.size() > 1 && takes_name_only(option))) {
    return cellname_usage();
  }
  return cellname_dispatch(s.value(), option, *c);
}

// --- instance ------------------------------------------------------------------

diag::Outcome<CellUse*> named_use(const cu::Session& s, const std::string& id) {
  auto def = cu::edit_cell(s);
  if (!def.ok()) {
    return def.error();
  }
  CellUse* use = find_use(*def.value(), id);
  if (use == nullptr) {
    return diag::make(codes::kUseNotFound, "no instance \"" + id + "\" in the edit cell");
  }
  return use;
}

CommandResult instance_list(const cu::Session& s) {
  auto def = cu::edit_cell(s);
  if (!def.ok()) {
    return cu::fail(def.error());
  }
  Value::List ids;
  for (const auto& u : def.value()->uses()) {
    ids.emplace_back(u->id);
  }
  return CommandResult::success(Value{std::move(ids)});
}

CommandResult instance_on_use(const cu::Session& s, CellUse& use, const std::vector<std::string>& args) {
  const std::string& op = args[0];
  if (op == "rename" && args.size() == 3) {
    auto st = rename_use(use, args[2]);
    return st.ok() ? CommandResult::success() : cu::fail(st.error());
  }
  if ((op == "lock" || op == "unlock") && args.size() == 2) {
    set_use_locked(use, op == "lock");
    return CommandResult::success();
  }
  if (op == "delete" && args.size() == 2) {
    CellDef* parent = use.parent;
    auto st = remove_use(use);
    if (!st.ok()) {
      return cu::fail(st.error());
    }
    recompute_bbox(*parent, s.magic_compat);
    return CommandResult::success();
  }
  if (op == "cell" && args.size() == 2) {
    return CommandResult::success(Value{use.def->name()});
  }
  return cu::fail(cu::bad_argument("usage: instance list | rename id new | lock id | unlock id | delete id | cell id"));
}

CommandResult cmd_instance(CommandContext& ctx, const Invocation& inv) {
  auto s = cu::session(ctx);
  if (!s.ok()) {
    return cu::fail(s.error());
  }
  if (inv.args.empty() || inv.args[0] == "list") {
    return instance_list(s.value());
  }
  if (inv.args.size() < 2) {
    return cu::fail(cu::bad_argument("usage: instance option id ?value?"));
  }
  auto use = named_use(s.value(), inv.args[1]);
  if (!use.ok()) {
    return cu::fail(use.error());
  }
  return instance_on_use(s.value(), *use.value(), inv.args);
}

// --- array ---------------------------------------------------------------------

diag::Outcome<ArrayInfo> parse_array(const std::vector<std::string>& args, std::size_t first) {
  std::vector<std::int64_t> n;
  for (std::size_t i = first; i < args.size(); ++i) {
    auto v = cu::parse_int(args[i]);
    if (!v.ok()) {
      return v.error();
    }
    n.push_back(v.value());
  }
  ArrayInfo a;
  if (n.size() == 2) {
    if (n[0] < 1 || n[1] < 1) {
      return cu::bad_argument("array sizes must be positive");
    }
    a.xhi = static_cast<std::int32_t>(n[0] - 1);
    a.yhi = static_cast<std::int32_t>(n[1] - 1);
    return a;
  }
  if (n.size() == 4) {
    a.xlo = static_cast<std::int32_t>(n[0]);
    a.xhi = static_cast<std::int32_t>(n[1]);
    a.ylo = static_cast<std::int32_t>(n[2]);
    a.yhi = static_cast<std::int32_t>(n[3]);
    return a;
  }
  return cu::bad_argument("usage: array ?count? xsize ysize | xlo xhi ylo yhi");
}

CommandResult cmd_array(CommandContext& ctx, const Invocation& inv) {
  auto s = cu::session(ctx);
  if (!s.ok()) {
    return cu::fail(s.error());
  }
  CellUse* use = cu::current_use(s.value());
  if (use == nullptr) {
    return cu::fail(diag::make(codes::kUseNotFound, "no instance is selected (place one with getcell)"));
  }
  const std::size_t first = !inv.args.empty() && inv.args[0] == "count" ? 1 : 0;
  auto a = parse_array(inv.args, first);
  if (!a.ok()) {
    return cu::fail(a.error());
  }
  auto b = cu::box(s.value());
  if (!b.ok()) {
    return cu::fail(b.error());
  }
  ArrayInfo info = a.value();
  info.xsep = b.value().x1 - b.value().x0;
  info.ysep = b.value().y1 - b.value().y0;
  make_array(*use, invert(use->transform), info);
  recompute_bbox(*use->parent, s.value().magic_compat);
  return CommandResult::success();
}

}  // namespace

namespace cmdutil {

bool register_cell_commands(cmd::CommandRegistry& registry) {
  bool ok = add(registry, "cellname", "cellname list|rename|delete|create|exists|readonly|writeable|modified ?args?",
                "Lists, renames, creates or deletes cell definitions.", cmd_cellname);
  ok = add(registry, "instance", "instance list | rename|lock|unlock|delete|cell id ?value?",
           "Lists or changes instances of the edit cell.", cmd_instance) && ok;
  ok = add(registry, "array", "array ?count? xsize ysize | xlo xhi ylo yhi",
           "Arrays the current instance; the box size gives the spacing.", cmd_array) && ok;
  return ok;
}

}  // namespace cmdutil

}  // namespace hefesto::db
