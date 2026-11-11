// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "gui/gui_util.hpp"

#include <cstdlib>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gui/diag_codes.hpp"
#include "ui/command_util.hpp"

namespace hefesto::gui::detail {

std::string run(cmd::CommandContext& ctx, const std::vector<std::string>& words) {
  return run_value(ctx, words).to_string();
}

cmd::Value run_value(cmd::CommandContext& ctx, const std::vector<std::string>& words) {
  return ui::detail::run_forwarded(ctx, words).value;
}

void note(cmd::CommandContext& ctx, std::string_view code, std::string text) {
  ctx.emit(diag::make(code, std::move(text)));
}

platform::Path session_path(cmd::CommandContext& ctx, std::string_view name) {
  platform::Path path{std::string{name}};
  if (path.is_relative() && ctx.editor() != nullptr) {
    const platform::Path cwd = ctx.editor()->cell_session.file_host().current_directory();
    if (!cwd.empty()) {
      path = cwd / path;
    }
  }
  return path;
}

std::vector<std::string> words_of(const cmd::Value& value) {
  return ui::detail::value_words(value);
}

std::vector<cmd::Value> elements_of(const cmd::Value& value) {
  if (const auto* list = std::get_if<cmd::Value::List>(&value.data())) {
    return *list;
  }
  std::vector<cmd::Value> out;
  for (std::string& word : words_of(value)) {
    out.emplace_back(std::move(word));
  }
  return out;
}

std::optional<double> number_of(const cmd::Value& value) {
  if (const auto* i = std::get_if<std::int64_t>(&value.data())) {
    return static_cast<double>(*i);
  }
  if (const auto* d = std::get_if<double>(&value.data())) {
    return *d;
  }
  const std::string text = value.to_string();
  if (text.empty()) {
    return std::nullopt;
  }
  char* end = nullptr;
  const double v = std::strtod(text.c_str(), &end);
  return end != nullptr && *end == '\0' ? std::optional<double>{v} : std::nullopt;
}

cmd::CommandResult no_session(const cmd::CommandContext& ctx) {
  return cmd::CommandResult::failure(cmd::require_editor(ctx).error());
}

cmd::CommandResult wrong_args(std::string_view usage) {
  return cmd::CommandResult::raised(
      diag::make(codes::kUsage, "wrong # args: should be \"" + std::string{usage} + "\""));
}

}  // namespace hefesto::gui::detail
