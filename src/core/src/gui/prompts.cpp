// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The file prompts (see prompts.hpp).
// (Magic ref: tcltk/wrapper.tcl:66-231,1033-1048)

#include "hefesto/gui/prompts.hpp"

#include <algorithm>

#include "gui/gui_util.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gui/diag_codes.hpp"
#include "ui/command_util.hpp"

namespace hefesto::gui {

namespace {

using cmd::CommandContext;
using cmd::CommandResult;
using platform::FilePickerRequest;
using FileTypes = std::vector<std::pair<std::string, std::string>>;
using detail::run;

const FileTypes kMagicTypes = {{"Magic", ".mag"}, {"All files", "*"}};
const FileTypes kGdsTypes = {{"GDS", ".gds .strm .cal"}, {"All files", "*"}};
const FileTypes kCifTypes = {{"CIF", ".cif"}, {"All files", "*"}};

std::string pick(platform::FilePicker* picker, FilePickerRequest::Mode mode, const FileTypes& types,
                 std::string title = {}) {
  if (picker == nullptr) {
    return {};
  }
  FilePickerRequest request;
  request.mode = mode;
  request.title = std::move(title);
  request.file_types = types;
  const auto chosen = picker->pick(request);
  return chosen ? chosen->generic_string() : std::string{};
}

// Tcl's `file root`, `file tail` and `file dirname` on a path string.
std::string file_root(const std::string& name) {
  const std::size_t slash = name.rfind('/');
  const std::size_t dot = name.rfind('.');
  return dot == std::string::npos || (slash != std::string::npos && dot < slash) ? name : name.substr(0, dot);
}

std::string file_tail(const std::string& name) {
  const std::size_t slash = name.rfind('/');
  return slash == std::string::npos ? name : name.substr(slash + 1);
}

std::string file_dirname(const std::string& name) {
  const std::size_t slash = name.rfind('/');
  if (slash == std::string::npos) {
    return ".";
  }
  return slash == 0 ? "/" : name.substr(0, slash);
}

bool contains(const std::vector<std::string>& list, const std::string& word) {
  return std::find(list.begin(), list.end(), word) != list.end();
}

std::string joined(const std::vector<std::string>& words) {
  std::string out;
  for (const std::string& w : words) {
    out += (out.empty() ? "" : " ") + w;
  }
  return out;
}

void load_gds(CommandContext& ctx, const std::string& file) {
  const std::vector<std::string> before = detail::words_of(detail::run_value(ctx, {"cellname", "list", "top"}));
  (void)run(ctx, {"gds", "read", file_root(file)});
  std::vector<std::string> fresh;
  for (const std::string& cell : detail::words_of(detail::run_value(ctx, {"cellname", "list", "top"}))) {
    if (!contains(before, cell)) {
      fresh.push_back(cell);
    }
  }
  if (fresh.size() == 1) {
    (void)run(ctx, {"load", fresh.front()});
  } else if (!fresh.empty()) {
    detail::note(ctx, codes::kPromptNote, "Top-level cells read from GDS file: " + joined(fresh));
  }
}

void load_cif(CommandContext& ctx, const std::string& file) {
  (void)run(ctx, {"cellname", "create", "cif_temp"});
  (void)run(ctx, {"load", "cif_temp"});
  (void)run(ctx, {"cif", "read", file_root(file)});
  const std::vector<std::string> children =
      detail::words_of(detail::run_value(ctx, {"cellname", "list", "children", "cif_temp"}));
  (void)run(ctx, {"load", children.empty() ? std::string{} : children.front()});
  (void)run(ctx, {"cellname", "delete", "cif_temp", "-noprompt"});
  if (children.size() > 1) {
    detail::note(ctx, codes::kPromptNote, "Cells read from GDS file: " + joined(children));
  }
}

void load_getcell(CommandContext& ctx, const std::string& file) {
  const std::string dir = file_dirname(file);
  if (!contains(detail::words_of(detail::run_value(ctx, {"path", "search"})), dir)) {
    (void)run(ctx, {"path", "search", "+" + dir});
  }
  (void)run(ctx, {"getcell", file_tail(file)});
  // wrapper.tcl's `set $Layoutfilename "./..."` sets another variable, so a
  // relative name stays as chosen.
  const std::size_t slash = file.rfind('/');
  if (slash != std::string::npos && slash > 0) {
    (void)run(ctx, {"path", "cell", "+" + file.substr(0, slash + 1)});
  }
}

void save_unnamed(CommandContext& ctx, platform::FilePicker* picker) {
  if (!contains(detail::words_of(detail::run_value(ctx, {"cellname", "list", "window"})), "(UNNAMED)")) {
    return;
  }
  std::string file = pick(picker, FilePickerRequest::Mode::save, kMagicTypes, "Save cell (UNNAMED) as:");
  if (file.empty()) {
    return;
  }
  const std::string dir = file_dirname(file);
  const platform::Path cwd = ctx.editor()->cell_session.file_host().current_directory();
  if (dir == cwd.generic_string()) {
    file = file_tail(file);
  } else {
    (void)run(ctx, {"path", "cell", "+" + dir});
  }
  (void)run(ctx, {"save", file});
}

}  // namespace

CommandResult prompt_load(CommandContext& ctx, platform::FilePicker* picker, std::string_view type) {
  if (ctx.editor() == nullptr) {
    return detail::no_session(ctx);
  }
  const bool cif = type == "cif";
  const bool gds = type == "gds";
  if (!cif && !gds && type != "magic" && type != "getcell") {
    return CommandResult::success();
  }
  const std::string file =
      pick(picker, FilePickerRequest::Mode::open, cif ? kCifTypes : gds ? kGdsTypes : kMagicTypes);
  if (file.empty()) {
    return CommandResult::success();
  }
  if (cif) {
    load_cif(ctx, file);
  } else if (gds) {
    load_gds(ctx, file);
  } else if (type == "magic") {
    (void)run(ctx, {"load", file_root(file)});
  } else {
    load_getcell(ctx, file);
  }
  return CommandResult::success();
}

CommandResult prompt_save(CommandContext& ctx, platform::FilePicker* picker, std::string_view type) {
  if (ctx.editor() == nullptr) {
    return detail::no_session(ctx);
  }
  if (type == "cif" || type == "gds") {
    const std::string file = pick(picker, FilePickerRequest::Mode::save, type == "cif" ? kCifTypes : kGdsTypes);
    if (!file.empty()) {
      (void)run(ctx, {std::string{type}, "write", file});
    }
    return CommandResult::success();
  }
  if (type != "magic" && type != "force") {
    return CommandResult::success();
  }
  save_unnamed(ctx, picker);
  (void)run(ctx, type == "force" ? std::vector<std::string>{"writeall", "force"} : std::vector<std::string>{"writeall"});
  return CommandResult::success();
}

CommandResult save_all_and_quit(CommandContext& ctx, platform::FilePicker* picker) {
  CommandResult saved = prompt_save(ctx, picker, "force");
  if (!saved.ok()) {
    return saved;
  }
  const std::vector<std::string> modified = detail::words_of(detail::run_value(ctx, {"cellname", "list", "modified"}));
  return ui::detail::run_forwarded(ctx, modified.empty() ? std::vector<std::string>{"quit", "-noprompt"}
                                                         : std::vector<std::string>{"quit"});
}

std::string prompt_dialog_initial(CommandContext& ctx, std::string_view type) {
  return type == "save" ? run(ctx, {"cellname", "list", "window"}) : std::string{"(UNNAMED)"};
}

std::vector<std::string> prompt_dialog_command(std::string_view type, std::string_view name) {
  return {type == "save" ? "save" : "load", std::string{name}};
}

}  // namespace hefesto::gui
