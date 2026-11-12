// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc_save_report, drc_load_report and the DRC manager (see drc_report.hpp).
// (Magic ref: tcltk/drcmgr.tcl:18-58,159-300)

#include "hefesto/gui/drc_report.hpp"

#include <cstdio>

#include "gui/gui_util.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gui/diag_codes.hpp"
#include "ui/command_util.hpp"

namespace hefesto::gui {

namespace {

using cmd::CommandContext;
using cmd::CommandResult;
using detail::run;

const std::string kSeparator(40, '-');

std::string box_text(const cmd::Value& rect) {
  std::string out;
  for (const std::string& word : detail::words_of(rect)) {
    out += (out.empty() ? "" : " ") + word;
  }
  return out;
}

// `drc listall why` (text, {rect ...}, ...) in internal units, in its order.
std::vector<DrcManagerError> why_errors(CommandContext& ctx) {
  std::vector<DrcManagerError> out;
  const ui::detail::InternalUnits internal{*ctx.editor()};
  const std::vector<cmd::Value> flat = detail::elements_of(detail::run_value(ctx, {"drc", "listall", "why"}));
  for (std::size_t i = 0; i + 1 < flat.size(); i += 2) {
    DrcManagerError error{flat[i].to_string(), {}};
    for (const cmd::Value& rect : detail::elements_of(flat[i + 1])) {
      error.boxes.push_back(box_text(rect));
    }
    out.push_back(std::move(error));
  }
  return out;
}

// `cif scale out` (BINDING's `cif`, Q-SCRIPT-7: float-widened with
// magic-compat ON); 1.0 when it gives no number, as without a CIF style.
double output_scale(CommandContext& ctx) {
  const CommandResult scale = ui::detail::run_nested(ctx, {"cif", "scale", "out"});
  return scale.ok() ? detail::number_of(scale.value).value_or(1.0) : 1.0;
}

std::string format_box(std::string_view box, double scale) {
  double v[4] = {0, 0, 0, 0};
  const std::vector<std::string> words = detail::words_of(cmd::Value{std::string{box}});
  for (std::size_t i = 0; i < 4 && i < words.size(); ++i) {
    v[i] = detail::number_of(cmd::Value{words[i]}).value_or(0.0);
  }
  char line[256];
  std::snprintf(line, sizeof line, " %.3f %.3f %.3f %.3f", scale * v[0], scale * v[1], scale * v[2], scale * v[3]);
  return line;
}

CommandResult write_report(CommandContext& ctx, std::string_view file, const std::string& text) {
  if (!ctx.services().files.write_atomic(detail::session_path(ctx, file), text).ok()) {
    return CommandResult::raised(diag::make(
        codes::kReportOpen, "couldn't open \"" + std::string{file} + "\": no such file or directory"));
  }
  return CommandResult::success();
}

// A word of a box line: a run of non-space characters starting at `pos`.
std::size_t word_end(std::string_view line, std::size_t pos) {
  std::size_t end = pos;
  while (end < line.size() && line[end] != ' ') {
    ++end;
  }
  return end;
}

std::optional<ReportBox> match_at(std::string_view line, std::size_t start) {
  std::string words[4];
  std::size_t pos = start;
  for (std::string& word : words) {
    if (pos >= line.size() || line[pos] != ' ') {
      return std::nullopt;
    }
    const std::size_t end = word_end(line, pos + 1);
    if (end == pos + 1) {
      return std::nullopt;
    }
    word = std::string{line.substr(pos + 1, end - pos - 1)};
    pos = end;
  }
  return ReportBox{words[0], words[1], words[2], words[3]};
}

// The line of `text` at `pos` without its end (\n or \r\n); `pos` moves past it.
std::string_view next_line(std::string_view text, std::size_t& pos) {
  const std::size_t end = text.find('\n', pos);
  std::string_view line = text.substr(pos, end == std::string_view::npos ? std::string_view::npos : end - pos);
  pos = end == std::string_view::npos ? text.size() : end + 1;
  if (!line.empty() && line.back() == '\r') {
    line.remove_suffix(1);
  }
  return line;
}

// drc_load_report's box line: a box of the last rule text, when it matches.
void add_box(std::vector<DrcManagerError>& errors, const std::string& rule, std::string_view line) {
  const auto box = match_report_box(line);
  if (!box) {
    return;
  }
  if (errors.empty() || errors.back().rule != rule) {
    errors.push_back(DrcManagerError{rule, {}});
  }
  errors.back().boxes.push_back(box->llx + " " + box->lly + " " + box->urx + " " + box->ury);
}

}  // namespace

std::string format_drc_report(std::string_view cell, std::string_view count,
                              const std::vector<DrcManagerError>& errors, double scale) {
  std::string out = std::string{cell} + " " + std::string{count} + "\n" + kSeparator + "\n";
  for (const DrcManagerError& error : errors) {
    out += error.rule + "\n" + kSeparator + "\n";
    for (const std::string& box : error.boxes) {
      out += format_box(box, scale) + "\n";
    }
    out += kSeparator + "\n";
  }
  return out + "\n";
}

CommandResult drc_save_report(CommandContext& ctx, std::string_view cell, std::string_view file) {
  if (ctx.editor() == nullptr) {
    return detail::no_session(ctx);
  }
  const std::string outfile = file.empty() ? std::string{kDefaultDrcReport} : std::string{file};
  const double scale = output_scale(ctx);
  std::string name{cell};
  std::string original;
  if (name.empty()) {
    (void)run(ctx, {"select", "top", "cell"});
    name = run(ctx, {"cellname", "list", "self"});
  } else {
    original = run(ctx, {"cellname", "list", "self"});
    detail::note(ctx, codes::kReportNote, "loading " + name + "\n");
    (void)run(ctx, {"load", name});
    (void)run(ctx, {"select", "top", "cell"});
  }
  (void)run(ctx, {"drc", "check"});
  std::string count;
  {
    const ui::detail::InternalUnits internal{*ctx.editor()};
    count = run(ctx, {"drc", "list", "count"});
  }
  const std::string text = format_drc_report(name, count, why_errors(ctx), scale);
  if (!original.empty()) {
    (void)run(ctx, {"load", original});
  }
  CommandResult written = write_report(ctx, outfile, text);
  if (written.ok()) {
    detail::note(ctx, codes::kReportNote, "done with " + outfile + "\n");
  }
  return written;
}

std::optional<ReportBox> match_report_box(std::string_view line) {
  for (std::size_t start = line.find(' '); start != std::string_view::npos; start = line.find(' ', start + 1)) {
    if (auto box = match_at(line, start)) {
      return box;
    }
  }
  return std::nullopt;
}

std::vector<DrcManagerError> parse_drc_report(std::string_view text) {
  std::vector<DrcManagerError> out;
  std::string rule;
  std::size_t pos = 0;
  while (pos < text.size()) {
    const std::string_view line = next_line(text, pos);
    if (!line.empty() && line.front() == ' ') {
      add_box(out, rule, line);
    } else if (line.empty() || line.front() != '-') {
      rule = std::string{line};
    }
  }
  return out;
}

CommandResult drc_load_report(CommandContext& ctx, std::string_view file) {
  if (ctx.editor() == nullptr) {
    return detail::no_session(ctx);
  }
  const std::string infile = file.empty() ? std::string{kDefaultDrcReport} : std::string{file};
  auto text = ctx.services().files.read(detail::session_path(ctx, infile));
  if (!text.ok()) {
    return CommandResult::raised(diag::make(
        codes::kReportOpen, "couldn't open \"" + infile + "\": no such file or directory"));
  }
  detail::note(ctx, codes::kReportNote, "Reading " + infile + "\n");
  (void)run(ctx, {"suspendall"});
  for (const DrcManagerError& error : parse_drc_report(text.value())) {
    for (const std::string& box : error.boxes) {
      const std::vector<std::string> v = detail::words_of(cmd::Value{box});
      const auto um = [&](std::size_t i) { return v[i] + "um"; };
      (void)run(ctx, {"feedback", "add", error.rule, "vert_highlights", um(0), um(1), um(0), um(3), um(2), um(3),
                      um(2), um(1)});
    }
  }
  (void)run(ctx, {"resumeall"});
  detail::note(ctx, codes::kReportNote, "Done.\n");
  detail::note(ctx, codes::kReportNote, "Use \"feedback find\" to enumerate errors.\n");
  return CommandResult::success();
}

DrcManagerModel drc_manager_model(CommandContext& ctx) {
  DrcManagerModel model;
  if (ctx.editor() == nullptr) {
    return model;
  }
  (void)run(ctx, {"suspendall"});
  (void)run(ctx, {"select", "top", "cell"});
  model.cell = run(ctx, {"cellname", "list", "self"});
  model.errors = why_errors(ctx);
  (void)run(ctx, {"resumeall"});
  return model;
}

CommandResult drc_manager_zoom(CommandContext& ctx, std::string_view box) {
  if (ctx.editor() == nullptr) {
    return detail::no_session(ctx);
  }
  std::vector<std::string> values = {"box", "values"};
  for (std::string& word : detail::words_of(cmd::Value{std::string{box}})) {
    values.push_back(std::move(word));
  }
  const ui::detail::InternalUnits internal{*ctx.editor()};
  (void)run(ctx, values);
  (void)run(ctx, {"suspendall"});
  (void)run(ctx, {"findbox", "zoom"});
  (void)run(ctx, {"zoom", "2"});
  (void)run(ctx, {"resumeall"});
  return CommandResult::success();
}

}  // namespace hefesto::gui
