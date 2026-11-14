// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// gds::read and the file lookup of `gds read` (capability gds-read "Read
// command and file lookup", "Read diagnostics", "Host mechanisms used by
// the reader"; openspec/changes/m5-mask-gds-cif/design.md D4, D11). Ported
// from Magic 8.3.694 commands/CmdCD.c (CALMA_READ's extension loop) and
// calma/CalmaRead.c (CalmaReadFile's set-up: undo disabled, the redirect
// file); Magic is (c) the Regents of the University of California,
// distributed under its permissive license (THIRD_PARTY_NOTICES.md).
//
// The read holds the exclusive database lock (re-entrant for the command
// thread) and detaches the undo recorder of the cell library for its
// duration (UndoDisable). The `gds warning redirect <file>` text is written
// to the file at the end of the read.
//
// Owner: GDSR (openspec/changes/m5-mask-gds-cif/design.md D2).

#include "hefesto/gds/read/read.hpp"

#include <array>
#include <utility>

#include "gds/read/reader.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/stream/io.hpp"

namespace hefesto::gds {

namespace {

constexpr std::array<std::string_view, 5> kExtensions{".gds", ".gds.gz", ".gds2", ".strm", ""};

/// Detaches the cell library's change recorder for the read (UndoDisable).
class UndoDisabled {
 public:
  explicit UndoDisabled(db::CellLibrary& cells) : cells_(cells), recorder_(cells.recorder()) {
    cells_.set_recorder(nullptr);
  }
  ~UndoDisabled() { cells_.set_recorder(recorder_); }
  UndoDisabled(const UndoDisabled&) = delete;
  UndoDisabled& operator=(const UndoDisabled&) = delete;

 private:
  db::CellLibrary& cells_;
  db::ChangeRecorder* recorder_;
};

}  // namespace

std::optional<cell::FoundFile> find_gds_file(cell::LookupContext& ctx, std::string_view name) {
  for (const std::string_view extension : kExtensions) {
    const std::string candidate = std::string{name} + std::string{extension};
    auto found = cell::search_file(ctx, candidate, ctx.paths.path, nullptr);
    // PaZOpen adds no ".gz": a compressed counterpart is found only through
    // the ".gds.gz" extension.
    if (found && found->path.filename() == platform::Path{candidate}.filename()) {
      return found;
    }
  }
  return std::nullopt;
}

cmd::CommandResult read(editor::EditorState& editor, const platform::Path& path, const GdsReadOptions& options,
                        const mask::Progress* progress) {
  platform::LocalFileSystem local;
  platform::FileSystem& fs = options.files != nullptr ? *options.files : local;
  auto source = open_source(fs, path);
  if (!source.ok()) {
    cmd::CommandResult result = cmd::CommandResult::failure(
        diag::make("GDS-READ-010", "Cannot open " + path.string() + " to read GDS-II stream input."));
    result.diagnostics.push_back(source.error());
    return result;
  }
  GdsReadOptions effective = options;
  if (effective.gds_file.empty()) {
    effective.gds_file = path.string();
  }
  effective.files = &fs;
  return read(editor, *source.value(), effective, progress);
}

cmd::CommandResult read(editor::EditorState& editor, ByteSource& source, const GdsReadOptions& options,
                        const mask::Progress* progress) {
  const db::DatabaseLock::Exclusive lock(editor.database_lock);
  const UndoDisabled no_undo(editor.cells);
  read_detail::Reader reader(editor, source, options, progress);
  reader.run();
  cmd::CommandResult result;
  result.diagnostics = std::move(reader.messages().printed());
  if (reader.no_style()) {
    result.status = cmd::Status::failed;
    return result;
  }
  const mask::WarningSettings& warnings = options.warnings;
  if (warnings.level == mask::WarningLevel::redirect && !warnings.redirect_file.empty()) {
    platform::LocalFileSystem local;
    platform::FileSystem& fs = options.files != nullptr ? *options.files : local;
    if (auto written = fs.write_atomic(warnings.redirect_file, reader.messages().redirected()); !written.ok()) {
      result.diagnostics.push_back(written.error());
    }
  }
  if (reader.library_cell()) {
    result.value = cmd::Value{*reader.library_cell()};
  }
  return result;
}

}  // namespace hefesto::gds
