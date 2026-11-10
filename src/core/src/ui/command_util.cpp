// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private helpers of the ui commands (see command_util.hpp). Owner: DISPATCH.

#include "ui/command_util.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <numeric>
#include <utility>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/coordinate_units.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/ui/diag_codes.hpp"
#include "hefesto/ui/line.hpp"
#include "hefesto/ui/state.hpp"

namespace hefesto::ui::detail {

cmd::CommandResult usage(std::string text) {
  return cmd::CommandResult::failure(diag::make(codes::kUsage, std::move(text)));
}

cmd::CommandResult raise(std::string_view code, std::string text) {
  return cmd::CommandResult::raised(diag::make(code, std::move(text)));
}

void print(cmd::CommandContext& ctx, std::string text) {
  ctx.emit(diag::make(codes::kReport, std::move(text)));
}

diag::Outcome<editor::EditorState*> session(const cmd::CommandContext& ctx) {
  return cmd::require_editor(ctx);
}

std::string running_client(const cmd::CommandContext& ctx) {
  return ctx.editor() == nullptr ? std::string{cmd::kLayoutClient} : state(*ctx.editor()).client;
}

cmd::CommandResult run_nested(cmd::CommandContext& ctx, const std::vector<std::string>& words, cmd::Origin origin) {
  if (words.empty()) {
    return cmd::CommandResult::success();
  }
  // Bound and sent command lines are Tcl lines in Magic: exact names (Q-UI-41).
  const cmd::CommandLookup found =
      ctx.registry().resolve(words.front(), running_client(ctx), true, cmd::NameLookup::exact);
  if (found.status == cmd::LookupStatus::ambiguous) {
    return cmd::CommandResult::failure(diag::make(codes::kAmbiguous, "That command abbreviation is ambiguous."));
  }
  if (found.status == cmd::LookupStatus::not_found) {
    return cmd::CommandResult::failure(
        diag::make(diag::codes::kCmdDispatch, cmd::invalid_command_name(words.front())));
  }
  const cmd::Invocation inv{found.spec->name, {words.begin() + 1, words.end()}, origin, std::nullopt};
  return found.spec->handler(ctx, inv);
}

cmd::CommandResult run_forwarded(cmd::CommandContext& ctx, const std::vector<std::string>& words) {
  cmd::CommandResult result = run_nested(ctx, words);
  for (const auto& d : result.diagnostics) {
    ctx.emit(d);
  }
  result.diagnostics.clear();
  return result;
}

cmd::CommandResult run_line(cmd::CommandContext& ctx, std::string_view line) {
  const SplitLine split = split_command_line(line);
  if (split.unmatched_quote) {
    ctx.emit(unmatched_quote_warning(*split.unmatched_quote));
  }
  cmd::CommandResult last = cmd::CommandResult::success();
  for (const auto& words : split.commands) {
    last = run_forwarded(ctx, words);
    if (!last.ok()) {
      break;
    }
  }
  return last;
}

std::vector<std::string> value_words(const cmd::Value& value) {
  std::vector<std::string> out;
  if (const auto* list = std::get_if<cmd::Value::List>(&value.data())) {
    for (const auto& v : *list) {
      out.push_back(v.to_string());
    }
    return out;
  }
  const std::string text = value.to_string();
  std::size_t i = 0;
  while (i < text.size()) {
    const std::size_t start = text.find_first_not_of(" \t\n", i);
    if (start == std::string::npos) {
      break;
    }
    const std::size_t stop = text.find_first_of(" \t\n", start);
    out.push_back(text.substr(start, stop == std::string::npos ? std::string::npos : stop - start));
    i = stop == std::string::npos ? text.size() : stop;
  }
  return out;
}

std::optional<std::int64_t> parse_int(std::string_view text) {
  if (text.starts_with('+')) {
    text.remove_prefix(1);
  }
  std::int64_t value = 0;
  const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (ec != std::errc{} || end != text.data() + text.size() || text.empty()) {
    return std::nullopt;
  }
  return value;
}

std::int64_t atoi_like(std::string_view text) {
  const std::size_t start = text.find_first_not_of(" \t");
  if (start == std::string_view::npos) {
    return 0;
  }
  text.remove_prefix(start);
  if (text.starts_with('+')) {
    text.remove_prefix(1);
  }
  std::int64_t value = 0;
  (void)std::from_chars(text.data(), text.data() + text.size(), value);
  return value;
}

InternalUnits::InternalUnits(editor::EditorState& editor) : editor_(editor), saved_(editor.units) {
  editor_.units = cmd::UnitsState{cmd::InputUnit::internal, false};
}

InternalUnits::~InternalUnits() {
  editor_.units = saved_;
}

cmd::CoordinateUnits coordinate_units(const editor::EditorState& editor) {
  return editor::coordinate_units(editor);
}

double microns_per_unit(const editor::EditorState& editor) {
  std::int64_t scale = editor.grid.lambda;
  std::int64_t expander = editor.grid.internal;
  if (const auto& cif = editor.technology->cif_output_grid()) {
    scale *= cif->scale_factor;
    expander *= cif->expander;
  }
  return static_cast<double>(scale) / static_cast<double>(expander) / 100.0;
}

bool add(cmd::CommandRegistry& registry, std::string_view name, std::string_view synopsis, std::string_view help,
         cmd::Handler handler) {
  return !registry.add({std::string{name}, std::string{synopsis}, std::string{help}, std::move(handler)}).has_value();
}

}  // namespace hefesto::ui::detail
