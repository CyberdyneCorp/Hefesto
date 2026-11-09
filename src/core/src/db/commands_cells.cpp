// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// MODEL commands, part 3: cellname, instance, array (minimal Magic forms;
// `array` acts on the instance the last `getcell` placed, standing in for
// the selection until cmd-selection lands in M2).
// (Magic ref: commands/CmdCD.c CmdCellname, commands/CmdAB.c CmdArray,
// select/selOps.c SelectArray)

#include <utility>

#include "db/command_util.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"

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

CommandResult cellname_list(const cu::Session& s, std::string_view which) {
  Value::List names;
  s.editor->cells.for_each([&](CellDef& def) {
    if (def.flags().internal) {
      return;
    }
    const bool take = which == "allcells" || (which == "modified" && def.flags().modified) ||
                      (which == "topcells" && is_top_cell(def));
    if (take) {
      names.emplace_back(def.name());
    }
  });
  return CommandResult::success(Value{std::move(names)});
}

diag::Outcome<CellDef*> named_cell(const cu::Session& s, const std::string& name) {
  CellDef* def = s.editor->cells.find(name);
  if (def == nullptr) {
    return diag::make(codes::kCellNotFound, "no cell named \"" + name + "\"");
  }
  return def;
}

CommandResult cellname_rename(const cu::Session& s, const std::vector<std::string>& args) {
  if (args.size() < 3 || args.size() > 4) {
    return cu::fail(cu::bad_argument("usage: cellname rename old new ?-force?"));
  }
  auto def = named_cell(s, args[1]);
  if (!def.ok()) {
    return cu::fail(def.error());
  }
  const bool force = args.size() == 4 && args[3] == "-force";
  auto st = s.editor->cells.rename(*def.value(), args[2], force);
  return st.ok() ? CommandResult::success() : cu::fail(st.error());
}

CommandResult cellname_delete(const cu::Session& s, const std::vector<std::string>& args) {
  if (args.size() != 2) {
    return cu::fail(cu::bad_argument("usage: cellname delete name"));
  }
  auto def = named_cell(s, args[1]);
  if (!def.ok()) {
    return cu::fail(def.error());
  }
  CellDef* doomed = def.value();
  auto st = s.editor->cells.remove(*doomed);
  if (!st.ok()) {
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

CommandResult cellname_flag(const cu::Session& s, const std::vector<std::string>& args) {
  if (args.size() != 2) {
    return cu::fail(cu::bad_argument("usage: cellname " + args[0] + " name"));
  }
  auto def = named_cell(s, args[1]);
  if (!def.ok()) {
    return cu::fail(def.error());
  }
  const CellFlags& f = def.value()->flags();
  const bool readonly = f.no_edit || f.vendor_gds;
  const bool value = args[0] == "readonly" ? readonly : (args[0] == "writeable" ? !readonly : f.modified);
  return CommandResult::success(Value{std::int64_t{value ? 1 : 0}});
}

// `cellname exists name` and `cellname create name`.
CommandResult cellname_exists_or_create(const cu::Session& s, const std::string& op, const std::vector<std::string>& args) {
  if (op == "exists") {
    return CommandResult::success(Value{std::int64_t{args.size() == 2 && s.editor->cells.find(args[1]) != nullptr ? 1 : 0}});
  }
  auto made = s.editor->cells.create(args[1], s.tech->plane_count());
  return made.ok() ? CommandResult::success(Value{made.value()->name()}) : cu::fail(made.error());
}

CommandResult cmd_cellname(CommandContext& ctx, const Invocation& inv) {
  auto s = cu::session(ctx);
  if (!s.ok()) {
    return cu::fail(s.error());
  }
  const auto& args = inv.args;
  const std::string op = args.empty() ? std::string{"list"} : args[0];
  if (op == "list") {
    return cellname_list(s.value(), args.size() > 1 ? args[1] : "allcells");
  }
  if (op == "rename") {
    return cellname_rename(s.value(), args);
  }
  if (op == "delete") {
    return cellname_delete(s.value(), args);
  }
  if (op == "exists" || (op == "create" && args.size() == 2)) {
    return cellname_exists_or_create(s.value(), op, args);
  }
  if (op == "readonly" || op == "writeable" || op == "modified") {
    return cellname_flag(s.value(), args);
  }
  return cu::fail(cu::bad_argument("unknown cellname option \"" + op + "\""));
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
