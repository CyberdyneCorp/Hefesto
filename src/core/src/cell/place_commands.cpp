// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `getcell` and `flatten`: the CELL side of the two MODEL commands that may
// need cells from files. `load` reads only the named cell (Magic does not
// expand on load), so getcell reads the cell it places and flatten reads
// the edit cell's unavailable descendants before the MODEL handlers
// (db::getcell_command, db::flatten_command) run on memory only.
// Ported from Magic 8.3.694 commands/CmdCD.c (cmdDumpParseArgs) and
// commands/CmdFI.c (CmdGetcell, CmdFlatten); Magic is (c) the Regents of
// the University of California (THIRD_PARTY_NOTICES.md).

#include <string>

#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/db/commands.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "io_session.hpp"

namespace hefesto::cell::detail {

namespace {

using cmd::CommandResult;

struct GetcellName {
  std::string cell;  // the definition name (last path component, no .mag)
  std::string path;  // the name as given when it has a directory part
};

std::string strip_mag(std::string name) {
  if (name.size() > 4 && name.ends_with(".mag")) {
    name.resize(name.size() - 4);
  }
  return name;
}

GetcellName split_getcell_name(const std::string& arg) {
  const std::size_t slash = arg.rfind('/');
  if (slash == std::string::npos) {
    return {strip_mag(arg), {}};
  }
  return {strip_mag(arg.substr(slash + 1)), strip_mag(arg)};
}

// The path-mismatch rule of cmdDumpParseArgs: a definition that already
// points elsewhere is not reused; a new one named <cell>_a, <cell>_b, ... is.
db::CellDef* definition_for(IoSession& s, const GetcellName& n) {
  db::CellDef* def = s.ed.cells.find(n.cell);
  if (def == nullptr) {
    auto made = s.ed.cells.create(n.cell, s.ed.technology->plane_count());
    def = made ? made.value() : nullptr;
  }
  if (def == nullptr || n.path.empty()) {
    return def;
  }
  const std::string existing = def->file_path().generic_string();
  if (existing.empty()) {
    def->set_file_path(n.path);
    return def;
  }
  const auto same = s.io.host.same_file(existing + ".mag", n.path + ".mag");
  if (existing == n.path || (same && *same)) {
    return def;
  }
  s.emit(codes::kConflictingLocation, "Warning:  Cell file path mismatch.  Existing cell has path \"" + existing +
                                          "\", while getcell path is \"" + n.path + "\".");
  std::string renamed;
  for (char c = 'a'; c <= 'z'; ++c) {
    renamed = n.cell + "_" + c;
    if (s.ed.cells.find(renamed) == nullptr) {
      break;
    }
  }
  s.emit(codes::kConflictingLocation, "Renaming cell to \"" + renamed + "\" to avoid conflict.");
  auto made = s.ed.cells.create(renamed, s.ed.technology->plane_count());
  if (!made) {
    return nullptr;
  }
  made.value()->set_file_path(n.path);
  return made.value();
}

CommandResult getcell_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  if (inv.args.empty()) {
    return fail(codes::kBadUsage, "Missing cell name in \"getcell\" command.");
  }
  return with_session(ctx, [&](IoSession& s) {
    if (s.ed.edit_cell == nullptr) {
      return fail(codes::kNoCell, "No cell def being edited; cannot place cell use!");
    }
    db::CellDef* def = definition_for(s, split_getcell_name(inv.args[0]));
    if (def == nullptr) {
      return fail(codes::kNoCell, "Could not create cell \"" + inv.args[0] + "\"");
    }
    def->flags().not_found = false;
    ReadOptions read;
    read.ignore_tech = true;
    if (!read_cell(s.io, *def, read).ok() && !def->flags().available) {
      return CommandResult::success();  // dbReadOpen printed why; Magic places nothing
    }
    db::recompute_bbox(*def, s.io.magic_compat);
    if (!s.ed.box) {
      return fail(codes::kBadUsage, "The box's lower-left corner must point to the place\n"
                                    "    in the edit cell where you'd like to put \"" + inv.args[0] + "\".");
    }
    cmd::Invocation placed = inv;
    placed.args[0] = def->name();
    return db::getcell_command(s.command, placed);
  });
}

CommandResult flatten_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return with_session(ctx, [&](IoSession& s) {
    if (s.ed.edit_cell != nullptr) {
      expand_all(s.io, *s.ed.edit_cell);  // DBTreeSrLabels/DBCellCopyAllPaint read as they descend
    }
    return db::flatten_command(s.command, inv);
  });
}

}  // namespace

bool register_place_commands(cmd::CommandRegistry& registry) {
  bool ok = !registry
                 .add(cmd::CommandSpec{"getcell", "getcell name [orientation]",
                                       "Places an instance of a cell (read from its file when needed) with its "
                                       "lower-left corner at the box; the box then covers the instance.",
                                       getcell_cmd})
                 .has_value();
  ok = !registry
            .add(cmd::CommandSpec{"flatten", "flatten [options] name",
                                  "Flattens the edit cell into a new cell (-doinplace: flattens an instance).",
                                  flatten_cmd})
            .has_value() &&
       ok;
  return ok;
}

}  // namespace hefesto::cell::detail
