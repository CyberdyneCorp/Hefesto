// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `undo` and `redo` (layout-undo "undo command", "redo command"). Owner:
// UNDO. Argument handling follows Magic VLSI 8.3.694 (fc5daa9e)
// windows/windCmdSZ.c (windUndoCmd) and windows/windCmdNR.c (windRedoCmd),
// Copyright (C) 1985, 1990 Regents of the University of California
// (permissive license, THIRD_PARTY_NOTICES.md). Magic's TxError lines are
// UNDO-CMD-001 failures (UNDO-CMD-002 for the disabled-undo warning); its
// TxPrintf lines are UNDO-REPORT-001 reports.

#include "hefesto/undo/commands.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/undo/diag_codes.hpp"
#include "hefesto/undo/undo_log.hpp"

namespace hefesto::undo {

namespace {

using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;

constexpr std::string_view kUndoSynopsis = "undo ?count? | undo print count | undo enable|disable";
constexpr std::string_view kRedoSynopsis = "redo ?count? | redo print count";

/// Magic's StrIsInt + atoi: an optional sign and decimal digits (an empty
/// string and a lone sign are integers worth 0). Values beyond the int64
/// range saturate (Magic's atoi is undefined there).
std::optional<std::int64_t> magic_int(std::string_view text) {
  bool negative = false;
  if (!text.empty() && (text.front() == '-' || text.front() == '+')) {
    negative = text.front() == '-';
    text.remove_prefix(1);
  }
  for (const char c : text) {
    if (c < '0' || c > '9') {
      return std::nullopt;
    }
  }
  std::int64_t value = 0;
  if (!text.empty()) {
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec == std::errc::result_out_of_range) {
      value = std::numeric_limits<std::int64_t>::max();
    }
    (void)ptr;
  }
  return negative ? -value : value;
}

CommandResult usage_error(std::string text) {
  return CommandResult::failure(diag::make(codes::kBadUsage, std::move(text)));
}

void report(CommandContext& ctx, std::string text) {
  ctx.emit(diag::make(codes::kReport, std::move(text)));
}

/// The lines the replayed events printed (UndoLog::note).
void report_notes(CommandContext& ctx, editor::EditorState& editor) {
  for (std::string& line : editor.undo.take_notes()) {
    report(ctx, std::move(line));
  }
}

/// `undo print N` / `redo print N` (shared by both commands).
CommandResult print_log(CommandContext& ctx, const editor::EditorState& editor, const Invocation& inv,
                        bool before) {
  const auto count = magic_int(inv.args[1]);
  if (inv.args[0].compare(0, 5, "print") != 0 || !count) {
    return usage_error(before ? "Usage: undo print count" : "Usage: redo print count");
  }
  std::string listing = editor.undo.print(*count, before);
  if (!listing.empty() && listing.back() == '\n') {
    listing.pop_back();
  }
  if (!listing.empty()) {
    report(ctx, std::move(listing));
  }
  return CommandResult::success();
}

CommandResult replay_undo(CommandContext& ctx, editor::EditorState& editor, std::size_t count) {
  const bool compat = ctx.project().compat.magic_compat;
  auto undone = editor.undo.undo(editor, count, compat);
  report_notes(ctx, editor);
  if (!undone.ok()) {
    ctx.emit(undone.error());
  }
  if (!undone.ok() || undone.value().steps == 0) {
    report(ctx, "Nothing more to undo");
  }
  return CommandResult::success();
}

/// `undo enable|disable`, or the "Option must be a count" error.
CommandResult undo_keyword(editor::EditorState& editor, std::string_view word) {
  if (word == "enable") {
    editor.undo.resume();
  } else if (word == "disable") {
    editor.undo.suspend();
  } else {
    return usage_error("Option must be a count (integer)");
  }
  return CommandResult::success();
}

CommandResult undo_command(CommandContext& ctx, const Invocation& inv) {
  auto ed = cmd::require_editor(ctx);
  if (!ed.ok()) {
    return CommandResult::failure(ed.error());
  }
  editor::EditorState& editor = *ed.value();
  if (inv.args.size() > 2) {
    return usage_error("Usage: undo [count]\n       undo print [count]\n       undo enable|disable");
  }
  if (inv.args.size() == 2) {
    return print_log(ctx, editor, inv, true);
  }
  std::int64_t count = 1;
  if (inv.args.size() == 1) {
    const auto n = magic_int(inv.args[0]);
    if (!n) {
      return undo_keyword(editor, inv.args[0]);
    }
    if (*n < 0) {
      return usage_error("Count must be a positive integer");
    }
    count = *n;
  }
  if (count == 0) {
    // D-11 / Q-MODEL-30: `undo 0` is `undo enable` with magic-compat ON.
    if (ctx.project().compat.magic_compat) {
      editor.undo.resume();
    }
    return CommandResult::success();
  }
  return replay_undo(ctx, editor, static_cast<std::size_t>(count));
}

CommandResult redo_command(CommandContext& ctx, const Invocation& inv) {
  auto ed = cmd::require_editor(ctx);
  if (!ed.ok()) {
    return CommandResult::failure(ed.error());
  }
  editor::EditorState& editor = *ed.value();
  if (inv.args.size() > 2) {
    return usage_error("Usage: redo [count]");
  }
  if (inv.args.size() == 2) {
    return print_log(ctx, editor, inv, false);
  }
  std::int64_t count = 1;
  if (inv.args.size() == 1) {
    const auto n = magic_int(inv.args[0]);
    if (!n) {
      return usage_error("Count must be numeric");
    }
    if (*n < 0) {
      return usage_error("Count must be a positive integer");
    }
    count = *n;
  }
  if (count == 0) {
    // D-11 / Q-MODEL-30: `redo 0` is `undo disable` with magic-compat ON.
    if (ctx.project().compat.magic_compat) {
      editor.undo.suspend();
    }
    return CommandResult::success();
  }
  const auto redone = editor.undo.redo(editor, static_cast<std::size_t>(count), ctx.project().compat.magic_compat);
  report_notes(ctx, editor);
  if (redone.steps == 0) {
    report(ctx, "Nothing more to redo");
  }
  return CommandResult::success();
}

}  // namespace

bool register_undo_commands(cmd::CommandRegistry& registry) {
  bool ok = !registry
                 .add(cmd::CommandSpec{"redo", std::string{kRedoSynopsis},
                                       "Redoes the last undone commands (redo N: up to N), or lists the undone "
                                       "log entries (redo print N).",
                                       redo_command})
                 .has_value();
  ok = !registry
            .add(cmd::CommandSpec{"undo", std::string{kUndoSynopsis},
                                  "Undoes the last commands (undo N: up to N), lists the log (undo print N), or "
                                  "suspends and resumes recording (undo disable, undo enable).",
                                  undo_command})
            .has_value() &&
       ok;
  return ok;
}

}  // namespace hefesto::undo
