// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `drc` options that control the checker and print the rule database:
// on, off, status, catchup, check, euclidean, style, printrules, rulestats,
// statistics, *halo, *stepsize, *flatcheck, *showint (drc-commands
// "Background checker control", "drc catchup and drc check", "drc
// euclidean", "drc style", "drc printrules", "drc rulestats and
// statistics", "drc *halo and drc *stepsize", "drc *flatcheck and drc
// *showint"). Ported from Magic 8.3.694 (fc5daa9e) commands/CmdCD.c CmdDrc
// (the option cases and their messages), drc/DRCmain.c DRCPrintStats and
// DRCCatchUp, drc/DRCtech.c DRCPrintStyle and DRCSetStyle.
// Copyright (C) 1985, 1990 Regents of the University of California
// (Magic's license; see THIRD_PARTY_NOTICES.md).
//
// Owner: COMMANDS (openspec/changes/m3-drc/design.md D2).

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>

#include "cell/io_session.hpp"
#include "command_internal.hpp"
#include "hefesto/drc/engine/engine.hpp"
#include "hefesto/drc/rules/query.hpp"
#include "hefesto/drc/rules/style.hpp"

namespace hefesto::drc::detail {

namespace {

using cmd::CommandResult;

/// Internal units in DRC units: value * N / D (C integer arithmetic).
std::int64_t drc_units(const Request& q, std::int64_t internal) {
  const ScaleFactor& f = q.editor.drc.rules->scale();
  return f.d == 0 ? internal : internal * f.n / f.d;
}

std::string units_text(const Request& q, std::int64_t internal) {
  return std::to_string(internal) + " internal units (" + std::to_string(drc_units(q, internal)) + " DRC units)";
}

std::string stat_line(std::string_view label, std::uint64_t recent, std::uint64_t total) {
  return "\n    " + std::string{label} + ": " + std::to_string(recent) + "/" + std::to_string(total);
}

// DRCPrintStyle(FALSE, TRUE, TRUE): the current style and the style list.
void print_styles(Request& q) {
  const DrcSession& drc = q.editor.drc;
  std::string text = "The current style is \"" + drc.style + "\".\nThe DRC styles are: ";
  for (std::size_t i = 0; i < drc.styles.names.size(); ++i) {
    text += (i > 0 ? ", " : "") + drc.styles.names[i];
  }
  print(q, text + ".");
}

// DRCSetStyle: unique-prefix selection, then drcLoadStyle unless the style
// is already current (DRCTechFinal resets the halo and step size).
CommandResult set_style(Request& q, const std::string& name) {
  DrcSession& drc = q.editor.drc;
  const diag::Outcome<std::string> selected = select_style(drc.styles, name);
  if (!selected.ok()) {
    report_failure(q, selected.error());
    print_styles(q);
    return CommandResult::success();
  }
  if (selected.value() != drc.style) {
    drc.rules = load_style(*q.editor.technology, selected.value(), q.editor.grid, drc.cif, drc.compat_settings(),
                           q.ctx.sink());
    drc.style = selected.value();
    drc.halo_override.reset();
    drc.step_size.reset();
    drc.events.publish_style(drc.style);
  }
  q.ctx.emit(diag::make("DRC-TECH-004", "DRC style is now \"" + name + "\""));  // RULES' code for the message
  return CommandResult::success();
}

/// cmdScaleCoord(w, text, FALSE, TRUE, 1) for *halo and *stepsize.
diag::Outcome<db::Coord> distance(const Request& q, const std::string& text) {
  return db::cmdutil::parse_distance(q.s, text, cmd::Axis::x, false);
}

}  // namespace

// FLATCHECK: DRCFlatCheck over the box.
CommandResult drc_flatcheck(Request& q) {
  const auto box = box_window(q.editor);
  if (!box) {
    return CommandResult::success();
  }
  const diag::Outcome<std::size_t> count = flat_check(q.editor, *box->root, box->area);
  if (!count.ok()) {
    report_failure(q, count.error());
    return CommandResult::success();
  }
  print(q, std::to_string(count.value()) + " total errors found.");
  return CommandResult::success();
}

// DRC_HALO: the halo in effect, never above the style halo.
CommandResult drc_halo(Request& q) {
  DrcSession& drc = q.editor.drc;
  const std::int32_t style_halo = drc.rules->halo().value;
  if (q.argc() != 3) {
    const std::int32_t halo = current_halo(q.editor);
    std::string text = "DRC halo is " + units_text(q, halo);
    if (halo != style_halo) {
      text += "\nMaximum rule distance is " + units_text(q, style_halo);
    }
    print(q, std::move(text));
    return CommandResult::success();
  }
  const auto d = distance(q, q.argv(2));
  if (!d.ok()) {
    return CommandResult::failure(d.error());
  }
  const std::int32_t halo = std::min(d.value(), style_halo);
  drc.halo_override = halo;
  print(q, halo < style_halo ? "Warning: rulechecking limited to halo of " + units_text(q, halo) + "."
                             : "DRC checks all rules (halo of " + std::to_string(halo) + " internal units, or " +
                                   std::to_string(drc_units(q, halo)) + " DRC units)");
  return CommandResult::success();
}

// SHOWINT: DRCFindInteractions under the box for the radius; the box moves
// to the interaction area.
CommandResult drc_showint(Request& q) {
  if (q.argc() != 3) {
    return bad_usage(q);
  }
  const auto radius = db::cmdutil::parse_distance(q.s, q.argv(2), cmd::Axis::y, true);  // cmdParseCoord(TRUE, FALSE)
  if (!radius.ok()) {
    return CommandResult::failure(radius.error());
  }
  if (radius.value() < 0) {
    print(q, "Radius must not be negative");
    return CommandResult::success();
  }
  const auto box = box_window(q.editor);
  if (!box) {
    return CommandResult::success();
  }
  const auto found = interaction_area(q.editor, *box->root, box->area, radius.value());
  if (!found.ok()) {
    report_failure(q, found.error());
    return CommandResult::success();
  }
  if (!found.value()) {
    print(q, "No interactions in this area for that radius.");
    return CommandResult::success();
  }
  move_box(q.editor, box->root, *found.value());
  return CommandResult::success();
}

// DRC_STEPSIZE: the step size in effect (Magic's message keeps its "))").
CommandResult drc_stepsize(Request& q) {
  DrcSession& drc = q.editor.drc;
  if (q.argc() == 3) {
    const auto d = distance(q, q.argv(2));
    if (!d.ok()) {
      return CommandResult::failure(d.error());
    }
    drc.step_size = d.value();
    print(q, "DRC step size is now " + units_text(q, d.value()) + ")");
    return CommandResult::success();
  }
  const std::int64_t step = current_step_size(q.editor);
  const std::int64_t recommended = 16 * std::int64_t{drc.rules->halo().value};
  std::string text = "DRC step size is " + units_text(q, step);
  if (step != recommended) {
    text += "\nRecommended step size is " + units_text(q, recommended);
  }
  print(q, std::move(text));
  return CommandResult::success();
}

// CATCHUP: DRCCatchUp runs the checker whatever the background state and
// restores it. With work pending (DRCHasWork), DRCContinuous evaluates
// `after idle magic::drcstate busy` and `... idle`, which leaves the second
// `after` id as the command's Tcl result.
CommandResult drc_catchup(Request& q) {
  DrcSession& drc = q.editor.drc;
  const BackgroundMode saved = drc.background;
  const bool had_work = !drc.pending.empty();
  const diag::Status status = catchup(q.editor, &q.ctx.sink());
  drc.background = saved;
  if (!status.ok()) {
    report_failure(q, status.error());
  }
  if (!had_work) {
    return CommandResult::success();
  }
  drc.tcl_after_id += 2;
  return CommandResult::success(cmd::Value{"after#" + std::to_string(drc.tcl_after_id)});
}

// CHECK: DRCCheck marks the box area in every cell under it.
CommandResult drc_check(Request& q) {
  const auto box = box_window(q.editor);
  if (!box) {
    return CommandResult::success();
  }
  const auto read = [&q](db::CellDef& def) { return cell::detail::read_for_expansion(q.ctx, def); };
  const diag::Status status = force_recheck(q.editor, *box->root, box->area, read);
  if (!status.ok()) {
    report_failure(q, status.error());
  }
  return CommandResult::success();
}

// EUCLIDEAN: `on` (exactly) enables, any other word disables.
CommandResult drc_euclidean(Request& q) {
  bool& euclidean = q.editor.drc.euclidean;
  if (q.argc() == 2) {
    print(q, std::string{"DRC measurements are "} + (euclidean ? "Euclidean" : "Manhattan") + ".");
  } else {
    euclidean = q.argv(2) == "on";
  }
  return CommandResult::success();
}

// DRC_OFF: background checking off; a square in progress was already
// abandoned when the command took the database lock (design D8).
CommandResult drc_off(Request& q) {
  q.editor.drc.background = BackgroundMode::off;
  return CommandResult::success();
}

// DRC_ON: only from off (a "not set" state is left alone).
CommandResult drc_on(Request& q) {
  DrcSession& drc = q.editor.drc;
  if (drc.background == BackgroundMode::off) {
    drc.background = BackgroundMode::on;
    if (drc.worker != nullptr) {
      drc.worker->wake();
    }
  }
  return CommandResult::success();
}

// DRC_STATUS: the Tcl boolean; "not set" counts as on.
CommandResult drc_status(Request& q) {
  return CommandResult::success(cmd::Value{q.editor.drc.background == BackgroundMode::off ? 0 : 1});
}

// DRC_STYLE: DRCPrintStyle(dolist, doforall, !doforall) or DRCSetStyle.
CommandResult drc_style(Request& q) {
  if (q.argc() != 2) {
    return set_style(q, q.argv(2));
  }
  const DrcSession& drc = q.editor.drc;
  if (q.listall) {
    cmd::Value::List names;
    for (const std::string& name : drc.styles.names) {
      names.emplace_back(name);
    }
    return CommandResult::success(cmd::Value{std::move(names)});
  }
  if (q.list) {
    return CommandResult::success(cmd::Value{drc.style});
  }
  print(q, "The current style is \"" + drc.style + "\".");
  return CommandResult::success();
}

// PRINTRULES: DRCPrintRulesTable to the terminal or a file.
CommandResult drc_printrules(Request& q) {
  if (q.argc() > 3) {
    return bad_usage(q);
  }
  std::ostringstream text;
  print_rules(*q.editor.drc.rules, *q.editor.technology, text);
  if (q.argc() < 3) {
    if (!text.str().empty()) {
      db::cmdutil::print_block(q.s, text.str());
    }
    return CommandResult::success();
  }
  if (!q.ctx.services().files.write_atomic(q.argv(2), text.str()).ok()) {
    warn(q, "Cannot write file " + q.argv(2));
  }
  return CommandResult::success();
}

// RULESTATS: DRCTechRuleStats.
CommandResult drc_rulestats(Request& q) {
  std::ostringstream text;
  rule_stats(*q.editor.drc.rules, text);
  if (!text.str().empty()) {
    db::cmdutil::print_block(q.s, text.str());
  }
  return CommandResult::success();
}

// STATISTICS: DRCPrintStats (recent/total; the recent counters restart).
CommandResult drc_statistics(Request& q) {
  CheckStatistics& recent = q.editor.drc.statistics;
  CheckStatistics& total = q.editor.drc.statistics_total;  // Magic's drcTotal* counters
  total += recent;
  std::string text = "Design-rule checker statistics (recent/total):";
  text += stat_line("Squares processed", recent.squares, total.squares);
  text += stat_line("Tiles processed", recent.tiles, total.tiles);
  text += stat_line("Edges pieces processed", recent.edges, total.edges);
  text += stat_line("Constraint areas checked", recent.rules, total.rules);
  text += stat_line("Multi-tile constraints", recent.slow, total.slow);
  text += stat_line("Interaction areas processed", recent.interactions, total.interactions);
  text += stat_line("Tiles processed for interactions", recent.interaction_tiles, total.interaction_tiles);
  text += stat_line("Tiles processed for arrays", recent.array_tiles, total.array_tiles);
  const std::uint64_t cif_tiles = recent.cif_tiles;  // not printed, not reset (DRCstatCifTiles)
  recent = CheckStatistics{};
  recent.cif_tiles = cif_tiles;
  print(q, std::move(text));
  return CommandResult::success();
}

}  // namespace hefesto::drc::detail
