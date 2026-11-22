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

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "cell/io_session.hpp"
#include "def/read/reader.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/sink.hpp"
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

// DefRead's DBWloadWindow(w, design, DBW_LOAD_IGNORE_TECH): the design
// cell into the window, by name and not through the `load` command's
// argument parser (a hostile `DESIGN -nowindow ;` is a cell name, not an
// option).
// `pass_on` prints what the session printed so far (it prints into the
// read's master sink), so its lines keep their place.
void load_design(cmd::CommandContext& ctx, cell::detail::IoSession& session, const std::string& design,
                 const std::function<void()>& pass_on) {
  cell::ReadOptions options;
  options.ignore_tech = true;
  session.ed.windows.set_cell_reader(&cell::detail::read_for_expansion);
  const CommandResult loaded = cell::detail::load_into_window(session, design, options);
  pass_on();
  for (const diag::Diagnostic& d : loaded.diagnostics) {
    ctx.emit(d);
  }
  session.process_mismatches();
  pass_on();
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

// The file name the missing-file message gives: `name` with `.def` added
// when its last component has no extension, as find_def_file looks it up
// (a dot in a directory name, as in `./top` or `dir.v1/top`, is no
// extension).
std::string tried_name(const std::string& name) {
  const std::size_t slash = name.find_last_of('/');
  const std::size_t base = slash == std::string::npos ? 0 : slash + 1;
  return name.find('.', base) == std::string::npos ? name + ".def" : name;
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
    return CommandResult::failure(diag::make(
        detail::kCannotOpen, "Cannot open input file " + tried_name(name) + " (No such file or directory)."));
  }
  // The masters' reads (COMPONENTS, and the -annotate terminal lookup) print
  // into `masters`; the reader moves those lines into its own after each
  // read, so they come out where Magic prints them: after "Reading DEF data
  // from file ..." and before the section's count (LEFDEF-211, 232). What
  // the session prints after the read (mismatches, the design load) is
  // passed on to the command's sink.
  diag::CollectingSink masters;
  cmd::CommandContext master_ctx(ctx.services(), ctx.project(), ctx.session(), masters, ctx.registry(), &ed);
  auto session = std::make_shared<cell::detail::IoSession>(master_ctx, ed);
  options.read_master = [session](db::CellDef& def) {
    cell::ReadOptions read_options;
    read_options.ignore_tech = true;  // DBCellRead(def, TRUE, TRUE, NULL)
    return cell::read_cell(session->io, def, read_options).ok() && def.flags().available;
  };
  options.master_messages = &masters;
  const std::function<void()> pass_on = [&ctx, &masters] {
    for (const diag::Diagnostic& d : masters.take()) {
      ctx.emit(d);
    }
  };
  CommandResult result = hosted_read(ed, found->path, options);
  session->process_mismatches();
  pass_on();
  const std::string* design = std::get_if<std::string>(&result.value.data());
  if (result.ok() && design != nullptr && !design->empty() && ed.windows.size() == 1) {
    for (const diag::Diagnostic& d : result.diagnostics) {
      ctx.emit(d);
    }
    result.diagnostics.clear();
    load_design(ctx, *session, *design, pass_on);
  }
  // CmdLef sets no Tcl result for `def read`.
  result.value = cmd::Value{};
  return result;
}

}  // namespace hefesto::def
