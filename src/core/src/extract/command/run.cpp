// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The extraction run driver (extract/command/run.hpp; capability
// extract-commands "Incremental extraction", "Extract all, cell, parents
// and showparents", "Cell properties affecting extraction", "Output path
// and file location", "Progress and summary messages";
// openspec/changes/m7-extraction/design.md D12, Q-EXTRACT-6, Q-EXTRACT-27).
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtMain.c (ExtAll,
// ExtIncremental, ExtParents, ExtShowParents, ExtractOneCell, ExtUnique,
// extDefListFunc, extDefListFuncIncremental, extDefParentFunc,
// extTimestampMisMatch, extExtractStack) and extract/ExtCell.c (ExtCell),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: EXTCMD (design D3).

#include "hefesto/extract/command/run.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/cell/timestamps.hpp"
#include "hefesto/db/lock.hpp"
#include "hefesto/db/use_index.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/extract/device/devices.hpp"
#include "hefesto/extract/file/atomic_output.hpp"
#include "hefesto/extract/file/writer.hpp"
#include "hefesto/extract/hier/extract_cell.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/view/feedback.hpp"

namespace hefesto::extract {

namespace {

constexpr std::string_view kCodeCell = "EXTRACT-CMD-002";
constexpr std::string_view kCodeTotals = "EXTRACT-CMD-003";
constexpr std::string_view kCodeStart = "EXTRACT-CMD-004";
constexpr std::string_view kCodeOpen = "EXTRACT-CMD-005";
constexpr std::string_view kCodeWrite = "EXTRACT-CMD-006";
constexpr std::string_view kUnnamed = "(UNNAMED)";

/// Keeps the messages of one cell's preparation, replayed in Magic's order
/// when the cell is extracted.
class BufferSink final : public diag::DiagnosticSink {
 public:
  void emit(const diag::Diagnostic& d) override { items.push_back(d); }
  std::vector<diag::Diagnostic> items;
};

void emit(diag::DiagnosticSink* sink, std::string_view code, std::string text) {
  if (sink != nullptr) {
    sink->emit(diag::make(code, std::move(text)));
  }
}

std::string plural(std::int32_t n, std::string_view word) {
  return std::to_string(n) + " " + std::string{word} + (n != 1 ? "s" : "");
}

}  // namespace

/// One cell of the stack.
struct Job {
  db::CellDef* cell = nullptr;
  bool extract = false;  // ExtCell writes a file (otherwise the substrate only)
  bool is_top = false;
  std::optional<AtomicOutput> output;
  std::vector<diag::Diagnostic> messages;  // replayed before the cell is extracted
};

struct PreparedRun::State {
  RunMode mode = RunMode::incremental;
  diag::DiagnosticSink* sink = nullptr;
  ExtractContext ctx;
  std::vector<Job> jobs;
  UniqueRenamer renamer;
  std::vector<std::pair<db::CellDef*, db::Plane>> saved;  // the original substrate planes
  std::vector<std::pair<const db::CellDef*, CellFeedback>> feedback;
  RunReport report;
  bool totals = false;  // extExtractStack ran (it prints the totals)
};

PreparedRun::PreparedRun() : state_(std::make_unique<State>()) {}
PreparedRun::~PreparedRun() = default;
PreparedRun::PreparedRun(PreparedRun&&) noexcept = default;
PreparedRun& PreparedRun::operator=(PreparedRun&&) noexcept = default;

namespace {

using State = PreparedRun::State;

bool has_property(const db::CellDef& cell, std::string_view name) {
  return cell.properties().find(name) != nullptr;
}

// DBAllButSpaceAndDRCBits over the cell's own planes.
bool has_paint(const db::CellDef& cell) {
  tech::TypeMask all = tech::TypeMask::all();
  for (tech::TileType t = 0; t < tech::kFirstUserType; ++t) {
    all.clear_type(t);
  }
  for (std::size_t p = tech::kFirstUserPlane; p < cell.plane_count(); ++p) {
    const bool found = !cell.plane(static_cast<tech::PlaneId>(p)).enumerate(db::whole_plane(), all, [](const db::TileView&) {
      return false;
    });
    if (found) {
      return true;
    }
  }
  return false;
}

// The name ExtFileOpen opens for reading: ExtLocalPath, the cell file with
// its extension stripped, or the cell name.
std::string ext_base_name(const db::CellDef& cell, const std::optional<std::string>& local_path) {
  if (local_path) {
    return *local_path == "." ? cell.name() : *local_path + "/" + cell.name();
  }
  const std::filesystem::path file = cell.file_path();
  if (!file.empty()) {
    return file.has_extension() ? (file.parent_path() / file.stem()).string() : file.string();
  }
  return cell.name();
}

// extTimestampMisMatch.
bool timestamp_mismatch(cell::LookupContext& lookup, const db::CellDef& cell,
                        const std::optional<std::string>& local_path) {
  const std::optional<cell::FoundFile> found =
      cell::search_file(lookup, ext_base_name(cell, local_path) + ".ext", lookup.paths.path, &lookup.paths.library);
  if (!found) {
    return true;
  }
  const auto text = lookup.fs.read_at_most(found->path, 256);
  if (!text.ok()) {
    return true;
  }
  const std::string& t = text.value();
  const std::string first = t.substr(0, t.find('\n'));
  long long stamp = 0;
  if (std::sscanf(first.c_str(), "timestamp %lld", &stamp) != 1) {
    return true;
  }
  return static_cast<std::int32_t>(stamp) != static_cast<std::int32_t>(cell.timestamp());
}

/// extDefListFunc / extDefListFuncIncremental: the cells below and
/// including a root in extraction order (bottom-up), the "don't use"
/// cells dropped.
class DefList {
 public:
  DefList(const compat::CompatSettings& compat, std::unordered_set<const db::CellDef*>& dont_use)
      : compat_(compat), dont_use_(dont_use) {}

  /// With `stale` set, the incremental form: a cell whose `.ext` file is
  /// current goes to `current` (CDNOEXTRACT).
  std::function<bool(const db::CellDef&)> stale;
  std::unordered_set<const db::CellDef*> current;
  std::vector<db::CellDef*> order;

  void visit(db::CellDef& def) {
    if (def.flags().internal || dont_use_.contains(&def)) {
      return;
    }
    if (stale && !stale(def)) {
      current.insert(&def);
    }
    if (visited_.contains(&def)) {
      return;
    }
    for (db::CellUse* use : db::uses_in_search_order(def, compat_)) {
      if (use->def != nullptr) {
        visit(*use->def);
      }
    }
    visited_.insert(&def);
    if (!any_child_used(def) && !has_paint(def)) {
      dont_use_.insert(&def);
      return;
    }
    order.push_back(&def);
  }

 private:
  [[nodiscard]] bool any_child_used(const db::CellDef& def) const {
    for (const db::CellUse* use : db::uses_in_search_order(def, compat_)) {
      if (use->def != nullptr && !dont_use_.contains(use->def)) {
        return true;
      }
    }
    return false;
  }

  const compat::CompatSettings& compat_;
  std::unordered_set<const db::CellDef*>& dont_use_;
  std::unordered_set<const db::CellDef*> visited_;
};

// extDefParentFunc, returned in stack-pop order (the last pushed first).
void push_parents(db::CellDef& def, std::unordered_set<const db::CellDef*>& seen, std::vector<db::CellDef*>& pushed) {
  if (seen.contains(&def) || def.flags().internal) {
    return;
  }
  seen.insert(&def);
  pushed.push_back(&def);
  for (const db::CellUse* use : def.instances()) {
    if (use->parent != nullptr) {
      push_parents(*use->parent, seen, pushed);
    }
  }
}

compat::CompatSettings compat_of(const editor::EditorState& editor) {
  return editor.drc.compat_settings();
}

platform::FileSystem& files_of(const RunRequest& request) {
  static platform::LocalFileSystem local;
  return request.files != nullptr ? *request.files : local;
}

OutputLocation location_of(const editor::EditorState& editor, const RunRequest& request) {
  OutputLocation where;
  where.local_path = editor.extract.options.path;
  where.current_directory = request.current_directory.value_or(std::string{});
  return where;
}

// The context every cell of the run shares.
void init_context(State& st, editor::EditorState& editor, const RunRequest& request, const mask::Progress* progress) {
  ExtractContext& ctx = st.ctx;
  ctx.style = editor.extract.current;
  ctx.options = editor.extract.options;
  ctx.overrides = editor.extract.overrides;
  ctx.length = editor.extract.length;
  ctx.compat = compat_of(editor);
  ctx.progress = progress;
  ctx.technology = editor.technology.get();
  ctx.variable = request.variable;
  ctx.prepared = true;
  st.mode = request.mode;
  st.sink = request.sink;
}

// The run's root: the named cell (the window's root or the selection).
db::CellDef* find_root(editor::EditorState& editor, const RunRequest& request) {
  return request.root.empty() ? nullptr : editor.cells.find(request.root);
}

bool unnamed_root(const State& st, const db::CellDef& root) {
  if (root.name() != kUnnamed) {
    return false;
  }
  emit(st.sink, kCodeStart, "Please name the cell before extracting.");
  return true;
}

// DBCellReadArea(rootUse, bbox, TRUE) and DBFixMismatch.
bool load_tree(const State& st, db::CellDef& root, const RunRequest& request) {
  const gds::SubtreeResult loaded = gds::load_area(root, root.bbox(), request.loader, true, st.ctx.compat);
  if (loaded.failed != nullptr) {
    emit(st.sink, kCodeStart, "Failure to read entire subtree of cell.");
    emit(st.sink, kCodeStart, "Failed on cell " + loaded.failed->name() + ".");
    return false;
  }
  if (request.loader.fix_mismatches) {
    request.loader.fix_mismatches();
  }
  return true;
}

// The directory a relative target is shown against (PaOpen's realName).
std::string working_directory(const OutputLocation& where) {
  if (!where.current_directory.empty()) {
    return where.current_directory;
  }
  std::error_code ec;
  return std::filesystem::current_path(ec).string();
}

std::string shown_name(const std::string& target, const std::string& cwd) {
  const std::string prefix = cwd + "/";
  return !cwd.empty() && target.starts_with(prefix) ? target.substr(prefix.size()) : target;
}

// ExtFileOpen(def, outName, "w") and ExtCell's first messages.
void open_output(Job& job, cell::LookupContext& lookup, const OutputLocation& where,
                 const std::optional<std::string>& out_name) {
  BufferSink buffer;
  const std::string cwd = working_directory(where);
  cell::LookupContext ctx{lookup.fs, lookup.paths, lookup.vars, buffer};
  // `extract cell NAME` opens NAME.ext: the name stands in for the cell's.
  const db::CellDef named(out_name.value_or(std::string{}), 0);
  const db::CellDef& target = out_name ? named : *job.cell;
  OutputLocation at = where;
  at.current_directory = cwd;  // the target is absolute; it is shown relative to cwd
  if (out_name) {
    at.local_path.reset();
  }
  auto opened = AtomicOutput::open(ctx, target, ".ext", at);
  const std::string shown = opened.ok() ? shown_name(opened.value().target(), cwd)
                                        : ext_base_name(target, at.local_path) + ".ext";
  buffer.emit(diag::make(kCodeCell, "Extracting " + job.cell->name() + " into " + shown + ":"));
  if (opened.ok()) {
    job.output.emplace(std::move(opened).value());
  } else {
    buffer.emit(diag::make(kCodeOpen, "Cannot open output file."));
    job.extract = false;
  }
  job.messages = std::move(buffer.items);
}

// extCellFile's temporary unique pass (EXT_UNIQ_TEMP, or
// EXT_UNIQ_TEMP_NOPORTS for the root with `notopports`).
void unique_pass(State& st, Job& job) {
  const ExtractOptions& o = st.ctx.options;
  if (!o.unique || !job.extract) {
    return;
  }
  BufferSink buffer;
  UniqueContext uc;
  uc.technology = st.ctx.technology;
  uc.style = st.ctx.style.get();
  uc.temporary = true;
  uc.compat = st.ctx.compat;
  uc.sink = &buffer;
  const SubstrateSeeds seeds = substrate_seeds(*job.cell, *uc.technology, *uc.style, o.extresist);
  for (const auto& [plane, tile] : seeds.tiles) {
    uc.substrate.substrate_seeds.push_back(
        RegionSeed{plane, db::Point{tile.area.x0, tile.area.y0}, tile.split && tile.right_side});
  }
  if (seeds.tileless) {
    uc.substrate.substrate_without_tiles = uc.style->substrate.plane;
  }
  const UniqueMode mode = o.unique_notopports && job.is_top ? UniqueMode::noports : UniqueMode::all;
  (void)st.renamer.apply(*job.cell, mode, uc);
  for (diag::Diagnostic& d : buffer.items) {
    job.messages.push_back(std::move(d));
  }
}

// extPrepSubstrate: swaps the isolated substrate plane in until the finish.
void prepare_substrate(State& st, db::CellDef& cell) {
  const ExtStyle& style = *st.ctx.style;
  std::optional<db::Plane> plane = isolated_substrate_plane(cell, *st.ctx.technology, style);
  if (!plane) {
    return;
  }
  db::Plane& slot = cell.plane(*style.substrate.plane);
  st.saved.emplace_back(&cell, std::move(slot));
  slot = std::move(*plane);
}

// One stack entry, prepared as ExtCell would start it.
void prepare_job(State& st, Job job, cell::LookupContext& lookup, const OutputLocation& where,
                 const std::optional<std::string>& out_name) {
  if (job.extract && has_property(*job.cell, "noextract")) {
    job.extract = false;
  }
  bool prep = true;  // ExtCell returns before extPrepSubstrate when the file cannot be opened
  if (job.extract) {
    open_output(job, lookup, where, out_name);
    prep = job.output.has_value();
    unique_pass(st, job);
  }
  if (prep) {
    prepare_substrate(st, *job.cell);
  }
  st.jobs.push_back(std::move(job));
}

struct Stack {
  std::vector<db::CellDef*> cells;              // extraction order
  std::unordered_set<const db::CellDef*> current;  // CDNOEXTRACT
  db::CellDef* top = nullptr;                   // the root that gets path length
};

// ExtAll / ExtIncremental.
std::optional<Stack> tree_stack(State& st, editor::EditorState& editor, db::CellDef& root, const RunRequest& request) {
  if (!load_tree(st, root, request)) {
    return std::nullopt;
  }
  Stack out;
  out.top = &root;
  DefList list(st.ctx.compat, st.ctx.dont_use);
  diag::NullSink quiet;
  platform::FileSystem& fs = files_of(request);
  cell::LookupContext lookup{fs, editor.search_paths, editor.path_variables, quiet};
  if (request.mode == RunMode::incremental) {
    const std::int64_t now = request.now != 0 ? request.now
                                              : cell::stamp_from_time(std::chrono::system_clock::now());
    cell::update_timestamps(editor.cells, nullptr, now, lookup, st.ctx.compat.magic_compat);
    list.stale = [&](const db::CellDef& def) { return timestamp_mismatch(lookup, def, st.ctx.options.path); };
  }
  list.visit(root);
  out.cells = std::move(list.order);
  out.current = std::move(list.current);
  if (out.cells.empty() && request.mode == RunMode::all) {
    emit(st.sink, kCodeStart, "Warning:  There is nothing here to extract.");
    return std::nullopt;
  }
  return out;
}

// The children of `def` (bottom-up, `def` excluded): their substrates are
// prepared for ExtractOneCell and extParents.
std::vector<db::CellDef*> children_below(State& st, db::CellDef& def) {
  DefList list(st.ctx.compat, st.ctx.dont_use);
  list.visit(def);
  std::vector<db::CellDef*> out = std::move(list.order);
  if (!out.empty() && out.back() == &def) {
    out.pop_back();
  }
  return out;
}

void prepare_stack(State& st, editor::EditorState& editor, const Stack& stack, const RunRequest& request) {
  diag::NullSink quiet;
  platform::FileSystem& fs = files_of(request);
  cell::LookupContext lookup{fs, editor.search_paths, editor.path_variables, quiet};
  const OutputLocation where = location_of(editor, request);
  for (db::CellDef* cell : stack.cells) {
    Job job;
    job.cell = cell;
    job.extract = !stack.current.contains(cell);
    job.is_top = cell == stack.top;
    prepare_job(st, std::move(job), lookup, where, std::nullopt);
  }
  st.totals = true;
}

// extDefListFunc reads every cell it enumerates (DBCellEnum's DBCellRead),
// so ExtractOneCell and extParents read the whole subtree of the selected
// cell first, without DBFixMismatch (EX-065: `Cell leaf read from path`
// before `Extracting myinv`, and myinv's `use` record of leaf).
void read_below(const State& st, db::CellDef& def, const RunRequest& request) {
  gds::SubtreeLoader loader = request.loader;
  loader.fix_mismatches = {};
  (void)gds::load_area(def, def.bbox(), loader, false, st.ctx.compat);
}

// ExtractOneCell / extParents: the children's substrates, then the cells.
void prepare_selected(State& st, editor::EditorState& editor, db::CellDef& def, const RunRequest& request) {
  read_below(st, def, request);
  for (db::CellDef* child : children_below(st, def)) {
    prepare_substrate(st, *child);
  }
  diag::NullSink quiet;
  platform::FileSystem& fs = files_of(request);
  cell::LookupContext lookup{fs, editor.search_paths, editor.path_variables, quiet};
  const OutputLocation where = location_of(editor, request);
  if (request.mode == RunMode::cell) {
    Job job;
    job.cell = &def;
    job.extract = true;
    prepare_job(st, std::move(job), lookup, where, request.output_name);
    return;
  }
  std::unordered_set<const db::CellDef*> seen;
  std::vector<db::CellDef*> pushed;
  push_parents(def, seen, pushed);
  std::reverse(pushed.begin(), pushed.end());  // StackPop order
  for (db::CellDef* cell : pushed) {
    if (request.mode == RunMode::showparents) {
      st.report.extracted.push_back(cell->name());
      continue;
    }
    Job job;
    job.cell = cell;
    job.extract = true;
    prepare_job(st, std::move(job), lookup, where, std::nullopt);
  }
  st.totals = true;
}

bool check_style(const State& st) {
  if (st.ctx.style != nullptr && st.ctx.technology != nullptr) {
    return true;
  }
  emit(st.sink, kCodeStart, "No extraction style set.");
  return false;
}

}  // namespace

diag::Outcome<PreparedRun> prepare_run(editor::EditorState& editor, const RunRequest& request,
                                       const mask::Progress* progress) {
  const db::DatabaseLock::Exclusive guard(editor.database_lock);
  PreparedRun run;
  State& st = run.state();
  init_context(st, editor, request, progress);
  db::CellDef* root = find_root(editor, request);
  if (root == nullptr) {
    emit(st.sink, kCodeStart, "No cell selected");
    return run;
  }
  if (request.mode != RunMode::showparents && !check_style(st)) {
    return run;
  }
  const bool by_tree = request.mode == RunMode::incremental || request.mode == RunMode::all;
  if (request.mode != RunMode::cell && request.mode != RunMode::showparents && unnamed_root(st, *root)) {
    return run;
  }
  if (!by_tree) {
    prepare_selected(st, editor, *root, request);
    return run;
  }
  if (std::optional<Stack> stack = tree_stack(st, editor, *root, request)) {
    prepare_stack(st, editor, *stack, request);
  }
  return run;
}

namespace {

void replay(const State& st, std::vector<diag::Diagnostic>& messages) {
  if (st.sink != nullptr) {
    for (const diag::Diagnostic& d : messages) {
      st.sink->emit(d);
    }
  }
  messages.clear();
}

// ExtCell's counts line.
void cell_counts(const State& st, const db::CellDef& cell, const CellResult& r) {
  if (r.errors <= 0 && r.warnings <= 0) {
    return;
  }
  std::string text = cell.name() + ":";
  if (r.errors > 0) {
    text += " " + plural(r.errors, "error");
  }
  if (r.warnings > 0) {
    text += " " + plural(r.warnings, "warning");
  }
  emit(st.sink, kCodeCell, std::move(text));
}

// Writes and commits the cell's text; false on a failure (reported).
bool commit(const State& st, Job& job, ExtWriter& writer, const CellResult& r) {
  auto text = writer.finish();
  if (text.ok() && r.unterminated) {
    unterminate(text.value(), *r.unterminated);
  }
  diag::Status written = text.ok() ? job.output->write(text.value()) : diag::Status{text.error()};
  if (written.ok()) {
    written = job.output->commit();
  }
  if (!written.ok()) {
    job.output->abandon();
    emit(st.sink, kCodeWrite, written.error().text);
    return false;
  }
  return true;
}

// ExtCell for one prepared job; false when the run stops (cancelled).
bool extract_job(State& st, Job& job) {
  replay(st, job.messages);
  if (!job.extract || !job.output) {
    return true;
  }
  ExtWriter writer;
  auto result = extract_cell(st.ctx, *job.cell, writer, job.is_top);
  if (!result.ok()) {
    job.output->abandon();
    emit(st.sink, kCodeWrite, result.error().text);
    ++st.report.errors;
    return true;
  }
  const CellResult& r = result.value();
  if (r.cancelled) {
    job.output->abandon();
    st.report.cancelled = true;
    return false;
  }
  if (commit(st, job, writer, r)) {
    st.report.extracted.push_back(job.cell->name());
  }
  for (const CellFeedback& f : r.feedback) {
    st.feedback.emplace_back(job.cell, f);
  }
  cell_counts(st, *job.cell, r);
  st.report.errors += r.errors;
  st.report.warnings += r.warnings;
  return true;
}

bool cancelled(const State& st) {
  return st.ctx.progress != nullptr && st.ctx.progress->cancelled();
}

}  // namespace

void extract_run(editor::EditorState& editor, PreparedRun& run) {
  const db::DatabaseLock::Shared guard(editor.database_lock);
  State& st = run.state();
  st.ctx.sink = st.sink;
  const std::size_t total = st.jobs.size();
  for (std::size_t i = 0; i < total; ++i) {
    if (cancelled(st)) {
      st.report.cancelled = true;
      break;
    }
    if (!extract_job(st, st.jobs[i])) {
      break;
    }
    if (st.ctx.progress != nullptr) {
      st.ctx.progress->update(static_cast<double>(i + 1) / static_cast<double>(total));
    }
  }
  for (Job& job : st.jobs) {
    if (job.output) {
      job.output->abandon();  // a cancelled run's unwritten cells
    }
  }
}

namespace {

void add_feedback(editor::EditorState& editor, const State& st) {
  for (const auto& [cell, f] : st.feedback) {
    const view::FeedbackStyle style =
        f.medium ? view::producer_style::kExtractionHierarchical : view::producer_style::kExtraction;
    (void)editor.feedback.add(editor.windows, *cell, f.area, 1, style, f.text, &editor.highlights);
  }
}

void print_totals(const State& st) {
  if (st.mode == RunMode::showparents) {
    std::string names;
    for (const std::string& n : st.report.extracted) {
      names += (names.empty() ? "" : ", ") + n;
    }
    emit(st.sink, kCodeCell, names);
    return;
  }
  if (st.report.errors > 0) {
    emit(st.sink, kCodeTotals, "Total of " + plural(st.report.errors, "error") + " (check feedback entries).");
  }
  if (st.report.warnings > 0) {
    emit(st.sink, kCodeTotals, "Total of " + plural(st.report.warnings, "warning") + ".");
  }
}

}  // namespace

RunReport finish_run(editor::EditorState& editor, PreparedRun& run) {
  const db::DatabaseLock::Exclusive guard(editor.database_lock);
  State& st = run.state();
  // Q-EXTRACT-6 (fix): every renamed cell is reverted, the top cell included.
  st.renamer.revert();
  for (auto it = st.saved.rbegin(); it != st.saved.rend(); ++it) {
    it->first->plane(*st.ctx.style->substrate.plane) = std::move(it->second);
  }
  st.saved.clear();
  add_feedback(editor, st);
  if (st.totals) {
    print_totals(st);
  }
  return st.report;
}

cmd::CommandResult run(editor::EditorState& editor, const RunRequest& request, const mask::Progress* progress) {
  auto prepared = prepare_run(editor, request, progress);
  if (!prepared.ok()) {
    return cmd::CommandResult::failure(prepared.error());
  }
  extract_run(editor, prepared.value());
  (void)finish_run(editor, prepared.value());
  return cmd::CommandResult::success();
}

diag::Outcome<RunReport> ensure_extracted(editor::EditorState& editor, std::string_view cell,
                                          const mask::Progress* progress) {
  return ensure_extracted(editor, cell, progress, RunRequest{});
}

diag::Outcome<RunReport> ensure_extracted(editor::EditorState& editor, std::string_view cell,
                                          const mask::Progress* progress, RunRequest base) {
  // M9 has no console: the messages are kept, and a run that cannot start
  // fails with its first one.
  diag::CollectingSink messages;
  RunRequest request = std::move(base);
  request.mode = RunMode::incremental;
  request.root = std::string{cell};
  request.sink = &messages;
  auto prepared = prepare_run(editor, request, progress);
  if (!prepared.ok()) {
    return prepared.error();
  }
  extract_run(editor, prepared.value());
  const RunReport report = finish_run(editor, prepared.value());
  for (const diag::Diagnostic& d : messages.snapshot()) {
    if (d.code == kCodeStart) {
      return d;
    }
  }
  return report;
}

namespace {

// UniqueContext for a permanent pass over `cell`.
UniqueContext permanent_context(const editor::EditorState& editor, db::CellDef& cell, diag::DiagnosticSink* sink) {
  UniqueContext uc;
  uc.technology = editor.technology.get();
  uc.style = editor.extract.current.get();
  uc.temporary = false;
  uc.compat = compat_of(editor);
  uc.sink = sink;
  const SubstrateSeeds seeds = substrate_seeds(cell, *uc.technology, *uc.style, editor.extract.options.extresist);
  for (const auto& [plane, tile] : seeds.tiles) {
    uc.substrate.substrate_seeds.push_back(
        RegionSeed{plane, db::Point{tile.area.x0, tile.area.y0}, tile.split && tile.right_side});
  }
  if (seeds.tileless) {
    uc.substrate.substrate_without_tiles = uc.style->substrate.plane;
  }
  return uc;
}

}  // namespace

cmd::CommandResult run_unique(editor::EditorState& editor, const RunRequest& request, UniqueMode mode) {
  State st;
  init_context(st, editor, request, nullptr);
  db::CellDef* root = find_root(editor, request);
  if (root == nullptr || !check_style(st) || !load_tree(st, *root, request)) {
    return cmd::CommandResult::success();
  }
  if (editor.extract.options.unique) {
    editor.extract.options.unique = false;
    emit(st.sink, kCodeCell, "Warning:  Extract option \"do unique\" disabled because \"extract unique\" was run.");
  }
  DefList list(st.ctx.compat, st.ctx.dont_use);
  list.visit(*root);
  std::size_t nwarn = 0;
  UniqueRenamer renamer;
  for (db::CellDef* cell : list.order) {
    const bool top = cell == list.order.back();
    const UniqueMode local = mode == UniqueMode::notopports && !top ? UniqueMode::all : mode;
    const std::size_t before = renamer.feedback().size();
    (void)renamer.apply(*cell, local, permanent_context(editor, *cell, st.sink));
    nwarn += renamer.warnings();
    for (std::size_t i = before; i < renamer.feedback().size(); ++i) {
      const NodeFeedback& f = renamer.feedback()[i];
      st.feedback.emplace_back(cell, CellFeedback{f.area, f.text, f.medium});
    }
  }
  add_feedback(editor, st);
  if (nwarn > 0) {
    emit(st.sink, kCodeTotals, std::to_string(nwarn) + " uncorrected errors (see the feedback info)");
  }
  return cmd::CommandResult::success();
}

}  // namespace hefesto::extract
