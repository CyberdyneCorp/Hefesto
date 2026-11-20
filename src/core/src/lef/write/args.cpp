// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `lef write`, `lef writeall` and `lef nocheck` options (capability
// lef-write "Command dispatch and target cell", "lef write argument
// grammar", "Distance option values", "lef writeall arguments and scope",
// "lef nocheck net list", "Help text"; see args.hpp).
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/lefCmd.c CmdLef (the target
// cell, the LEF_WRITE, LEF_WRITEALL and LEF_NOCHECK cases and the help),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: LEFW (design D2, D14).

#include "lef/write/args.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

#include "db/command_util.hpp"
#include "hefesto/db/use_index.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/lef/command/command.hpp"
#include "hefesto/lef/write/write.hpp"
#include "hefesto/view/window.hpp"

namespace hefesto::lef {

namespace {

namespace cu = db::cmdutil;
using cmd::CommandResult;

/// What an argument scan decided.
enum class Scan : std::uint8_t { ok, usage, failed };

/// strncmp(arg, option, strlen(option)) == 0.
bool matches(std::string_view arg, std::string_view option) {
  return arg.starts_with(option);
}

// LEF_HELP / wrongNumArgs: the `lef` table.
CommandResult help(const cu::Session& s) {
  std::string text = "The \"lef\" options are:\n";
  for (const std::string_view entry : lef_options()) {
    text += "    lef " + std::string{entry} + "\n";
  }
  cu::print_block(s, std::move(text));
  return CommandResult::success();
}

// CmdGetSelectedCell: the definition of the first selected instance.
db::CellDef* selected_cell(const cu::Session& s) {
  const db::CellDef* sel = s.editor->selection.def();
  if (sel == nullptr) {
    return nullptr;
  }
  for (db::CellUse* use : db::uses_in_search_order(*sel, compat::CompatSettings{s.magic_compat, true})) {
    if (use->def != nullptr) {
      return use->def;
    }
  }
  return nullptr;
}

// CmdLef's target: the selected cell, else the current window's root,
// else the box's cell. A command point without a window (a script) stands
// for the edit root, as the dispatcher's window check does.
std::optional<db::CellDef*> target_cell(const cu::Session& s) {
  if (db::CellDef* selected = selected_cell(s)) {
    return selected;
  }
  if (const view::LayoutWindow* w = s.editor->windows.current(); w != nullptr) {
    return w->root();
  }
  if (s.editor->box && s.editor->box_root != nullptr) {
    return s.editor->box_root;
  }
  if (s.editor->command_point) {
    return s.editor->edit_root != nullptr ? s.editor->edit_root : s.editor->edit_cell;
  }
  return std::nullopt;
}

/// The parsed arguments.
struct Parsed {
  LefWriteOptions options;
  std::optional<std::string> name;
  std::optional<diag::Diagnostic> error;
};

// `-hide` / `-pinonly` take the next argument as their distance when it
// starts with a number (StrIsNumeric), parsed by cmdParseCoord; else 0.
Scan distance(const cu::Session& s, const cmd::Invocation& inv, std::size_t& i, std::optional<db::Coord>& value,
              Parsed& p) {
  value = 0;
  if (i + 1 >= inv.args.size() || !cu::str_is_numeric(inv.args[i + 1])) {
    return Scan::ok;
  }
  ++i;
  auto parsed = cu::parse_distance(s, inv.args[i], cmd::Axis::x, false);
  if (!parsed.ok()) {
    p.error = parsed.error();
    return Scan::failed;
  }
  value = parsed.value();
  return Scan::ok;
}

// The options both `write` and `writeall` take.
std::optional<Scan> shared_option(const cu::Session& s, const cmd::Invocation& inv, std::size_t& i, Parsed& p) {
  const std::string& arg = inv.args[i];
  if (matches(arg, "-tech")) {
    p.options.tech = true;
  } else if (matches(arg, "-hide")) {
    return distance(s, inv, i, p.options.hide, p);
  } else if (matches(arg, "-pinonly")) {
    return distance(s, inv, i, p.options.pin_only, p);
  } else if (matches(arg, "-toplayer")) {
    p.options.top_layer = true;
  } else if (matches(arg, "-nomaster")) {
    p.options.do_master = false;
  } else {
    return std::nullopt;
  }
  return Scan::ok;
}

// The `def write` options, which `lef write` ignores with a note.
bool def_only_option(const cu::Session& s, std::string_view arg) {
  for (const std::string_view option : {"-allspec", "-units", "-anal"}) {
    if (matches(arg, option)) {
      s.ctx->emit(diag::make("LEF-WRITE-010", "The \"" + std::string{option} + "\" option is only for def write"));
      return true;
    }
  }
  return false;
}

// `-allspec` is tested before `-tech` as in CmdLef; the order only
// matters for the note.
Scan write_option(const cu::Session& s, const cmd::Invocation& inv, std::size_t& i, Parsed& p) {
  if (matches(inv.args[i], "-allspec")) {
    (void)def_only_option(s, inv.args[i]);
    return Scan::ok;
  }
  if (const std::optional<Scan> shared = shared_option(s, inv, i, p)) {
    return *shared;
  }
  return def_only_option(s, inv.args[i]) ? Scan::ok : Scan::usage;
}

// LEF_WRITE: [filename] then options; any other word is a usage error.
Scan parse_write(const cu::Session& s, const cmd::Invocation& inv, Parsed& p) {
  for (std::size_t i = 1; i < inv.args.size(); ++i) {
    const std::string& arg = inv.args[i];
    Scan scan = Scan::ok;
    if (arg.starts_with('-')) {
      scan = write_option(s, inv, i, p);
    } else if (i == 1) {
      p.name = arg;
    } else {
      scan = Scan::usage;
    }
    if (scan != Scan::ok) {
      return scan;
    }
  }
  return Scan::ok;
}

// LEF_WRITEALL: options only.
Scan parse_write_all(const cu::Session& s, const cmd::Invocation& inv, Parsed& p) {
  for (std::size_t i = 1; i < inv.args.size(); ++i) {
    const std::string& arg = inv.args[i];
    if (!arg.starts_with('-')) {
      return Scan::usage;
    }
    if (matches(arg, "-notop")) {
      p.options.top_cell = false;
      continue;
    }
    if (const std::optional<Scan> shared = shared_option(s, inv, i, p)) {
      if (*shared != Scan::ok) {
        return *shared;
      }
      continue;
    }
    if (!matches(arg, "-all")) {
      return Scan::usage;
    }
    p.options.recurse = true;
  }
  return Scan::ok;
}

CommandResult scan_result(const cu::Session& s, Scan scan, const Parsed& p) {
  if (scan == Scan::failed && p.error) {
    return cu::fail(*p.error);
  }
  return help(s);
}

CommandResult no_cell() {
  return CommandResult::failure(diag::make("LEF-WRITE-009", "No cell selected"));
}

CommandResult lef_write(cmd::CommandContext& ctx, const cu::Session& s, const cmd::Invocation& inv,
                        db::CellDef* target) {
  Parsed p;
  if (const Scan scan = parse_write(s, inv, p); scan != Scan::ok) {
    return scan_result(s, scan, p);
  }
  if (target == nullptr) {
    return no_cell();
  }
  p.options.files = &ctx.services().files;
  p.options.loader = gds::command_subtree_loader(ctx, *s.editor);
  p.options.typed_name = p.name.value_or(std::string{});
  const std::string name = p.name.value_or(target->name());
  return write(*s.editor, *target, name, p.options);
}

CommandResult lef_write_all(cmd::CommandContext& ctx, const cu::Session& s, const cmd::Invocation& inv,
                            db::CellDef* target) {
  Parsed p;
  if (const Scan scan = parse_write_all(s, inv, p); scan != Scan::ok) {
    return scan_result(s, scan, p);
  }
  if (target == nullptr) {
    return no_cell();
  }
  p.options.files = &ctx.services().files;
  p.options.loader = gds::command_subtree_loader(ctx, *s.editor);
  return write_all(*s.editor, *target, platform::Path{}, p.options);
}

// LEF_NOCHECK: the list (Tcl result `none` when empty), or the names
// added at its front, `none` (any case) clearing it.
CommandResult lef_nocheck(const cu::Session& s, const cmd::Invocation& inv) {
  std::vector<std::string>& names = s.editor->lefdef.nocheck;
  if (inv.args.size() == 1) {
    if (names.empty()) {
      return CommandResult::success(cmd::Value{"none"});
    }
    cmd::Value::List list;
    for (const std::string& n : names) {
      list.emplace_back(n);
    }
    return CommandResult::success(cmd::Value{std::move(list)});
  }
  for (std::size_t i = 1; i < inv.args.size(); ++i) {
    std::string lower = inv.args[i];
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
    if (lower == "none") {
      names.clear();
    } else {
      names.insert(names.begin(), inv.args[i]);
    }
  }
  return CommandResult::success();
}

}  // namespace

CommandResult run_write_command(cmd::CommandContext& ctx, const cmd::Invocation& invocation, WriteCommand which) {
  auto session = cu::session(ctx);
  if (!session) {
    return cu::fail(session.error());
  }
  const cu::Session& s = session.value();
  const std::optional<db::CellDef*> target = target_cell(s);
  if (!target) {
    return CommandResult::failure(diag::make("VIEW-WINDOW-003", "Point to a window first"));
  }
  switch (which) {
    case WriteCommand::write:
      return lef_write(ctx, s, invocation, *target);
    case WriteCommand::write_all:
      return lef_write_all(ctx, s, invocation, *target);
    case WriteCommand::nocheck:
      break;
  }
  return lef_nocheck(s, invocation);
}

}  // namespace hefesto::lef
