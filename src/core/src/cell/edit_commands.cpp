// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `xload`, `edit` and `down` (cmd-hierarchy "Xload command", "Edit
// command", "Down command"), with the edit frame of design.md D4.
// Ported from Magic VLSI 8.3.694 (fc5daa9e) commands/CmdTZ.c (CmdXload),
// commands/CmdE.c (CmdEdit, cmdEditEnumFunc) and commands/CmdCD.c
// (CmdDown, cmdDownEnumFunc). Magic is Copyright (C) 1985, 1990 Regents of
// the University of California (permissive license, THIRD_PARTY_NOTICES.md).

#include <optional>
#include <string>

#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/cell/hier.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "io_session.hpp"

namespace hefesto::cell::detail {

namespace {

using cmd::CommandResult;

CellReader reader_for(IoSession& s) {
  return [&s](db::CellDef& def) {
    ReadOptions opts;
    opts.ignore_tech = true;
    (void)read_cell(s.io, def, opts);
  };
}

/// The instance `edit`/`down` choose: by path from the window's root, else
/// among the selected instances (the one under the pointer, else the last).
struct Chosen {
  std::optional<InstanceRef> ref;
  bool by_path = false;
};

std::optional<db::Rect> pointer_area(const editor::EditorState& ed) {
  if (!ed.command_point) {
    return std::nullopt;
  }
  return db::Rect{ed.command_point->point.x, ed.command_point->point.y, ed.command_point->point.x,
                  ed.command_point->point.y};
}

Chosen choose(IoSession& s, const cmd::Invocation& inv) {
  Chosen c;
  if (inv.args.size() == 1) {
    c.by_path = true;
    const view::LayoutWindow* w = s.ed.windows.current();
    if (w != nullptr && w->root() != nullptr) {
      c.ref = find_instance(*w->root(), inv.args[0], reader_for(s));
    }
    return c;
  }
  c.ref = choose_instance(selected_instances(s.ed), pointer_area(s.ed));
  return c;
}

CommandResult edit_usage() {
  return fail(db::codes::kUseNotFound, "Usage: edit\nMaybe you want the \"load\" command");
}

// --- xload ------------------------------------------------------------------------

CommandResult xload_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  if (inv.args.size() > 1) {
    return fail(codes::kBadUsage, "Usage: xload [name]");
  }
  if (!inv.args.empty()) {
    if (auto bad = illegal_cell_name(inv.args[0])) {
      return fail(codes::kInvalidName, *bad);
    }
  }
  return with_session(ctx, [&](IoSession& s) {
    view::LayoutWindow* w = s.ed.windows.current();
    if (w == nullptr) {
      return fail(codes::kBadUsage, "Point to a window first.");
    }
    auto loaded = load_into_window(s, inv.args.empty() ? std::nullopt : std::optional{inv.args[0]}, ReadOptions{});
    if (!loaded.ok()) {
      return loaded;
    }
    w->clear_expansion();  // DBExpandAll(DB_UNEXPAND) over the whole hierarchy
    return loaded;
  });
}

// --- edit -------------------------------------------------------------------------

CommandResult edit_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  if (inv.args.size() > 1) {
    return edit_usage();
  }
  return with_session(ctx, [&](IoSession& s) {
    const Chosen c = choose(s, inv);
    if (!c.ref && c.by_path) {
      return edit_usage();
    }
    if (!c.ref) {
      return fail(codes::kNoCell, "No cell selected to edit.");
    }
    db::CellDef& def = *c.ref->use->def;
    if (!def.flags().available) {
      reader_for(s)(def);
    }
    if (def.flags().no_edit) {
      return fail(codes::kReadOnly, "File " + def.file_path().generic_string() + " is not writeable.  Edit not set.");
    }
    EditFrame frame{&def, nullptr, c.ref->to_root};
    if (c.by_path && s.io.magic_compat) {
      // Q-CMDEDIT-30: only the edit cell changes; the root and transform stay.
      frame.root = s.ed.edit_root;
      frame.to_root = s.ed.edit_transform;
    } else if (c.by_path) {
      frame.root = s.ed.windows.current()->root();
    } else {
      frame.root = s.ed.selection.root();
    }
    set_edit_frame(s.ed, frame);
    return CommandResult::success();
  });
}

// --- down -------------------------------------------------------------------------

/// CmdDown recasts the selection form with the instance for the command
/// log. Magic writes the new root use's id ("Topmost cell in the window"),
/// which does not replay; the chosen instance path does.
void log_down_target(CommandResult& result, const Chosen& c, const cmd::Invocation& inv) {
  if (result.ok() && c.ref && inv.args.empty()) {
    const std::string path = c.ref->path.empty() ? c.ref->use->id : c.ref->path;
    result.logged_as = cmd::Invocation{"down", {path}, inv.origin, std::nullopt};
  }
}

CommandResult down_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  if (inv.args.size() > 1) {
    return edit_usage();
  }
  return with_session(ctx, [&](IoSession& s) {
    const Chosen c = choose(s, inv);
    if (!c.ref && c.by_path) {
      return edit_usage();
    }
    db::CellDef* target = c.ref ? c.ref->use->def : s.ed.edit_cell;
    if (!c.ref) {
      s.emit(codes::kNoCell, "You haven't selected a new cell to edit.");
    }
    if (target == nullptr) {
      return CommandResult::success();
    }
    ReadOptions read;
    read.ignore_tech = true;  // DBW_LOAD_IGNORE_TECH
    CommandResult result = load_into_window(s, target->name(), read);
    log_down_target(result, c, inv);
    return result;
  });
}

}  // namespace

bool register_edit_commands(cmd::CommandRegistry& registry) {
  bool ok = !registry
                 .add(cmd::CommandSpec{"xload", "xload [name]",
                                       "Loads a cell into the window like load, with every instance unexpanded.",
                                       xload_cmd})
                 .has_value();
  ok = !registry
            .add(cmd::CommandSpec{"edit", "edit [instance-path]",
                                  "Makes the selected (or named) instance's cell the edit cell, keeping the "
                                  "window's root cell.",
                                  edit_cmd})
            .has_value() &&
       ok;
  ok = !registry
            .add(cmd::CommandSpec{"down", "down [instance-path]",
                                  "Loads the selected (or named) instance's cell as the window's root and edit cell.",
                                  down_cmd})
            .has_value() &&
       ok;
  return ok;
}

}  // namespace hefesto::cell::detail
