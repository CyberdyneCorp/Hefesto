// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `.mag` cell file reader and writer (capability cell-file-format;
// reserved Hefesto property namespace per hefesto-mag-extensions;
// timestamps per cell-timestamps), cell loading through the search paths
// (cell-library-resolution) and saving through the I/O safety rules
// (cell-io-safety). Byte-identical round trip of Magic files is an M1 exit
// criterion.
// (Magic ref: database/DBio.c)
//
// Owner: CELL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <string_view>

#include "hefesto/cell/host.hpp"
#include "hefesto/cell/search_path.hpp"
#include "hefesto/cell/timestamps.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/display/font.hpp"
#include "hefesto/platform/clock.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::cell {

/// Properties whose names start with this prefix are Hefesto extensions
/// (hefesto-mag-extensions "Reserved property namespace"); their values
/// start with a schema version token `v<N>:`.
inline constexpr std::string_view kExtensionPrefix = "HEFESTO_";
/// The newest extension schema version this build understands.
inline constexpr int kExtensionVersion = 1;

class FileLockService;

/// Everything a cell read or write touches. Non-owning.
struct CellIoContext {
  platform::FileSystem& fs;
  platform::Clock& clock;
  db::CellLibrary& library;
  const tech::Technology* tech;  // never null; replaced by load_technology
  tech::GridScale& grid;  // may change on read ("Coordinate scaling on read")
  const SearchPaths& paths;
  const PathVariables& vars;
  diag::DiagnosticSink& sink;
  FileHost& host;
  TimestampLog& stamps;
  FileLockService* locks = nullptr;              // nullptr = locking disabled
  const display::FontRegistry* fonts = nullptr;  // nullptr = only the default font
  bool magic_compat = true;
  bool property_compat = true;  // `property compat` (layout-properties)
  /// Definitions renamed by a duplicate-cell conflict (Q-CELL-24); saving
  /// a parent that references one warns. nullptr = not tracked.
  std::set<const db::CellDef*>* renamed_duplicates = nullptr;
  /// Rescales the whole database by n/d (scalegrid step 6, including the
  /// box) when a finer file refines the grid; returns false when it cannot.
  std::function<bool(std::int64_t n, std::int64_t d)> rescale_database;
  /// What load_technology did for a mismatched file.
  struct TechnologySwitch {
    /// The now-current technology (kept alive by the session), or nullptr
    /// when nothing was loaded. The reader then uses it through `tech`.
    const tech::Technology* tech = nullptr;
    /// The technology's PDK startup script was declined (hefesto-security
    /// "Consent before implicit script execution"): the reader reads the
    /// cell anyway with the current (`minimum`) technology and marks it
    /// read-only (owner: PDKINT, m4-scripting design D7/D14).
    bool read_only = false;
  };
  /// Automatic technology discovery and loading for a mismatched file
  /// (cell-library-resolution "Automatic technology discovery").
  std::function<TechnologySwitch(std::string_view tech)> load_technology;
  /// Called at the end of every `.mag` read of `def` (dbCellReadDef's
  /// `done` and `badfile` paths); `format_error` is true on the badfile
  /// path, where Magic calls DRCCheckThis(def, TT_CHECKPAINT, NULL). The
  /// session uses it for DRC (m3-drc design D7). Empty = nothing.
  std::function<void(db::CellDef& def, bool format_error)> on_read;
};

struct ReadOptions {
  bool ignore_tech = false;  // `load -force`, subcell expansion, backups
  bool dereference = false;  // load_cell: the cell's descendants ignore recorded paths
  /// read_cell: the lookup follows the definition's dereference flag (false
  /// for the cell named on a load, which is never dereferenced itself).
  bool lookup_dereferenced = true;
  /// read_cell: a missing file also prints the system error text (DBCellRead
  /// called with an error pointer, as for the cell named on a load).
  bool report_errno = false;
  /// Magic's DBVerbose for the reading command: `load -quiet` prints only
  /// errors, `load -silent` nothing (generators' `load <cell> -silent`;
  /// owner PDKINT, m4-scripting design D14). Messages at a level above it
  /// are not printed. (Magic ref: commands/CmdLQ.c:436-441,500-508,
  /// database/DBio.c:1581-1660, dbwind/DBWprocs.c:312,494)
  enum class Verbosity : std::uint8_t { none, errors, warnings, all };
  Verbosity verbosity = Verbosity::all;

  /// True when a message of `level` is printed.
  [[nodiscard]] bool prints(Verbosity level) const noexcept { return verbosity >= level; }
};

/// Reads `.mag` text into the definition `def` (normally empty; a backup
/// read adds to an existing one). `origin` names the file for messages.
/// "Format errors and partial reads": a format error keeps what was read
/// and returns CELL-READ-001; the post-read steps run either way.
[[nodiscard]] diag::Status read_mag(CellIoContext& ctx, db::CellDef& def, std::string_view text,
                                    const ReadOptions& options = {});

/// Decompresses gzip data; plain data is returned unchanged.
[[nodiscard]] diag::Outcome<std::string> maybe_decompress(std::string data);

/// DBCellRead: reads `def` from its file when it is not yet available
/// (lookup order, read-only detection, locking, recorded path). The
/// definition stays an unavailable placeholder when no file is found.
[[nodiscard]] diag::Status read_cell(CellIoContext& ctx, db::CellDef& def, const ReadOptions& options = {});

/// Finds or creates the definition `name` (a trailing `.mag` stripped; a
/// name with a directory part sets the recorded path) and reads it if
/// needed. Descendants are not read (Magic's `load` does not expand); call
/// expand_all() when their contents are needed.
[[nodiscard]] diag::Outcome<db::CellDef*> load_cell(CellIoContext& ctx, std::string_view name,
                                                    const ReadOptions& options = {});

/// Reads every unavailable descendant of `def` (subcell expansion: the
/// technology is ignored, the dereference flag is inherited).
void expand_all(CellIoContext& ctx, db::CellDef& def);

struct WriteOptions {
  bool backup = false;  // backup/archive record: no out-of-date warning
  /// The file being written (DBCellWrite sets cd_file to it first, and use
  /// paths are relative to it); empty = the definition's recorded path.
  std::string target_file;
};

/// Renders `def` as `.mag` text ("Writer ..." requirements, "Natural
/// ordering of names"). `scale` is the session grid.
[[nodiscard]] std::string write_mag(const CellIoContext& ctx, const db::CellDef& def,
                                    const WriteOptions& options = {});

/// DBCellFindScale: the write-scale reducer r.
[[nodiscard]] std::int64_t find_write_scale(const db::CellDef& def, const tech::Technology& tech,
                                            const tech::GridScale& grid, bool magic_compat = true);

/// Renders `def` as the command-script save variant ("Command-script
/// content", with the Q-CELL-14 fixes).
[[nodiscard]] std::string write_command_script(const CellIoContext& ctx, const db::CellDef& def);

/// Saves `def` (DBCellWrite): to `path` when given (a `.tcl` name writes
/// the command script), else to its recorded file; refuses protected
/// cells, writes through a temporary file, verifies the size, moves the
/// lock and updates the recorded path and flags (cell-io-safety). Stamps
/// are updated first ("Generating new stamps").
[[nodiscard]] diag::Status save_cell(CellIoContext& ctx, db::CellDef& def, std::string_view path = {});

/// Refuses names that cannot be written back (Q-CELL-41): whitespace in
/// cell names, instance ids and paths.
[[nodiscard]] diag::Status check_writable_name(std::string_view what, std::string_view name);

}  // namespace hefesto::cell
