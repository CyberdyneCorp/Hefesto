// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cell loading and saving: DBCellRead/dbReadOpen (lookup, read-only
// detection, locking, recorded path), DBWloadWindow's name handling,
// expansion of unavailable descendants and DBCellWrite (refusal, write
// through a temporary file, verification, lock move). Capabilities
// cell-library-resolution, cell-io-safety, cell-timestamps. Ported from
// Magic 8.3.694 database/DBio.c, dbwind/DBWprocs.c and database/DBexpand.c;
// Magic is (c) the Regents of the University of California, distributed
// under its permissive license (THIRD_PARTY_NOTICES.md).

#include <algorithm>
#include <array>
#include <set>

#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/cell/io_safety.hpp"
#include "hefesto/cell/mag_io.hpp"
#include "hefesto/db/hierarchy.hpp"

namespace hefesto::cell {

namespace {

constexpr std::string_view kMag = ".mag";
/// A declined cell cannot be written (script/pdk/codes.inc).
constexpr std::string_view kDeclinedWrite = "SCRIPT-TRUST-005";

void emit(const CellIoContext& ctx, std::string_view code, std::string text) {
  ctx.sink.emit(diag::make(code, std::move(text)));
}

/// "Cell file path bookkeeping": the recorded path of an opened file
/// (a trailing `.mag` or, Q-CELL-16, `.mag.gz` removed).
std::string recorded_path_of(std::string name) {
  for (const std::string_view suffix : {std::string_view{".mag.gz"}, kMag}) {
    if (name.size() > suffix.size() && name.ends_with(suffix)) {
      name.resize(name.size() - suffix.size());
      return name;
    }
  }
  return name;
}

// dbReadOpen's message when no file opens; `with_errno` adds the strerror
// line DBCellRead prints when its caller asked for the error (a top-level
// load). The lookup fails with ENOENT. (Magic ref: database/DBio.c:1581-1604)
void report_missing(CellIoContext& ctx, db::CellDef& def, const ReadOptions& options) {
  if (def.flags().not_found) {
    return;  // reported once per definition
  }
  const bool print = options.prints(ReadOptions::Verbosity::errors);  // DB_VERBOSE_ERR
  if (!def.file_path().empty()) {
    if (print) {
      emit(ctx, codes::kNotFound, "File " + def.file_path().generic_string() + " couldn't be read");
    }
  } else {
    if (print) {
      emit(ctx, codes::kNotFound, "Cell " + def.name() + " couldn't be read");
    }
    def.set_file_path(def.name() + std::string{kMag});
  }
  if (options.report_errno && print) {
    emit(ctx, codes::kNotFound, "No such file or directory");
  }
  def.flags().not_found = true;
}

// "Recording the result" at full verbosity (Magic ref: database/DBio.c:1642-1654).
void report_read_from(CellIoContext& ctx, const db::CellDef& def, std::string_view recorded,
                      const ReadOptions& options) {
  if (!options.prints(ReadOptions::Verbosity::all)) {
    return;
  }
  const std::size_t slash = recorded.rfind('/');
  if (slash == std::string_view::npos) {
    emit(ctx, codes::kReadNote, "Cell " + def.name() + " read from current working directory");
  } else {
    emit(ctx, codes::kReadNote, "Cell " + def.name() + " read from path " + std::string{recorded.substr(0, slash)});
  }
}

/// "Read-only cell detection" and "Advisory locking between sessions".
/// `file` is the file opened; `named` the candidate name, which Magic tests
/// for writability (file_is_not_writeable(filename)): for a compressed
/// counterpart that is the uncompressed name, so `inv.mag.gz` alone reads
/// as not writeable (Magic ref: database/DBio.c:1607-1625).
void detect_read_only(CellIoContext& ctx, db::CellDef& def, const platform::Path& file, const platform::Path& named,
                      const ReadOptions& options) {
  std::string owner;
  bool locked = false;
  if (ctx.locks != nullptr && !ctx.locks->holds(file)) {
    const auto got = ctx.locks->acquire(file, &owner);
    locked = got.ok() && !got.value() && !owner.empty();
  }
  if (locked) {
    def.flags().no_edit = true;
    emit(ctx, codes::kReadOnly,
         "File " + file.generic_string() + " is already locked by " + owner + ".  Opening read-only.");
    return;
  }
  const bool writable = ctx.fs.exists(named) && ctx.host.writable(named);
  def.flags().no_edit = !writable;
  if (!writable && options.prints(ReadOptions::Verbosity::warnings)) {
    emit(ctx, codes::kReadOnly,
         "Warning: cell <" + def.name() + "> from file " + named.generic_string() + " is not writeable");
  }
}

diag::Outcome<std::string> read_text(CellIoContext& ctx, const platform::Path& file) {
  auto data = ctx.fs.read(file);
  if (!data) {
    return data.error();
  }
  return maybe_decompress(std::move(data).value());
}

/// "Path conflicts on explicit load and placement" (load): false when the
/// load must fail.
bool check_load_conflict(CellIoContext& ctx, db::CellDef& def, std::string_view name) {
  LookupContext lookup{ctx.fs, ctx.paths, ctx.vars, ctx.sink};
  const auto found = search_file(lookup, std::string{name}.ends_with(kMag) ? std::string{name}
                                                                          : std::string{name} + std::string{kMag},
                                 ctx.paths.path, &ctx.paths.library);
  std::string existing = def.file_path().generic_string();
  if (!existing.ends_with(kMag)) {
    existing += kMag;
  }
  if (found) {
    const auto same = ctx.host.same_file(found->path, existing);
    if (same && *same) {
      return true;
    }
    if (!same && !def.flags().available) {
      emit(ctx, codes::kConflictingLocation,
           "Warning:  Existing cell " + def.name() + " points to invalid path \"" + def.file_path().generic_string() +
               "\".  Cell was found at location \"" + found->path.generic_string() +
               "\" and this location will be used.");
      def.set_file_path({});
      return true;
    }
  }
  emit(ctx, codes::kConflictingLocation,
       "File \"" + std::string{name} + "\":  The cell was already read and points to conflicting location \"" +
           def.file_path().generic_string() + "\".");
  return false;
}

/// DBExpandAll over unavailable descendants (`seen` guards cycles).
void expand_from(CellIoContext& ctx, db::CellDef& def, std::set<const db::CellDef*>& seen) {
  if (!seen.insert(&def).second) {
    return;
  }
  std::vector<db::CellDef*> children;
  for (const auto& u : def.uses()) {
    children.push_back(u->def);
  }
  for (db::CellDef* child : children) {
    if (!child->flags().available) {
      if (def.flags().dereference) {
        child->flags().dereference = true;
      }
      ReadOptions opts;
      opts.ignore_tech = true;
      if (!read_cell(ctx, *child, opts).ok()) {
        emit(ctx, codes::kNotFound, "Cell " + child->name() + " is unavailable.  It could not be expanded.");
        continue;
      }
    }
    expand_from(ctx, *child, seen);
  }
}

// --- saving -------------------------------------------------------------------

/// "Choosing the file name": `.mag` appended, a leading `~`/`$` expanded.
std::string target_name(const CellIoContext& ctx, const db::CellDef& def, std::string_view path) {
  std::string name = path.empty() ? def.file_path().generic_string() : std::string{path};
  if (name.empty()) {
    name = def.name();
  }
  if (!name.ends_with(kMag)) {
    name += kMag;
  }
  if (name.front() == '~' || name.front() == '$') {
    if (auto expanded = expand_entry(name, ctx.vars)) {
      name = *expanded;
    }
  }
  return name;
}

diag::Status check_refusal(CellIoContext& ctx, const db::CellDef& def, const std::string& file) {
  if (const auto declined = ctx.declined != nullptr ? ctx.declined->technology_of(def) : std::nullopt) {
    // hefesto-security "Consent before implicit script execution": the cell
    // holds only what `minimum` could read; writing it would drop the PDK's layers.
    return diag::make(kDeclinedWrite, "Cell " + def.name() + " was opened read-only because the " + *declined +
                                          " scripts were not run; it cannot be written.");
  }
  if (def.flags().no_edit) {
    const bool locked = ctx.locks != nullptr && !ctx.locks->holds(file) && ctx.fs.exists(file);
    return diag::make(codes::kWriteRefused,
                      "File " + file + (locked ? " is locked by another user and cannot be written"
                                               : " is read_only and cannot be written"));
  }
  if (ctx.fs.exists(file) && !ctx.host.writable(file)) {
    return diag::make(codes::kWriteRefused, "File " + file + ": Permission denied");
  }
  return diag::Done{};
}

diag::Status check_names(const db::CellDef& def) {
  if (auto ok = check_writable_name("cell name", def.name()); !ok) {
    return ok;
  }
  for (const auto& u : def.uses()) {
    if (auto ok = check_writable_name("instance id", u->id); !ok) {
      return ok;
    }
  }
  return diag::Done{};
}

void warn_renamed_children(const CellIoContext& ctx, const db::CellDef& def) {
  if (ctx.renamed_duplicates == nullptr) {
    return;
  }
  for (const auto& u : def.uses()) {
    if (ctx.renamed_duplicates->contains(u->def)) {
      emit(ctx, codes::kRenamedReference,
           "Cell " + def.name() + " references cell " + u->def->name() +
               ", which was renamed after a duplicate-cell conflict.");
    }
  }
}

/// The write itself: temporary file and rename (platform write_atomic),
/// existing permission bits kept, size verified.
diag::Status write_file(CellIoContext& ctx, const std::string& file, const std::string& text) {
  const auto mode = ctx.fs.exists(file) ? ctx.host.permissions(file) : std::nullopt;
  if (auto written = ctx.fs.write_atomic(file, text); !written) {
    emit(ctx, codes::kWriteFailed, "Warning: I/O error in writing file " + file);
    return diag::make(codes::kWriteFailed, "Could not write file.  Cell not written.");
  }
  if (mode) {
    (void)ctx.host.set_permissions(file, *mode);
  }
  const auto size = ctx.fs.size(file);
  if (!size) {
    return diag::make(codes::kWriteFailed, "Warning: Cannot open file \"" + file + "\" for writing!");
  }
  if (size.value() != text.size()) {
    return diag::make(codes::kWriteFailed, "Warning: I/O error in writing file \"" + file + "\"");
  }
  return diag::Done{};
}

diag::Status save_script(CellIoContext& ctx, const db::CellDef& def, const std::string& file) {
  if (auto written = ctx.fs.write_atomic(file, write_command_script(ctx, def)); !written) {
    return diag::make(codes::kWriteFailed, "Cannot write file " + file + ": " + written.error().text);
  }
  emit(ctx, codes::kSaved,
       "Saved cell " + def.name() + " as a sequence of magic commands (file " + file + ").");
  return diag::Done{};
}

}  // namespace

std::string load_name(std::string_view name) {
  if (name.starts_with("./")) {
    name.remove_prefix(2);
  }
  const std::size_t slash = name.rfind('/');
  const std::size_t tail = slash == std::string_view::npos ? 0 : slash + 1;
  for (const std::string_view suffix : {std::string_view{".gz"}, kMag}) {
    const std::size_t dot = name.rfind('.');
    if (dot != std::string_view::npos && dot >= tail && name.substr(dot) == suffix) {
      name = name.substr(0, dot);
    }
  }
  return std::string{name};
}

std::string load_definition_name(std::string_view name) {
  std::string out = load_name(name);
  if (const std::size_t slash = out.rfind('/'); slash != std::string::npos) {
    out.erase(0, slash + 1);
  }
  return out;
}

diag::Status read_cell(CellIoContext& ctx, db::CellDef& def, const ReadOptions& options) {
  if (def.flags().available) {
    return diag::Done{};
  }
  LookupContext lookup{ctx.fs, ctx.paths, ctx.vars, ctx.sink};
  const bool deref = options.lookup_dereferenced && def.flags().dereference;
  const auto found = find_cell_file(lookup, def.name(), def.file_path().generic_string(), deref);
  if (!found) {
    report_missing(ctx, def, options);
    return diag::make(codes::kNotFound, "Cell " + def.name() + " couldn't be read");
  }
  auto text = read_text(ctx, found->path);
  if (!text) {
    report_missing(ctx, def, options);
    return text.error();
  }
  detect_read_only(ctx, def, found->path, found->recorded, options);
  report_read_from(ctx, def, found->recorded, options);
  def.flags().not_found = false;
  def.set_file_path(recorded_path_of(found->recorded));
  return read_mag(ctx, def, text.value(), options);  // a format error is reported by read_mag
}

void expand_all(CellIoContext& ctx, db::CellDef& def) {
  std::set<const db::CellDef*> seen;
  expand_from(ctx, def, seen);
}

namespace {

// The outcome of reading the cell named on a load. DBCellRead failing on a
// file that opened (technology mismatch, format error) leaves errno at
// ENOENT from the probe of the missing compressed candidate, so
// DBWloadWindow reports a new cell as for a missing file and keeps what was
// read (Magic ref: dbwind/DBWprocs.c:491-498).
// "Creating new cell" is not printed for `-quiet` or `-silent` (beQuiet).
diag::Status settle_load(CellIoContext& ctx, db::CellDef& def, const diag::Status& read, bool was_available,
                         const ReadOptions& options) {
  const bool quiet = !options.prints(ReadOptions::Verbosity::warnings);
  if (read.ok()) {
    if (!was_available) {
      db::recompute_bbox(def, ctx.magic_compat);
    }
    return diag::Done{};
  }
  const std::string_view code = read.error().code;
  if (def.flags().available) {
    if (!quiet && (code == codes::kTechMismatch || code == codes::kFormatError)) {
      emit(ctx, codes::kReadNote, "Creating new cell");
    }
    return diag::Done{};
  }
  if (code != codes::kNotFound && code != codes::kTechMismatch) {
    return read.error();
  }
  if (!quiet) {
    emit(ctx, codes::kReadNote, "Creating new cell");
  }
  def.flags().not_found = false;
  def.flags().available = true;
  return diag::Done{};
}

}  // namespace

diag::Outcome<db::CellDef*> load_cell(CellIoContext& ctx, std::string_view name, const ReadOptions& options) {
  const std::string requested = load_name(name);
  const std::string root = load_definition_name(requested);
  db::CellDef* def = ctx.library.find(root);
  if (def != nullptr && !def->file_path().empty() && requested != root && !check_load_conflict(ctx, *def, requested)) {
    return diag::make(codes::kConflictingLocation, "cell " + root + " points to a conflicting location");
  }
  if (def == nullptr) {
    auto created = ctx.library.create(root, ctx.tech->plane_count());
    if (!created) {
      return created.error();
    }
    def = created.value();
  }
  if (options.dereference) {
    def->flags().dereference = true;
  }
  if (def->file_path().empty() && requested != root) {
    def->set_file_path(requested);
  }
  ReadOptions top = options;
  top.lookup_dereferenced = false;  // the named cell itself is never dereferenced
  top.report_errno = true;
  const bool was_available = def->flags().available;
  if (auto settled = settle_load(ctx, *def, read_cell(ctx, *def, top), was_available, top); !settled) {
    return settled.error();
  }
  // DBWloadWindow without DBW_LOAD_EXPAND: descendants stay unread until
  // something needs them (getcell, flatten, timestamp mismatches).
  // (Magic ref: commands/CmdLQ.c:500-507, dbwind/DBWprocs.c:540-552)
  return def;
}

diag::Status check_writable_name(std::string_view what, std::string_view name) {
  const bool bad = std::any_of(name.begin(), name.end(), [](char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
  });
  if (bad) {
    return diag::make(codes::kInvalidName, "The " + std::string{what} + " \"" + std::string{name} +
                                               "\" contains whitespace and cannot be written to a cell file.");
  }
  return diag::Done{};
}

void update_save_stamps(CellIoContext& ctx, db::CellDef* only, std::int64_t now) {
  const std::array<db::CellDef*, 1> one{only};
  update_save_stamps(ctx, std::span<db::CellDef* const>{one.data(), one.size()}, now);
}

void update_save_stamps(CellIoContext& ctx, std::span<db::CellDef* const> cells, std::int64_t now) {
  diag::NullSink quiet;
  LookupContext lookup{ctx.fs, ctx.paths, ctx.vars, quiet};
  update_timestamps(ctx.library, cells, now, lookup);
}

diag::Status save_cell(CellIoContext& ctx, db::CellDef& def, std::string_view path) {
  if (path.size() > 4 && path.ends_with(".tcl")) {
    return save_script(ctx, def, std::string{path});
  }
  const std::string file = target_name(ctx, def, path);
  if (auto refused = check_refusal(ctx, def, file); !refused) {
    return refused;
  }
  if (auto names = check_names(def); !names) {
    return names;
  }
  update_save_stamps(ctx, &def, stamp_from_time(ctx.clock.wall_now()));
  warn_renamed_children(ctx, def);
  WriteOptions write;
  write.target_file = recorded_path_of(file);
  const std::string text = write_mag(ctx, def, write);
  const platform::Path old_file = def.file_path().generic_string() + std::string{kMag};
  if (auto written = write_file(ctx, file, text); !written) {
    return written;
  }
  if (ctx.locks != nullptr) {
    ctx.locks->release(old_file);
    (void)ctx.locks->acquire(file);
  }
  def.set_file_path(file.substr(0, file.size() - kMag.size()));
  db::CellFlags& f = def.flags();
  f.modified = false;
  f.child_bboxes_changed = false;
  f.child_timestamps_changed = false;
  return diag::Done{};
}

}  // namespace hefesto::cell
