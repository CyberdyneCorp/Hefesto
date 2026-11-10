// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `drc` options that report errors: count, find, why and ignore
// (drc-commands "drc count", "drc find", "drc why", "drc ignore",
// "Scripting results on iPadOS and non-Tcl hosts"). Ported from Magic
// 8.3.694 (fc5daa9e) commands/CmdCD.c CmdDrc (the COUNT, FIND, WHY and
// DRC_IGNORE cases: messages, Tcl result structures, Q-DRC-14 total line)
// and drc/DRCmain.c DRCWhyAll (result list); the searches themselves are
// in reporting.cpp.
// Copyright (C) 1985, 1990 Regents of the University of California
// (Magic's license; see THIRD_PARTY_NOTICES.md).
//
// Owner: COMMANDS (openspec/changes/m3-drc/design.md D2).

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>

#include "command_internal.hpp"
#include "hefesto/drc/commands/reporting.hpp"

namespace hefesto::drc::detail {

namespace {

using cmd::CommandResult;
using cmd::Value;

// strcasecmp(text, "none") == 0.
bool is_none(std::string_view text) {
  return std::ranges::equal(text, std::string_view{"none"}, [](char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) == static_cast<unsigned char>(b);
  });
}

// The per-cell lines of a count without `total`.
void print_counts(Request& q, const std::vector<CellErrorCount>& counts) {
  for (const CellErrorCount& c : counts) {
    print(q, c.count > 1 ? "Cell " + c.cell->name() + " has " + std::to_string(c.count) + " error tiles."
                         : "Cell " + c.cell->name() + " has just one error tile.");
  }
}

Value count_list(const std::vector<CellErrorCount>& counts) {
  Value::List out;
  for (const CellErrorCount& c : counts) {
    out.emplace_back(Value::List{Value{c.cell->name()}, Value{static_cast<std::int64_t>(c.count)}});
  }
  return Value{std::move(out)};
}

std::int64_t total_of(const std::vector<CellErrorCount>& counts) {
  std::int64_t sum = 0;
  for (const CellErrorCount& c : counts) {
    sum += static_cast<std::int64_t>(c.count);
  }
  return sum;
}

/// The `Total DRC errors found:` value of a count without `total` outside
/// list mode: none with background DRC off, else 0 with magic-compat ON
/// (Q-DRC-14, Magic's reset) and the sum with it OFF.
std::optional<std::int64_t> plain_total(const Request& q, const std::vector<CellErrorCount>& counts) {
  if (q.editor.drc.background == BackgroundMode::off) {
    return std::nullopt;
  }
  return q.magic_compat() ? 0 : total_of(counts);
}

// {llx lly urx ury} in the current units (DBWPrintValue: x, then y).
Value rect_value(const Request& q, const db::Rect& r) {
  return Value{Value::List{db::cmdutil::report_value(q.s, r.x0, cmd::Axis::x),
                           db::cmdutil::report_value(q.s, r.y0, cmd::Axis::y),
                           db::cmdutil::report_value(q.s, r.x1, cmd::Axis::x),
                           db::cmdutil::report_value(q.s, r.y1, cmd::Axis::y)}};
}

/// DRCWhyAll's result: text, {rect ...}, text, {rect ...}, ...
Value why_all_value(const Request& q, const std::vector<WhyGroup>& groups) {
  Value::List out;
  for (const WhyGroup& g : groups) {
    Value::List rects;
    for (const db::Rect& r : g.areas) {
      rects.push_back(rect_value(q, r));
    }
    out.emplace_back(g.text);
    out.emplace_back(std::move(rects));
  }
  return Value{std::move(out)};
}

Value texts_value(const std::vector<WhyGroup>& groups) {
  Value::List out;
  for (const WhyGroup& g : groups) {
    out.emplace_back(g.text);
  }
  return Value{std::move(out)};
}

void print_texts(Request& q, const std::vector<WhyGroup>& groups) {
  for (const WhyGroup& g : groups) {
    print(q, g.text);
  }
}

// The box moves and `Error area #N:` lines of every area `drc find` tried.
void show_attempts(Request& q, db::CellDef* root, const FindResult& found) {
  for (const FindAttempt& a : found.attempts) {
    move_box(q.editor, root, a.area);
    if (!q.list) {
      print(q, "Error area #" + std::to_string(a.index) + ":");
      print_texts(q, a.why);
    }
  }
}

}  // namespace

// COUNT: DRCCount over the window (or box) area, recursive in listall mode.
CommandResult drc_count(Request& q) {
  const bool total = q.argc() == 3 && q.argv(2).starts_with("total");
  const auto where = command_area(q.editor);
  if (!where) {
    return CommandResult::success();
  }
  const std::vector<CellErrorCount> counts = count_errors(*where->root, where->area, q.listall);
  if (total) {
    if (q.list) {
      return CommandResult::success(Value{total_of(counts)});
    }
    print(q, "Total DRC errors found: " + std::to_string(total_of(counts)));
    return CommandResult::success();
  }
  if (q.list) {
    return CommandResult::success(count_list(counts));
  }
  print_counts(q, counts);
  if (const auto line = plain_total(q, counts)) {
    print(q, "Total DRC errors found: " + std::to_string(*line));
  }
  return CommandResult::success();
}

// FIND: the next (or nth, or next matching) error of the root cell.
CommandResult drc_find(Request& q) {
  const auto where = command_area(q.editor);
  if (!where) {
    return CommandResult::success();
  }
  if (q.argc() > 3) {
    return bad_usage(q);
  }
  std::optional<std::string_view> argument;
  if (q.argc() == 3) {
    argument = q.argv(2);
  }
  const FindResult found = find_error(q.editor, *where->root, argument);
  show_attempts(q, where->root, found);
  switch (found.status) {
    case FindStatus::found:
      return CommandResult::success(q.list ? texts_value(found.attempts.back().why) : Value{});
    // Magic sets -1 / 0 as the Tcl result, but its TxPrintf writes through
    // the Tcl console, which replaces the result: only the list forms keep it.
    case FindStatus::out_of_range:
      if (!q.list) {
        print_unterminated(q, "There aren't that many errors");
        return CommandResult::success();
      }
      return CommandResult::success(Value{-1});
    case FindStatus::no_errors:
      if (!q.list) {
        print(q, "There are no errors in " + where->root->name() + ".");
        return CommandResult::success();
      }
      return CommandResult::success(Value{0});
    case FindStatus::wrapped:
      break;
  }
  return CommandResult::success();
}

// DRC_IGNORE: list, clear (`none`, any case) or add the matching texts.
CommandResult drc_ignore(Request& q) {
  DrcSession& drc = q.editor.drc;
  if (q.argc() == 2) {
    for (const WhyIndex why : drc.ignore.items()) {
      print(q, std::string{drc.rules->why().text(why)});
    }
    if (drc.ignore.empty()) {
      print(q, "(none)");
    }
    return CommandResult::success();
  }
  if (q.argc() != 3) {
    return bad_usage(q);
  }
  if (is_none(q.argv(2))) {
    drc.ignore.clear();
    return CommandResult::success();
  }
  // Each match is prepended in index order, as Magic links them.
  const std::vector<WhyIndex> matches = matching_whys(drc.rules->why(), q.argv(2), q.magic_compat()).items();
  for (auto it = matches.rbegin(); it != matches.rend(); ++it) {
    drc.ignore.add(*it);
  }
  return CommandResult::success();
}

// WHY: DRCWhy (texts) or, in listall mode, DRCWhyAll (texts and areas).
CommandResult drc_why(Request& q) {
  const auto box = box_window(q.editor);
  if (!box) {
    return CommandResult::success();
  }
  WhyOptions options;
  options.all = q.listall;
  const std::vector<WhyGroup> groups = why_errors(q.editor, *box->root, box->area, options);
  if (groups.empty()) {
    print(q, "No errors found.");
  }
  if (q.listall) {
    return CommandResult::success(why_all_value(q, groups));
  }
  if (q.list) {
    return CommandResult::success(texts_value(groups));
  }
  print_texts(q, groups);
  return CommandResult::success();
}

}  // namespace hefesto::drc::detail
