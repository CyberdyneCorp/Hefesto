// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `oasis read <file>` (capability oasis-command "Command name and forms";
// oasis-read "Read command and file lookup", "Top cell selection",
// "Read-only OASIS vendor views"): the file lookup (oasis::find_oasis_file:
// `.oas`, `.oas.gz`, `.oasis`, none), an empty edit cell when there is
// none (as `gds read`), oasis::read with the session's settings at the
// time of the call (the shared read settings, the `oasis` settings, the
// rescale switch and the read warnings) through the desktop's read host
// (MaskSession::read_host, design D16), then the top cell loaded into the
// window. OASIS_FILE is the name the file was found as with magic-compat
// ON and the absolute canonical path with it OFF, then substituted, as
// GDS_FILE is (Q-MASK-128, Q-MASK-129).
//
// Owner: OASCMD (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <filesystem>
#include <system_error>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/read/vendor.hpp"
#include "hefesto/oasis/read/find_file.hpp"
#include "hefesto/oasis/read/read.hpp"
#include "options.hpp"

namespace hefesto::oasis::command_detail {

namespace {

using cmd::CommandResult;

std::string oasis_file_property(const cu::Session& s, const platform::Path& found) {
  std::string path = found.generic_string();
  if (!s.magic_compat) {
    std::error_code ec;
    const std::filesystem::path absolute = std::filesystem::absolute(found, ec);
    const std::filesystem::path canonical = ec ? found : std::filesystem::weakly_canonical(absolute, ec);
    path = (ec ? absolute.lexically_normal() : canonical).generic_string();
  }
  return gds::gds_file_value(path, *s.editor);
}

CommandResult hosted_read(editor::EditorState& editor, const platform::Path& path, const OasisReadOptions& options) {
  const mask::ProgressHost& host = editor.mask.read_host;
  if (!host) {
    return read(editor, path, options);
  }
  return host(path.filename().string(),
              [&editor, &path, &options](const mask::Progress* p) { return read(editor, path, options, p); });
}

OasisReadOptions session_options(cmd::CommandContext& ctx, const editor::EditorState& editor) {
  OasisReadOptions options;
  options.stream_read = editor.mask.stream_read;
  options.settings = editor.mask.oasis;
  options.rescale_allowed = editor.mask.rescale_allowed;
  options.warnings = editor.mask.warnings;
  options.files = &ctx.services().files;
  return options;
}

}  // namespace

CommandResult run_nested(cmd::CommandContext& ctx, std::vector<std::string> words) {
  const cmd::CommandLookup found = ctx.registry().resolve(words.front(), cmd::kLayoutClient);
  if (found.status != cmd::LookupStatus::found) {
    return CommandResult::success();
  }
  const cmd::Invocation invocation{found.spec->name, {words.begin() + 1, words.end()}, cmd::Origin::script, {}};
  CommandResult result = found.spec->handler(ctx, invocation);
  for (const diag::Diagnostic& d : result.diagnostics) {
    ctx.emit(d);
  }
  result.diagnostics.clear();
  return result;
}

CommandResult read_option(cmd::CommandContext& ctx, const cu::Session& s, const cmd::Invocation& invocation) {
  if (invocation.args.size() != 2) {
    return wrong_args(invocation);
  }
  editor::EditorState& editor = *s.editor;
  const std::string& name = invocation.args[1];
  cell::LookupContext lookup{ctx.services().files, editor.search_paths, editor.path_variables, ctx.sink()};
  const std::optional<cell::FoundFile> found = find_oasis_file(lookup, name);
  if (!found) {
    return CommandResult::failure(diag::make(
        kCannotOpen, "Cannot open " + name + ".oas, " + name + ".oasis or " + name + " to read OASIS input."));
  }
  if (editor.edit_cell == nullptr) {
    (void)run_nested(ctx, {"load"});
  }
  OasisReadOptions options = session_options(ctx, editor);
  options.typed_name = name;
  options.oasis_file = oasis_file_property(s, found->path);
  CommandResult result = hosted_read(editor, found->path, options);
  const std::string* top = std::get_if<std::string>(&result.value.data());
  if (top != nullptr && editor.cells.find(*top) != nullptr) {
    const std::string cell = *top;
    for (const diag::Diagnostic& d : result.diagnostics) {
      ctx.emit(d);
    }
    result.diagnostics.clear();
    result.value = cmd::Value{};
    (void)run_nested(ctx, {"load", cell});
  }
  return result;
}

}  // namespace hefesto::oasis::command_detail
