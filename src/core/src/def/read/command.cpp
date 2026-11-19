// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `def read` (def/read/command.hpp; capability def-read "def read
// command", "Command context", "DEF file lookup").
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/lefCmd.c:222-273 (CmdLef's
// LEF_READ for DEF), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: DEFR (design D2, D14).

#include "hefesto/def/read/command.hpp"

#include <memory>
#include <string>
#include <vector>

#include "cell/io_session.hpp"
#include "def/read/reader.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/lef/command/command.hpp"
#include "hefesto/mask/gen/progress.hpp"

namespace hefesto::def {

namespace {

using cmd::CommandResult;

// The `def` option table, as `def help` prints it (wrongNumArgs): one
// block, each entry on its own line as TxPrintf("    %s %s\n") prints it.
CommandResult print_options(cmd::CommandContext& ctx, const std::string& command) {
  std::string text = "The \"" + command + "\" options are:\n";
  for (const std::string_view entry : lef::def_options()) {
    text += "    " + command + " " + std::string{entry} + "\n";
  }
  text.pop_back();
  ctx.emit(diag::make(detail::kReport, std::move(text)));
  return CommandResult::success();
}

// A command of the layout client run from this one (DBWloadWindow through
// `load`), its messages passed on.
void run_nested(cmd::CommandContext& ctx, std::vector<std::string> words) {
  const cmd::CommandLookup found = ctx.registry().resolve(words.front(), cmd::kLayoutClient);
  if (found.status != cmd::LookupStatus::found) {
    return;
  }
  const cmd::Invocation invocation{found.spec->name, {words.begin() + 1, words.end()}, cmd::Origin::script, {}};
  const CommandResult result = found.spec->handler(ctx, invocation);
  for (const diag::Diagnostic& d : result.diagnostics) {
    ctx.emit(d);
  }
}

// The read through the session's progress host (the desktop pumps frames
// from its reports and may cancel); without one the read runs inline.
CommandResult hosted_read(editor::EditorState& editor, const platform::Path& path, DefReadOptions options) {
  const mask::ProgressHost& host = editor.mask.read_host;
  if (!host) {
    return read(editor, path, options);
  }
  return host(path.filename().string(), [&editor, &path, &options](const mask::Progress* p) {
    options.progress = p;
    return read(editor, path, options);
  });
}

}  // namespace

DefReadOptions parse_read_arguments(std::span<const std::string> after_name) {
  DefReadOptions options;
  if (after_name.empty()) {
    return options;
  }
  for (const std::string& arg : after_name) {
    if (!arg.starts_with('-')) {
      continue;
    }
    if (arg.starts_with("-anno")) {
      options.annotate = true;
    } else if (arg.starts_with("-label")) {
      options.labels = true;
    } else if (arg.starts_with("-noblock")) {
      options.no_blockage = true;
    }
    // -import is accepted and has no effect for DEF.
  }
  options.labels = options.labels || options.annotate;
  return options;
}

CommandResult read_command(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  if (invocation.args.size() < 2) {
    return print_options(ctx, invocation.name);
  }
  auto editor = cmd::require_editor(ctx);
  if (!editor) {
    return CommandResult::failure(editor.error());
  }
  editor::EditorState& ed = *editor.value();
  const std::string& name = invocation.args[1];
  DefReadOptions options =
      parse_read_arguments(std::span<const std::string>{invocation.args}.subspan(2));
  options.typed_name = name;
  options.files = &ctx.services().files;
  cell::LookupContext lookup{ctx.services().files, ed.search_paths, ed.path_variables, ctx.sink()};
  const auto found = find_def_file(lookup, name);
  if (!found) {
    const std::string tried = name.find('.') == std::string::npos ? name + ".def" : name;
    return CommandResult::failure(
        diag::make(detail::kCannotOpen, "Cannot open input file " + tried + " (No such file or directory)."));
  }
  auto session = std::make_shared<cell::detail::IoSession>(ctx, ed);
  options.read_master = [session](db::CellDef& def) {
    cell::ReadOptions read_options;
    read_options.ignore_tech = true;  // DBCellRead(def, TRUE, TRUE, NULL)
    return cell::read_cell(session->io, def, read_options).ok() && def.flags().available;
  };
  // The read's first line is printed before the COMPONENTS masters are read
  // from disk, whose messages go straight to the console (DefRead prints it
  // before parsing); def::read's copy of it is dropped below.
  const std::string reading = "Reading DEF data from file " + found->path.string() + ".";
  ctx.emit(diag::make(detail::kReport, reading));
  CommandResult result = hosted_read(ed, found->path, options);
  session->process_mismatches();
  if (!result.diagnostics.empty() && result.diagnostics.front().text == reading) {
    result.diagnostics.erase(result.diagnostics.begin());
  }
  const std::string* design = std::get_if<std::string>(&result.value.data());
  if (result.ok() && design != nullptr && !design->empty() && ed.windows.size() == 1) {
    for (const diag::Diagnostic& d : result.diagnostics) {
      ctx.emit(d);
    }
    result.diagnostics.clear();
    run_nested(ctx, {"load", *design});
  }
  // CmdLef sets no Tcl result for `def read`.
  result.value = cmd::Value{};
  return result;
}

}  // namespace hefesto::def
