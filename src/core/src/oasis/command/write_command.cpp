// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `oasis write <file>` and a bare `oasis` (capability oasis-command
// "Command name and forms"; oasis-write "Write command and output file
// naming"): the root cell of the current window is written to `<file>`, or
// to the root cell's name after its last `/` for a bare `oasis`, with
// `.oas` appended when the name has no `.` anywhere (the output is never
// gzip-wrapped, Q-OASIS-91). Any number of arguments other than one file
// name is an argument error and nothing is written. The write runs
// oasis::write with the session's `oasis` settings, through the desktop's
// write host (MaskSession::write_host, design D16) when there is one, as
// `gds write` does.
//
// Owner: OASCMD (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/write/subtree.hpp"
#include "hefesto/oasis/write/write.hpp"
#include "options.hpp"

namespace hefesto::oasis::command_detail {

namespace {

using cmd::CommandResult;

db::CellDef* window_root(const cu::Session& s) {
  const view::LayoutWindow* w = s.editor->windows.current();
  return w != nullptr ? w->root() : nullptr;
}

CommandResult write_oasis(cmd::CommandContext& ctx, const cu::Session& s, db::CellDef& root,
                          const std::string& name) {
  editor::EditorState& editor = *s.editor;
  OasisWriteOptions options;
  options.settings = editor.mask.oasis;
  options.prepare = to_gds_write_options(editor.mask.oasis, editor.mask.stream_read);
  options.prepare.typed_name = name;
  options.prepare.loader = gds::command_subtree_loader(ctx, editor);
  options.prepare.files = &ctx.services().files;
  options.prepare.time_zone = editor.mask.time_zone;
  const platform::Path path{oasis::output_file_name(name)};
  const mask::ProgressHost& host = editor.mask.write_host;
  if (!host) {
    return write(editor, root, path, options);
  }
  return host(path.filename().string(), [&editor, &root, &path, &options](const mask::Progress* progress) {
    return write(editor, root, path, options, progress);
  });
}

}  // namespace

CommandResult write_option(cmd::CommandContext& ctx, const cu::Session& s, const cmd::Invocation& invocation) {
  db::CellDef* root = window_root(s);
  if (root == nullptr) {
    return CommandResult::failure(diag::make(kNoWindow, "Point to a window first"));
  }
  if (invocation.args.empty()) {
    return write_oasis(ctx, s, *root, oasis::default_output_name(*root));
  }
  if (invocation.args.size() != 2) {
    return wrong_args(invocation);
  }
  return write_oasis(ctx, s, *root, invocation.args[1]);
}

}  // namespace hefesto::oasis::command_detail
