// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// OASIS write: the prepare-option mapping (INTEGRATION), the file naming
// and the three phases (oasis-write "Write command and output file
// naming", "Write result and error propagation";
// openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D4): gds::prepare_write
// (exclusive lock), oasis::emit (on the session's emit host when it has
// one) and gds::finish_write.
//
// Owner: OASW (design D2), except to_gds_write_options (INTEGRATION).

#include "hefesto/oasis/write/write.hpp"

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/oasis/write/emit.hpp"

namespace hefesto::oasis {

gds::GdsWriteOptions to_gds_write_options(const OasisSettings& settings,
                                          const stream::StreamReadSettings& /*stream_read*/) {
  gds::GdsWriteOptions options;
  gds::GdsSettings& gds = options.settings;
  gds.magscale = settings.magscale;
  gds.library = settings.library;
  gds.labels = settings.labels;
  gds.lower = settings.lower;
  gds.arrays = settings.arrays;
  gds.addendum = settings.addendum;
  gds.allow_undefined = settings.allow_undefined;
  gds.allow_abstract = settings.allow_abstract;
  gds.contacts = settings.contacts;
  gds.merge = settings.merge;
  gds.compress = 0;
  return options;
}

std::string output_file_name(std::string_view name) {
  std::string file{name};
  if (name.find('.') == std::string_view::npos) {
    file += ".oas";
  }
  return file;
}

std::string default_output_name(const db::CellDef& root) {
  const std::string& cell = root.name();
  const std::size_t slash = cell.rfind('/');
  return slash == std::string::npos ? cell : cell.substr(slash + 1);
}

cmd::CommandResult write(editor::EditorState& editor, db::CellDef& root, const platform::Path& path,
                         const OasisWriteOptions& options, const mask::Progress* progress) {
  gds::GdsWriteOptions prepare = options.prepare;
  if (progress != nullptr) {
    prepare.loader.cancel = progress->cancel;
    prepare.loader.tick = progress->tick;
  }
  diag::Outcome<gds::PreparedWrite> prepared = gds::prepare_write(editor, root, prepare);
  if (!prepared) {
    return cmd::CommandResult::failure(prepared.error());
  }
  const OasisSettings& settings = options.settings;
  const mask::ProgressHost& host = editor.mask.emit_host;
  cmd::CommandResult result =
      host ? host(path.filename().string(),
                  [&prepared, &settings, &path](const mask::Progress* hosted) {
                    return emit(prepared.value(), settings, path, hosted);
                  })
           : emit(prepared.value(), settings, path, progress);
  gds::finish_write(editor, prepared.value());
  return result;
}

}  // namespace hefesto::oasis
