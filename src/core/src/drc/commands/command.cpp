// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `drc` command: Magic's option table, option lookup, the list and
// listall words, the extra-argument rule, help, and the shared helpers
// (drc-commands "Command syntax and option matching"). Ported from Magic
// 8.3.694 (fc5daa9e) commands/CmdCD.c CmdDrc (option table and help lines
// verbatim) and utils/lookup.c Lookup; dbwind/DBWtools.c ToolGetBoxWindow,
// ToolMoveBox, ToolMoveCorner.
// Copyright (C) 1985, 1990 Regents of the University of California
// (Magic's license; see THIRD_PARTY_NOTICES.md).
//
// Owner: COMMANDS (openspec/changes/m3-drc/design.md D2).

#include "hefesto/drc/commands/command.hpp"

#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

#include "command_internal.hpp"
#include "db/edit_util.hpp"
#include "hefesto/cmd/context.hpp"

namespace hefesto::drc {

namespace {

// cmdDrcOption, in Magic's order (the help lines verbatim, tabs included).
constexpr std::array kOptions = {
    DrcOption{"*flatcheck", "*flatcheck             check box area by flattening", true, false},
    DrcOption{"*halo", "*halo [d]\t\tlimit error checking to areas of d units", true, true},
    DrcOption{"*showint", "*showint radius        show interaction area under box", true, true},
    DrcOption{"*stepsize", "*stepsize [d]\t\tchange DRC step size to d units", true, true},
    DrcOption{"catchup", "catchup                run checker and wait for it to complete", false, false},
    DrcOption{"check", "check                  recheck area under box in all cells", false, false},
    DrcOption{"count", "count [total]          count error tiles in each cell under box", false, true},
    DrcOption{"euclidean", "euclidean on|off\tenable/disable Euclidean geometry checking", false, true},
    DrcOption{"find", "find [nth]     \tlocate next (or nth) error in the layout", false, true},
    DrcOption{"help", "help                   print this help information", false, true},
    DrcOption{"ignore", "ignore\t[<text>|none]   do not report on rules with this text", false, true},
    DrcOption{"off", "off                    turn off background checker", false, false},
    DrcOption{"on", "on                     reenable background checker", false, false},
    DrcOption{"status", "status\t\t\treport if the drc checker is on or off", false, false},
    DrcOption{"style", "style\t\t\tset the DRC style", false, true},
    DrcOption{"printrules", "printrules [file]      print out design rules in file or on tty", false, true},
    DrcOption{"rulestats", "rulestats              print out stats about design rule database", false, false},
    DrcOption{"statistics", "statistics             print out statistics gathered by checker", false, false},
    DrcOption{"why", "why                    print out reasons for errors under box", false, false},
};

constexpr std::size_t kHelp = 9;  // DRC_HELP

// Parallel to kOptions.
constexpr std::array<detail::OptionHandler, kOptions.size()> kHandlers = {
    detail::drc_flatcheck, detail::drc_halo,      detail::drc_showint,    detail::drc_stepsize,
    detail::drc_catchup,   detail::drc_check,     detail::drc_count,      detail::drc_euclidean,
    detail::drc_find,      detail::drc_help,      detail::drc_ignore,     detail::drc_off,
    detail::drc_on,        detail::drc_status,    detail::drc_style,      detail::drc_printrules,
    detail::drc_rulestats, detail::drc_statistics, detail::drc_why,
};

// Lookup's character test: equal, or the same letter in another case.
bool same_letter(char table, char typed) noexcept {
  const auto t = static_cast<unsigned char>(table);
  const auto s = static_cast<unsigned char>(typed);
  return t == s || (std::isalpha(t) != 0 && std::isalpha(s) != 0 && std::tolower(t) == std::tolower(s));
}

// The word without a leading "::magic::" or "magic::" (MAGIC_WRAPPER).
std::string_view without_namespace(std::string_view word) noexcept {
  for (const std::string_view ns : {std::string_view{"::magic::"}, std::string_view{"magic::"}}) {
    if (word.starts_with(ns)) {
      return word.substr(ns.size());
    }
  }
  return word;
}

enum class LineMatch : unsigned char { none, prefix, exact };

LineMatch match_line(std::string_view word, std::string_view line) noexcept {
  std::size_t i = 0;
  while (i < word.size() && i < line.size() && line[i] != ' ' && same_letter(line[i], word[i])) {
    ++i;
  }
  if (i < word.size()) {
    return LineMatch::none;
  }
  return i == line.size() || line[i] == ' ' ? LineMatch::exact : LineMatch::prefix;
}

/// The request for `invocation`: the list words consumed, Magic's argv left.
detail::Request make_request(cmd::CommandContext& ctx, const db::cmdutil::Session& s,
                             const cmd::Invocation& invocation) {
  detail::Request q{ctx, s, *s.editor, invocation.args, false, false, 0};
  if (!q.words.empty() && q.words.front().starts_with("list")) {
    q.list = true;
    q.listall = q.words.front().starts_with("listall");
    q.words.erase(q.words.begin());
  }
  return q;
}

cmd::CommandResult run_drc(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  const auto s = db::cmdutil::session(ctx);
  if (!s.ok()) {
    return cmd::CommandResult::failure(s.error());
  }
  detail::Request q = make_request(ctx, s.value(), invocation);
  if (q.words.empty()) {
    // Also `drc list` / `drc listall` alone (Q-DRC-22: help, both modes).
    detail::warn(q, "No option given in \":drc\" command.");
    q.option = kHelp;
    return detail::drc_help(q);
  }
  const int option = lookup_option(q.words.front());
  if (option < 0) {
    detail::warn(q, q.words.front() + " isn't a valid drc option.");
    q.words.resize(1);
    q.option = kHelp;
    return detail::drc_help(q);
  }
  q.option = static_cast<std::size_t>(option);
  if (q.argc() > 2 && !kOptions[q.option].takes_arguments) {
    return detail::bad_usage(q);
  }
  return kHandlers[q.option](q);
}

}  // namespace

std::span<const DrcOption> drc_options() noexcept {
  return kOptions;
}

int lookup_option(std::string_view word) noexcept {
  const std::string_view typed = without_namespace(word);
  int match = -2;
  for (std::size_t i = 0; i < kOptions.size(); ++i) {
    const LineMatch m = match_line(typed, kOptions[i].help);
    if (m == LineMatch::exact) {
      return static_cast<int>(i);
    }
    if (m == LineMatch::prefix) {
      match = match == -2 ? static_cast<int>(i) : -1;
    }
  }
  return match;
}

bool register_drc_command(cmd::CommandRegistry& registry) {
  cmd::CommandSpec spec{
      "drc",
      "drc option",
      "design rule checker; type \"drc help\" for information on options",
      run_drc,
  };
  return !registry.add(std::move(spec)).has_value();
}

namespace detail {

void print(Request& q, std::string text) {
  q.ctx.emit(diag::make(kReport, std::move(text)));
}

void print_unterminated(Request& q, std::string text) {
  diag::Diagnostic d = diag::make(kReport, std::move(text));
  d.no_newline = true;
  q.ctx.emit(std::move(d));
}

void warn(Request& q, std::string text) {
  q.ctx.emit(diag::make(kWarning, std::move(text)));
}

void report_failure(Request& q, const diag::Diagnostic& error) {
  q.ctx.emit(error);
}

cmd::CommandResult bad_usage(Request& q) {
  warn(q, "Wrong arguments in \"drc " + q.argv(1) + "\" command:\n    drc " +
              std::string{kOptions[q.option].help} + "\nTry \"drc help\" for more help.");
  return cmd::CommandResult::success();
}

// DRC_HELP: "help wizard" (exactly, as the only argument) adds the `*` options.
cmd::CommandResult drc_help(Request& q) {
  const bool wizard = q.argc() == 3 && q.argv(2) == "wizard";
  std::string text = "DRC commands have the form \":drc [list|listall] option\", where option is one of:";
  for (const DrcOption& option : kOptions) {
    if (!option.wizard || wizard) {
      text += "\n    ";
      text += option.help;
    }
  }
  print(q, std::move(text));
  return cmd::CommandResult::success();
}

std::optional<RootArea> box_window(const editor::EditorState& editor) {
  if (!editor.box) {
    return std::nullopt;
  }
  db::CellDef* root = db::edit::box_root(editor);
  std::optional<RootArea> found;
  editor.windows.for_each([&](view::LayoutWindow& w) {
    if (!found && root != nullptr && w.root() == root) {
      found = RootArea{root, *editor.box};
    }
  });
  return found;
}

std::optional<RootArea> command_area(const editor::EditorState& editor) {
  if (const view::LayoutWindow* w = editor.windows.current(); w != nullptr && w->root() != nullptr) {
    return RootArea{w->root(), w->mapping().surface};
  }
  return box_window(editor);
}

void move_box(editor::EditorState& editor, db::CellDef* root, const db::Rect& area) {
  if (const std::optional<db::Rect> old = editor.box; old && db::edit::box_root(editor) == root) {
    const std::int64_t x1 = std::int64_t{area.x0} + old->width();
    const std::int64_t y1 = std::int64_t{area.y0} + old->height();
    if (x1 <= db::kPlaneMax && y1 <= db::kPlaneMax) {
      db::edit::set_box(editor, root, db::Rect{area.x0, area.y0, static_cast<db::Coord>(x1), static_cast<db::Coord>(y1)});
    }
  }
  db::edit::set_box(editor, root, area);
}

}  // namespace detail

}  // namespace hefesto::drc
