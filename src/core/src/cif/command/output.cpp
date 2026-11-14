// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The output options of the `cif` command (capability cif-format-io "cif
// command dispatch", "cif read, write and flat options", "cif see, paint,
// coverage and statistics options"; mask-hierarchical-generation
// "Hierarchical layer preview"). Ported from Magic 8.3.694 (fc5daa9e)
// commands/CmdCD.c CmdCif (READ, CIF_WRITE, CIF_WRITE_FLAT, outputCIF,
// SEE, ARRAY, HIER, CIF_PAINT, COVERAGE, STATS), cif/CIFsee.c (CIFSeeLayer,
// CIFSeeHierLayer, cifSeeFunc, CIFCoverageLayer) and cif/CIFmain.c
// CIFPrintStats, Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md). The generation itself is GEN's
// (mask/gen/generate.hpp).
//
// The `cif statistics` counters belong to the session in Magic (CIFTileOps
// and friends); until MaskSession holds them they are kept here per
// editor and count the generations of the `cif` command's previews.
// Owner: CIF.

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <unordered_map>

#include "command_impl.hpp"
#include "hefesto/cif/read/read.hpp"
#include "hefesto/cif/write/write.hpp"
#include "hefesto/cell/search_path.hpp"
#include "hefesto/drc/engine/marking.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/write/subtree.hpp"
#include "hefesto/mask/gen/generate.hpp"
#include "hefesto/view/feedback.hpp"

namespace hefesto::cif::command_detail {

namespace {

using cmd::CommandResult;

constexpr std::string_view kNoStyle = "CIF-CMD-003";

// Counts generation problems (Magic's feedback entries) while keeping them.
class CountingSink final : public diag::DiagnosticSink {
 public:
  void emit(const diag::Diagnostic& diagnostic) override {
    ++count;
    kept.push_back(diagnostic);
  }
  std::size_t count = 0;
  std::vector<diag::Diagnostic> kept;
};

// A preview's generation context, its problems and statistics.
class Preview {
 public:
  explicit Preview(const CifCall& c) : c_(c) { ctx.sink = &problems; }
  ~Preview();
  Preview(const Preview&) = delete;
  Preview& operator=(const Preview&) = delete;

  mask::GenerationContext ctx;

 private:
  const CifCall& c_;
  CountingSink problems;
};

mask::StatisticsCounters& session_counters(const editor::EditorState& editor) {
  static std::mutex mutex;
  static std::unordered_map<const editor::EditorState*, mask::StatisticsCounters> counters;
  const std::lock_guard<std::mutex> lock(mutex);
  return counters[&editor];
}

Preview::~Preview() {
  session_counters(*c_.s.editor).add(ctx.statistics);
  for (const diag::Diagnostic& d : problems.kept) {
    c_.s.ctx->emit(d);
  }
  if (problems.count > 0) {
    c_.s.ctx->emit(diag::make(kProblems, std::to_string(problems.count) + " problems occurred.  See feedback entries."));
  }
}

// The window's root cell (windCheckOnlyWindow; the dispatcher checked that
// a window exists).
db::CellDef* window_root(const CifCall& c) {
  const view::LayoutWindow* w = c.s.editor->windows.current();
  return w != nullptr ? w->root() : nullptr;
}

const mask::OutputStyle* current_style(const CifCall& c) {
  const mask::OutputStyle* style = c.s.editor->mask.output_styles.current_style();
  if (style == nullptr) {
    c.s.ctx->emit(diag::make(kNoStyle, "No CIF output style set!"));
  }
  return style;
}

std::string environment(const char* name) {
  const char* value = std::getenv(name);  // NOLINT(concurrency-mt-unsafe): read once per command
  return value != nullptr ? std::string{value} : std::string{"?"};
}

// cifSeeFunc for each tile: the align warning, then the feedback area.
void show_tiles(const CifCall& c, const db::CellDef& root, const std::vector<db::TileView>& tiles,
                const std::string& layer, const mask::OutputStyle& style) {
  editor::EditorState& editor = *c.s.editor;
  const std::string text = "CIF layer \"" + layer + "\"";
  const std::int32_t scale = std::max(style.scale.scale_factor, 1);
  for (const db::TileView& tile : tiles) {
    const bool odd = (tile.area.x0 & 1) != 0 || (tile.area.y0 & 1) != 0;
    if (odd && editor.mask.warnings.level == mask::WarningLevel::align) {
      std::array<char, 128> buffer{};
      (void)std::snprintf(buffer.data(), buffer.size(), "Warning: Corner (%.1f, %.1f) has half-lambda placement.",
                          static_cast<double>(static_cast<float>(tile.area.x0) / static_cast<float>(scale)),
                          static_cast<double>(static_cast<float>(tile.area.y0) / static_cast<float>(scale)));
      cu::warn(c.s, buffer.data());
    }
    (void)editor.feedback.add(editor.windows, root, tile.area, scale,
                              view::tile_feedback_style(view::kStylePaleHighlights, tile), text, &editor.highlights);
  }
}

}  // namespace

CommandResult write_option(const CifCall& c, const std::string& name, bool flat) {
  editor::EditorState& editor = *c.s.editor;
  db::CellDef* root = window_root(c);
  if (root == nullptr) {
    return CommandResult::failure(diag::make("VIEW-WINDOW-003", "Point to a window first"));
  }
  CifWriteOptions options;
  options.settings = editor.mask.cif;
  options.flat = flat;
  options.typed_name = name;
  options.loader = gds::command_subtree_loader(*c.s.ctx, editor);
  options.files = &c.s.ctx->services().files;
  options.time_zone = editor.mask.time_zone;
  options.user = environment("USER");
  options.machine = environment("HOSTNAME");
  return write(editor, *root, output_file_name(name, editor.drc.compat_settings()), options);
}

CommandResult read_option(const CifCall& c) {
  if (c.argc() != 3) {
    return wrong_args(c, kRead);
  }
  editor::EditorState& editor = *c.s.editor;
  const std::string& name = c.words[1];
  const std::string candidate = output_file_name(name, editor.drc.compat_settings());
  diag::CollectingSink quiet;
  cell::LookupContext lookup{c.s.ctx->services().files, editor.search_paths, editor.path_variables, quiet};
  const std::optional<cell::FoundFile> found = cell::search_file(lookup, candidate, editor.search_paths.path, nullptr);
  // PaOpen adds no ".gz": only the file itself counts.
  if (!found || found->path.filename() != platform::Path{candidate}.filename()) {
    cu::warn(c.s, "Cannot open " + name + ".cif to read CIF.");
    return CommandResult::success();
  }
  CifReadOptions options;
  options.settings = editor.mask.cif;
  options.warnings = editor.mask.warnings;
  options.typed_name = name;
  options.files = &c.s.ctx->services().files;
  return read(editor, found->path, options);
}

// SEE, and the preview forms of ARRAY and HIER (three words).
CommandResult see_option(const CifCall& c, std::size_t option) {
  if (c.argc() != 3) {
    return wrong_args(c, option);
  }
  editor::EditorState& editor = *c.s.editor;
  if (!editor.box || editor.box_root == nullptr) {
    cu::warn(c.s, "Use the box to select the area in which you want to see CIF.");
    return CommandResult::success();
  }
  const mask::OutputStyle* style = current_style(c);
  if (style == nullptr) {
    return CommandResult::success();
  }
  const db::CellDef& root = *editor.box_root;
  const std::string& layer = c.words[1];
  diag::Outcome<std::vector<db::TileView>> tiles = diag::Outcome<std::vector<db::TileView>>{std::vector<db::TileView>{}};
  {
    Preview preview(c);
    const mask::GenerationSource source = mask::generation_source(editor);
    tiles = option == kSee ? mask::see_tiles(source, *style, root, layer, *editor.box, preview.ctx)
                           : mask::see_hierarchical(source, *style, root, layer, *editor.box, option == kArray,
                                                    option == kHier, preview.ctx);
  }
  if (!tiles.ok()) {
    c.s.ctx->emit(tiles.error());
    return CommandResult::success();
  }
  show_tiles(c, root, tiles.value(), layer, *style);
  return CommandResult::success();
}

CommandResult paint_option(const CifCall& c) {
  if (c.argc() != 4 && c.argc() != 5) {
    return wrong_args(c, kPaint);
  }
  editor::EditorState& editor = *c.s.editor;
  if (!editor.box || editor.box_root == nullptr) {
    cu::warn(c.s, "Use the box to select the area in which you want to paint CIF.");
    return CommandResult::success();
  }
  db::CellDef& root = *editor.box_root;
  db::CellDef* target = &root;
  if (c.argc() == 5) {
    target = editor.cells.find(c.words[3]);
    if (target == nullptr) {
      auto made = editor.cells.create(c.words[3], editor.technology->plane_count());
      if (!made.ok()) {
        return cu::fail(made.error());
      }
      target = made.value();
      target->flags().available = true;  // DBCellSetAvail
    }
  }
  const diag::Outcome<tech::TileType> type = cu::parse_layer(*editor.technology, c.words[2]);
  if (!type.ok()) {
    c.s.ctx->emit(type.error());
    return CommandResult::success();
  }
  if (type.value() < tech::kFirstUserType) {  // layer >= TT_TECHDEPBASE
    return CommandResult::success();
  }
  diag::Status painted = diag::Done{};
  {
    Preview preview(c);
    painted = mask::paint(editor, root, c.words[1], *editor.box, type.value(), *target, preview.ctx);
  }
  if (!painted.ok()) {
    c.s.ctx->emit(painted.error());
    return CommandResult::success();
  }
  // CIFPaintLayer queues the window's root, then CmdCif the painted cell.
  drc::check_this(editor, root, *editor.box);
  drc::check_this(editor, *target, *editor.box);
  return CommandResult::success();
}

CommandResult coverage_option(const CifCall& c) {
  editor::EditorState& editor = *c.s.editor;
  const bool box = c.argc() == 4 && c.words[2] == "box";
  if (!box && c.argc() != 3) {
    return wrong_args(c, kCoverage);
  }
  db::CellDef* root = window_root(c);
  db::Rect area = root != nullptr ? root->bbox() : db::Rect{};
  if (box) {
    if (!editor.box || editor.box_root == nullptr) {
      cu::warn(c.s, "Box requested but no cursor box exists");
      return CommandResult::success();
    }
    root = editor.box_root;
    area = *editor.box;
  }
  if (root == nullptr) {
    return CommandResult::failure(diag::make("VIEW-WINDOW-003", "Point to a window first"));
  }
  diag::Outcome<mask::Coverage> covered = diag::Outcome<mask::Coverage>{mask::Coverage{}};
  {
    Preview preview(c);
    covered = mask::coverage(editor, *root, c.words[1], area, preview.ctx);
  }
  if (!covered.ok()) {
    c.s.ctx->emit(covered.error());
    return CommandResult::success();
  }
  if (c.list) {
    return CommandResult::success(cmd::Value{static_cast<double>(covered.value().fraction())});
  }
  cu::print_block(c.s, mask::coverage_report(covered.value(), box));
  return CommandResult::success();
}

CommandResult statistics_option(const CifCall& c) {
  cu::print_block(c.s, mask::statistics(session_counters(*c.s.editor)));
  return CommandResult::success();
}

}  // namespace hefesto::cif::command_detail
