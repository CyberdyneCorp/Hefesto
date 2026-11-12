// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// MODEL commands, part 3: cellname and instance (cmd-hierarchy "Cellname
// and instance commands"). `array` moved to the select area at the M2
// scaffold (select/commands.cpp). Owner: HIER
// (openspec/changes/m2-interactive-editing/design.md D2).
// Ported from Magic VLSI 8.3.694 (fc5daa9e) commands/CmdCD.c (CmdCellname)
// and database/DBcellname.c (DBCellPrint, DBUsePrint, DBLockUse,
// DBOrientUse, DBAbutmentUse, DBPositionUse). Magic is Copyright (C) 1985,
// 1990 Regents of the University of California (permissive license,
// THIRD_PARTY_NOTICES.md).

#include <algorithm>
#include <array>
#include <iterator>
#include <string>
#include <variant>
#include <vector>
#include <optional>
#include <utility>

#include "db/command_util.hpp"
#include "db/hier_commands.hpp"
#include "hefesto/cell/commands.hpp"
#include "hefesto/cell/hier.hpp"
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

CommandResult cellname_usage(const std::string& cmd = "cellname") {
  return cu::fail(cu::bad_argument(
      "Usage: " + cmd + " [list] children|parents|self|exists|instances|celldef|delete [name]\n"
      "or:    " + cmd + " [list] allcells|topcells|window\n"
      "or:    " + cmd + " create name\n"
      "or:    " + cmd + " rename name newname [-force]\n"
      "or:    " + cmd + " [un]lock [name]\n"
      "or:    " + cmd + " writeable [name] [true|false]"));
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
  s.editor->drc.pending.remove(*doomed);  // DBCellDeleteDef: DRCRemovePending
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

/// `exists`/`self` NAME (dbCellPrintInfo SELF): the list form returns the
/// name, or 0 when no such cell is loaded; the plain form prints "Cell NAME
/// is currently loaded." or reports "Cell NAME is not currently loaded."
/// with no result (CMDEDIT-HIER-08).
CommandResult cellname_exists(const cu::Session& s, const std::vector<std::string>& args, bool dolist) {
  if (args.size() < 2) {
    const CellDef* def = s.editor->edit_cell;
    return CommandResult::success(Value{def != nullptr ? def->name() : std::string{}});
  }
  const CellDef* def = s.editor->cells.find(args[1]);
  if (dolist) {
    return CommandResult::success(def != nullptr ? Value{def->name()} : Value{std::int64_t{0}});
  }
  if (def == nullptr) {
    cu::warn(s, "Cell " + args[1] + " is not currently loaded.");
  } else {
    cu::print(s, "Cell " + def->name() + " is currently loaded.");
  }
  return CommandResult::success();
}

CommandResult cellname_create(const cu::Session& s, const std::vector<std::string>& args) {
  if (args.size() != 2) {
    return cellname_usage();
  }
  // CmdCellname IDX_CREATE: an existing cell is left as it is, silently (gencell
  // re-creates a generated cell whose parameters hash to the same name).
  if (s.editor->cells.find(args[1]) != nullptr) {
    return CommandResult::success();
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
  bool def_names = false;  // -def
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
      c.def_names = last == "-def";
      c.args.pop_back();
    }
  }
  if (c.args.empty() || c.args.size() > 4) {
    return std::nullopt;
  }
  return c;
}

// --- cellname: the per-cell queries --------------------------------------------------
//
// DBCellPrint / dbCellPrintInfo (Magic ref: database/DBcellname.c:457-880)
// and the IDX_FILEPATH, IDX_TIMESTAMP, IDX_FLAGS, IDX_PROPERTY branches of
// CmdCellname (commands/CmdCD.c:1290-1560).

// The cell a per-cell option names: NAME, else the first selected
// instance's cell, else the edit root (EditRootDef).
CellDef* option_cell(const cu::Session& s, const std::vector<std::string>& args, bool use_selection) {
  if (args.size() >= 2) {
    return s.editor->cells.find(args[1]);
  }
  if (use_selection) {
    const auto selected = cell::selected_instances(*s.editor);
    if (!selected.empty()) {
      return selected.front().use->def;
    }
  }
  return s.editor->edit_root != nullptr ? s.editor->edit_root : s.editor->edit_cell;
}

CommandResult names_or_lines(const cu::Session& s, std::vector<std::string> names, bool dolist, std::string head) {
  if (dolist) {
    return CommandResult::success(Value{Value::List{names.begin(), names.end()}});
  }
  for (const std::string& n : names) {
    head += "\n    " + n;
  }
  cu::print(s, std::move(head));
  return CommandResult::success();
}

CommandResult unknown_cell(const std::vector<std::string>& args) {
  return cu::fail(diag::make(codes::kCellNotFound, "Unknown cell " + (args.size() >= 2 ? args[1] : std::string{})));
}

// children: every definition (cell table order) with a use in `def`;
// parents: the distinct non-internal parents of `def`'s uses (cd_parents order).
std::vector<std::string> relatives(const cu::Session& s, const CellDef& def, bool children) {
  std::vector<std::string> names;
  if (children) {
    s.editor->cells.for_each([&](CellDef& d) {
      for (const CellUse* u : d.instances()) {
        if (u->parent == &def) {
          names.push_back(d.name());
          break;
        }
      }
    });
    return names;
  }
  for (const CellUse* u : def.instances()) {
    if (u->parent != nullptr && !u->parent->flags().internal &&
        std::find(names.begin(), names.end(), u->parent->name()) == names.end()) {
      names.push_back(u->parent->name());
    }
  }
  return names;
}

// instances: the identifiers of `def`'s uses (cd_parents order, internal
// parents skipped); childinst: the identifiers of the uses inside `def`.
std::vector<std::string> use_names(const CellDef& def, bool inside) {
  std::vector<std::string> names;
  if (inside) {
    for (const auto& u : def.uses()) {
      names.push_back(u->id);
    }
    return names;
  }
  for (const CellUse* u : def.instances()) {
    if (u->parent != nullptr && !u->parent->flags().internal) {
      names.push_back(u->id);
    }
  }
  return names;
}

CommandResult cellname_relation(const cu::Session& s, int option, const CellnameArgs& c) {
  CellDef* def = option_cell(s, c.args, true);
  if (def == nullptr) {
    return c.dolist ? CommandResult::success(Value{Value::List{}}) : unknown_cell(c.args);
  }
  switch (option) {
    case kChildren:
      return names_or_lines(s, relatives(s, *def, true), c.dolist, "Cell " + def->name() + "'s children are:");
    case kParents:
      return names_or_lines(s, relatives(s, *def, false), c.dolist, "Cell " + def->name() + "'s parents are:");
    case kInstances:
      return names_or_lines(s, use_names(*def, false), c.dolist, "Names of cell instances:");
    case kChildinst:
      return names_or_lines(s, use_names(*def, true), c.dolist, "Def " + def->name() + "'s children are:");
    default:  // celldef
      return CommandResult::success(Value{def->name()});
  }
}

CommandResult cellname_window(const cu::Session& s) {
  const view::LayoutWindow* w = s.editor->windows.current();
  if (w == nullptr || w->root() == nullptr) {
    return cu::fail(cu::bad_argument("No window was selected for search."));
  }
  return CommandResult::success(Value{w->root()->name()});
}

CommandResult cellname_filepath(const cu::Session& s, const CellnameArgs& c) {
  CellDef* def = option_cell(s, c.args, false);
  if (def == nullptr) {
    return unknown_cell(c.args);
  }
  if (c.args.size() <= 2) {
    const std::string file = def->file_path().generic_string();
    if (file.empty()) {
      return CommandResult::success(Value{std::string{"default"}});
    }
    const std::size_t slash = file.rfind('/');
    return CommandResult::success(Value{slash == std::string::npos ? file : file.substr(0, slash)});
  }
  const std::string& path = c.args[2];
  if (path == "default") {
    def->set_file_path({});
  } else {
    def->set_file_path(path + "/" + def->name());
    def->flags().not_found = false;
  }
  return CommandResult::success();
}

CommandResult cellname_timestamp(const cu::Session& s, const CellnameArgs& c) {
  CellDef* def = option_cell(s, c.args, false);
  if (def == nullptr) {
    return unknown_cell(c.args);
  }
  if (c.args.size() <= 2) {
    return CommandResult::success(Value{std::int64_t{def->timestamp()}});
  }
  if (!cu::str_is_int(c.args[2])) {
    return cu::fail(cu::bad_argument("Invalid timestamp \"" + c.args[2] + "\"."));
  }
  const std::int64_t stamp = cu::c_atoi(c.args[2]);
  if (stamp != def->timestamp()) {
    def->set_timestamp(stamp);
    def->flags().needs_new_timestamp = false;
    // Q-CELL-36: Magic's `flags &= ~CDGETNEWSTAMP | CDFIXEDSTAMP` only clears
    // the pending flag; with magic-compat OFF the stamp becomes fixed.
    if (!s.magic_compat) {
      def->flags().fixed_timestamp = true;
    }
  }
  return CommandResult::success();
}

CommandResult cellname_flags(const cu::Session& s, const CellnameArgs& c) {
  CellDef* def = option_cell(s, c.args, false);
  if (def == nullptr) {
    return unknown_cell(c.args);
  }
  const CellFlags& f = def->flags();
  const std::array<std::pair<bool, std::string_view>, 12> kFlags = {{
      {f.available, "available"}, {f.modified, "modified"}, {f.no_edit, "readonly"}, {f.internal, "internal"},
      {f.fixed_timestamp, "fixed-timestamp"}, {f.needs_new_timestamp, "timestamp-pending"},
      {f.child_timestamps_changed, "child-timestamp-changed"}, {f.fixed_bbox, "fixed-bbox"},
      {f.child_bboxes_changed, "bbox-changed"}, {f.bbox_processed, "bbox-processed"}, {f.vendor_gds, "vendor-gds"},
      {f.dereference, "dereference"},
  }};
  Value::List out;
  for (const auto& [on, name] : kFlags) {
    if (on) {
      out.emplace_back(std::string{name});
    }
  }
  if (f.not_found) {
    out.emplace_back(std::string{"not-found"});
  }
  return CommandResult::success(Value{std::move(out)});
}

CommandResult cellname_property(const cu::Session& s, const std::vector<std::string>& args) {
  CellDef* def = args.size() >= 2 ? s.editor->cells.find(args[1]) : s.editor->edit_root;
  if (def == nullptr) {
    return unknown_cell(args);
  }
  const std::vector<std::string> rest(args.begin() + std::min<std::ptrdiff_t>(2, std::ssize(args)), args.end());
  return cu::property_on(s, *def, rest);
}

bool takes_name_only(int option) {
  return option == kAllcells || option == kTopcells || option == kModified || option == kWindow;
}

// CmdCellname's argument-count checks (`n` words from the option on).
bool too_many_args(int option, std::size_t n) {
  const bool takes_more = option == kRename || option == kDelete || option == kDereference || option == kWriteable ||
                          option == kProperty || option == kFilepath || option == kOrientation ||
                          option == kTimestamp;
  return (n > 2 && !takes_more) || (n > 3 && option != kProperty) || (n > 1 && takes_name_only(option));
}

// IDX_DEREFERENCE: cmdFlushCell(def, TRUE) then SelectClear.
CommandResult cellname_dereference(const cu::Session& s, const std::vector<std::string>& args) {
  if (args.size() != 2) {
    return cu::fail(cu::bad_argument("Dereference cell command missing cellname"));
  }
  CellDef* def = s.editor->cells.find(args[1]);
  if (def == nullptr) {
    return cu::fail(cu::bad_argument("No such cell \"" + args[1] + "\""));
  }
  return cell::dereference_cell(*s.ctx, *def);
}

CommandResult cellname_dispatch(const cu::Session& s, int option, const CellnameArgs& c) {
  switch (option) {
    case kAllcells:
    case kTopcells:
    case kModified:
      return cellname_list(s, option, c.dolist);
    case kExists:
    case kSelf:
      return cellname_exists(s, c.args, c.dolist);
    case kChildren:
    case kParents:
    case kInstances:
    case kChildinst:
    case kCelldef:
      return cellname_relation(s, option, c);
    case kWindow:
      return cellname_window(s);
    case kCreate:
      return cellname_create(s, c.args);
    case kDelete:
      return cellname_delete(s, c.args);
    case kRename:
      return cellname_rename(s, c.args, c.force);
    case kWriteable:
      return cellname_writeable(s, c.args, c.dolist);
    case kFilepath:
      return cellname_filepath(s, c);
    case kTimestamp:
      return cellname_timestamp(s, c);
    case kFlags:
      return cellname_flags(s, c);
    case kProperty:
      return cellname_property(s, c.args);
    case kLock:
    case kUnlock:
      return cu::fail(cu::bad_argument("Cell definitions cannot be locked.  Use \"instance\"?\n"
                                       "  or do you mean \"cellname writeable\"?"));
    case kOrientation:
      return cu::fail(cu::bad_argument("Cell definitions do not have orientations.  Use \"instance\"?"));
    case kPosition:
      return cu::fail(cu::bad_argument("Cell definitions do not have positions.  Use \"instance\"?"));
    case kAbutment:
      return cu::fail(cu::bad_argument("Use \"property get FIXED_BBOX\" to get the cell abutment box."));
    default:
      return cellname_dereference(s, c.args);
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
  if (option < 0 || too_many_args(option, c->args.size())) {
    return cellname_usage();
  }
  return cellname_dispatch(s.value(), option, *c);
}

// --- instance ------------------------------------------------------------------
//
// CmdCellname for instances (any command name containing "in"; Magic ref:
// commands/CmdCD.c:1125-1700, database/DBcellname.c DBUsePrint, DBLockUse,
// DBOrientUse, DBAbutmentUse, DBPositionUse). NAME is an instance of the
// edit cell; without it the selected instances are used. Hefesto keeps the
// M1 conveniences `instance list` (the edit cell's instance identifiers)
// and `instance delete ID`.

/// The instances an option applies to: NAME in the edit cell, else the
/// selected instances.
std::vector<CellUse*> option_uses(const cu::Session& s, const std::vector<std::string>& args) {
  std::vector<CellUse*> out;
  if (args.size() >= 2) {
    if (s.editor->edit_cell != nullptr) {
      if (CellUse* u = find_use(*s.editor->edit_cell, args[1])) {
        out.push_back(u);
      }
    }
    return out;
  }
  for (const cell::InstanceRef& ref : cell::selected_instances(*s.editor)) {
    out.push_back(ref.use);
  }
  return out;
}

std::string_view orientation_word(const Transform& t, bool def_names) {
  constexpr std::array<std::string_view, 8> kMagic = {"0", "180", "90", "270", "0h", "180h", "90h", "270h"};
  constexpr std::array<std::string_view, 8> kDef = {"N", "S", "E", "W", "FN", "FS", "FE", "FW"};
  const auto o = static_cast<std::size_t>(orientation(t));
  return o < kMagic.size() ? (def_names ? kDef[o] : kMagic[o]) : std::string_view{};
}

Rect abutment_box(const CellUse& use) {
  Rect box = use.def->bbox();
  if (const auto* v = use.def->properties().find("FIXED_BBOX")) {
    if (const auto* dims = std::get_if<DimensionList>(v); dims != nullptr && dims->values.size() == 4) {
      box = Rect{dims->values[0], dims->values[1], dims->values[2], dims->values[3]};
    }
  }
  return apply(use.transform, box);
}

// One instance's answer to a query option, appended to `out`.
void instance_query(const cu::Session& s, int option, const CellUse& use, bool def_names, Value::List& out) {
  switch (option) {
    case kChildren:
    case kChildinst:
      for (const auto& child : use.def->uses()) {
        out.emplace_back(child->id);
      }
      break;
    case kParents:
      out.emplace_back(use.parent != nullptr ? use.parent->name() : std::string{});
      break;
    case kCelldef:
      out.emplace_back(use.def->name());
      break;
    case kOrientation:
      out.emplace_back(std::string{orientation_word(use.transform, def_names)});
      break;
    case kAbutment:
      for (const Coord v : {abutment_box(use).x0, abutment_box(use).y0, abutment_box(use).x1, abutment_box(use).y1}) {
        out.push_back(cu::coord_value(s, v));
      }
      break;
    case kPosition: {
      const Transform t = element_transform(use, use.array.xlo, use.array.ylo);
      out.push_back(Value{Value::List{cu::coord_value(s, t.c), cu::coord_value(s, t.f)}});
      break;
    }
    default:  // exists, self, instances
      out.emplace_back(use.id);
      break;
  }
}

CommandResult instance_unavailable(int option) {
  switch (option) {
    case kTopcells:
      return cu::fail(cu::bad_argument("Instances do not have a top level.  Use \"cellname\"?"));
    case kDelete:
      return cu::fail(cu::bad_argument("Function unimplemented for instances.  Use \"delete\""));
    case kCreate:
      return cu::fail(cu::bad_argument("Function unimplemented for instances.  Use \"getcell\""));
    case kRename:
      return cu::fail(cu::bad_argument("Function unimplemented for instances.  Use \"identify\""));
    default:
      return cu::fail(cu::bad_argument("Function unimplemented for instances."));
  }
}

bool instance_supported(int option) {
  switch (option) {
    case kTopcells: case kWindow: case kWriteable: case kFlags: case kProperty: case kFilepath:
    case kModified: case kDereference: case kTimestamp: case kDelete: case kCreate: case kRename:
      return false;
    default:
      return true;
  }
}

CommandResult instance_lock(const cu::Session& s, const std::vector<CellUse*>& uses, bool lock) {
  for (CellUse* use : uses) {
    if (s.editor->edit_cell != nullptr && use->parent != s.editor->edit_cell) {
      cu::warn(s, "Cell " + use->id + " (" + use->def->name() + ") isn't a child of the edit cell.");
      continue;
    }
    if (use->locked != lock) {
      set_use_locked(*use, lock);
    }
  }
  return CommandResult::success();
}

// M1 conveniences: `instance list` and `instance delete ID`.
std::optional<CommandResult> instance_extension(const cu::Session& s, const std::vector<std::string>& args) {
  if (args.size() == 1 && args[0] == "list") {
    auto def = cu::edit_cell(s);
    if (!def.ok()) {
      return cu::fail(def.error());
    }
    const std::vector<std::string> ids = use_names(*def.value(), true);
    return CommandResult::success(Value{Value::List{ids.begin(), ids.end()}});
  }
  if (args.size() == 2 && args[0] == "delete") {
    const auto uses = option_uses(s, args);
    if (uses.empty()) {
      return cu::fail(diag::make(codes::kUseNotFound, "no instance \"" + args[1] + "\" in the edit cell"));
    }
    CellDef* parent = uses.front()->parent;
    if (auto st = remove_use(*uses.front()); !st.ok()) {
      return cu::fail(st.error());
    }
    recompute_bbox(*parent, s.magic_compat);
    return CommandResult::success();
  }
  return std::nullopt;
}

CommandResult instance_command(const cu::Session& s, int option, const CellnameArgs& c, bool def_names) {
  if (!instance_supported(option)) {
    return instance_unavailable(option);
  }
  if (option == kAllcells) {
    return CommandResult::success();  // DBUsePrint ignores ALLCELLS
  }
  if (option == kOrientation && c.args.size() == 3) {
    return cu::fail(cu::bad_argument("Cannot set orientation by name.  Use selection."));
  }
  const std::vector<CellUse*> uses = option_uses(s, c.args);
  if (option == kLock || option == kUnlock) {
    return instance_lock(s, uses, option == kLock);
  }
  if (uses.empty()) {
    if (!c.dolist) {
      cu::print(s, c.args.size() >= 2 ? "Cell " + c.args[1] + " is not currently loaded." : "No cells selected.");
    }
    return CommandResult::success();
  }
  Value::List out;
  for (const CellUse* use : uses) {
    instance_query(s, option, *use, def_names, out);
  }
  if (out.size() == 1) {
    return CommandResult::success(std::move(out.front()));
  }
  return CommandResult::success(Value{std::move(out)});
}

CommandResult cmd_instance(CommandContext& ctx, const Invocation& inv) {
  auto s = cu::session(ctx);
  if (!s.ok()) {
    return cu::fail(s.error());
  }
  if (auto extension = instance_extension(s.value(), inv.args)) {
    return std::move(*extension);
  }
  const auto c = split_cellname(inv.args);
  if (!c) {
    return cellname_usage("instance");
  }
  const int option = cellname_option(c->args[0]);
  if (option < 0 || too_many_args(option, c->args.size())) {
    return cellname_usage("instance");
  }
  return instance_command(s.value(), option, *c, c->def_names);
}

}  // namespace

namespace cmdutil {

bool register_cell_commands(cmd::CommandRegistry& registry) {
  bool ok = add(registry, "cellname",
                "cellname [list] children|parents|self|exists|instances|childinst|celldef|allcells|topcells|window|"
                "create|delete|filepath|flags|timestamp|property|rename|writeable|modified [name] [args] "
                "[-list|-force]",
                "Queries, creates, renames or deletes cell definitions.", cmd_cellname);
  ok = add(registry, "instance",
           "instance [list] children|parents|self|exists|instances|childinst|celldef|lock|unlock|orientation|"
           "abutment|position [name] [-list|-def]",
           "Queries the named (or selected) instances of the edit cell, or locks and unlocks them.", cmd_instance) &&
       ok;
  return ok;
}

}  // namespace cmdutil

}  // namespace hefesto::db
