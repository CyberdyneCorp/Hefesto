// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `def write` arguments (capability def-write "def write command", "def
// command argument validation"; Q-LEFDEF-104 fixed, Q-LEFDEF-109).
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/lefCmd.c CmdLef (the target
// cell, the LEF_WRITE case for `def` and wrongNumArgs), Copyright (C) 1985,
// 1990 Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: DEFW (openspec/changes/m6-lef-def-oasis/design.md D2, D14).

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "db/command_util.hpp"
#include "def/write/internal.hpp"
#include "hefesto/db/use_index.hpp"
#include "hefesto/def/write/command.hpp"
#include "hefesto/def/write/write.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/write/subtree.hpp"
#include "hefesto/lef/command/command.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/view/window.hpp"

namespace hefesto::def {

namespace {

namespace cu = db::cmdutil;
using cmd::CommandResult;

/// The parsed arguments; `usage` sends the command to the help.
struct Parsed {
  DefWriteOptions options;
  std::optional<std::string> name;
  bool usage = false;
};

// wrongNumArgs: the `def` table, one TxPrintf per entry.
CommandResult help(cmd::CommandContext& ctx, const std::string& command) {
  std::string text = "The \"" + command + "\" options are:\n";
  for (const std::string_view entry : lef::def_options()) {
    text += "    " + command + " " + std::string{entry} + "\n";
  }
  text.pop_back();
  ctx.emit(diag::make(write_detail::kArgument, std::move(text)));
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
// else the box's cell; a command point without a window (a script) stands
// for the edit root.
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

/// StrIsInt, and a value that fits an int (atoi's range).
std::optional<std::int32_t> integer(std::string_view text) {
  std::string_view digits = text;
  if (!digits.empty() && (digits.front() == '-' || digits.front() == '+')) {
    digits.remove_prefix(1);
  }
  if (digits.find_first_not_of("0123456789") != std::string_view::npos) {
    return std::nullopt;
  }
  if (digits.empty()) {
    return 0;  // StrIsInt("") and StrIsInt("-") are true; atoi gives 0
  }
  std::int32_t value = 0;
  const bool negative = text.front() == '-';
  std::string with_sign = negative ? "-" + std::string{digits} : std::string{digits};
  const auto [end, ec] = std::from_chars(with_sign.data(), with_sign.data() + with_sign.size(), value);
  if (ec != std::errc{} || end != with_sign.data() + with_sign.size()) {
    return std::nullopt;
  }
  return value;
}

void note(cmd::CommandContext& ctx, std::string text) {
  ctx.emit(diag::make(write_detail::kArgument, std::move(text)));
}

// `-units <n>`: takes the next word (Q-LEFDEF-104: a missing value is
// reported instead of read past the arguments).
void units_option(cmd::CommandContext& ctx, const cmd::Invocation& inv, std::size_t& i, std::size_t& cargs,
                  Parsed& p) {
  ++i;
  --cargs;
  const std::optional<std::int32_t> value = i < inv.args.size() ? integer(inv.args[i]) : std::nullopt;
  if (!value) {
    note(ctx, "The \"-units\" option requires an argument.");
    return;
  }
  p.options.units = *value;
}

// One `-` word; false sends the command to the help.
bool option(cmd::CommandContext& ctx, const cmd::Invocation& inv, std::size_t& i, std::size_t& cargs, Parsed& p) {
  const std::string& arg = inv.args[i];
  if (arg.starts_with("-allspec")) {
    p.options.all_special = true;
    return true;
  }
  for (const std::string_view lef_only : {"-tech", "-hide", "-pinonly", "-toplayer", "-nomaster"}) {
    if (arg.starts_with(lef_only)) {
      note(ctx, "The \"" + std::string{lef_only} + "\" option is only for lef write");
      return true;
    }
  }
  if (arg.starts_with("-units")) {
    units_option(ctx, inv, i, cargs, p);
    return true;
  }
  if (arg.starts_with("-anal")) {
    p.options.anal = true;
    return true;
  }
  return false;
}

// LEF_WRITE for `def`: `cargs` counts the words that are not options or
// option values, `def` and `write` included.
void parse(cmd::CommandContext& ctx, const cmd::Invocation& inv, Parsed& p) {
  std::size_t cargs = inv.args.size() + 1;
  for (std::size_t i = 1; i < inv.args.size(); ++i) {
    if (inv.args[i].starts_with('-')) {
      if (!option(ctx, inv, i, cargs, p)) {
        p.usage = true;
        return;
      }
      --cargs;
    } else if (i != 1) {
      p.usage = true;  // a file name only right after `write`
      return;
    }
  }
  p.usage = cargs != 2 && cargs != 3;
  if (!p.usage && cargs == 3) {
    p.name = inv.args[1];
  }
}

}  // namespace

CommandResult write_command(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  auto session = cu::session(ctx);
  if (!session) {
    return cu::fail(session.error());
  }
  const cu::Session& s = session.value();
  const std::optional<db::CellDef*> target = target_cell(s);
  if (!target) {
    return CommandResult::failure(diag::make("VIEW-WINDOW-003", "Point to a window first"));
  }
  Parsed p;
  parse(ctx, invocation, p);
  if (p.usage) {
    return help(ctx, invocation.name);
  }
  if (*target == nullptr) {
    return CommandResult::failure(diag::make(write_detail::kNoCell, "No cell selected"));
  }
  p.options.files = &ctx.services().files;
  p.options.typed_name = p.name.value_or(std::string{});
  // The net search reads the subcells it reaches (LEFDEF-328); the reads
  // print into the write's messages, through a context on its sink.
  p.options.read_area = [&ctx, editor = s.editor](db::CellDef& cell, const db::Rect& area,
                                                  diag::DiagnosticSink& sink) {
    cmd::CommandContext nested(ctx.services(), ctx.project(), ctx.session(), sink, ctx.registry(), ctx.editor());
    (void)gds::load_area(cell, area, gds::command_subtree_loader(nested, *editor), false,
                         editor->drc.compat_settings());
  };
  const std::string name = p.name.value_or((*target)->name());
  // Through the session's write host (the desktop shows a card and pumps
  // frames, design D16); def::write reports no progress and the file is
  // written only once it is complete. Without a host it runs inline.
  const mask::ProgressHost& host = s.editor->mask.write_host;
  if (!host) {
    return write(*s.editor, **target, name, p.options);
  }
  return host(name, [&](const mask::Progress*) { return write(*s.editor, **target, name, p.options); });
}

}  // namespace hefesto::def
