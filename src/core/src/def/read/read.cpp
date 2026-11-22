// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// def::read (capability def-read "DEF file lookup", "Diagnostics and error
// limits", "Read completion"; design D4, D8): the file lookup of
// lefFileOpen, DefRead's set-up (the scale of the current output style,
// the line count), the totals, the completion of the design cell and the
// read's undo step (Q-LEFDEF-22: no "This action cannot be undone.").
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/defRead.c:2455-2776 and
// lef/lefWrite.c:150-220 (lefFileOpen), Copyright (C) 1985, 1990 Regents
// of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: DEFR (design D2).

#include "hefesto/def/read/read.hpp"

#include <memory>
#include <string>
#include <utility>

#include "def/read/reader.hpp"
#include "def/read/undo.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/lef/lex/lexer.hpp"

namespace hefesto::def {

namespace {

using detail::kCannotOpen;
using detail::kReport;

std::optional<db::Rect> die_area_of(const db::CellDef& def) {
  if (const auto* value = def.properties().find("FIXED_BBOX")) {
    if (const auto* dims = std::get_if<db::DimensionList>(value); dims != nullptr && dims->values.size() == 4) {
      return db::Rect{dims->values[0], dims->values[1], dims->values[2], dims->values[3]};
    }
  }
  return std::nullopt;
}

// DefRead's clean-up: labels re-attached, the box recomputed, the cell
// redisplayed and marked modified.
void complete(editor::EditorState& editor, db::CellDef& def, bool magic_compat) {
  (void)db::adjust_labels(def, *editor.technology, db::whole_plane());
  db::recompute_bbox(def, magic_compat);
  editor.windows.area_changed(def, def.bbox(), std::nullopt);
  db::mark_modified(def);
}

}  // namespace

std::optional<cell::FoundFile> find_def_file(cell::LookupContext& ctx, std::string_view name) {
  const std::size_t slash = name.find_last_of('/');
  const std::string_view base = slash == std::string_view::npos ? name : name.substr(slash + 1);
  const std::size_t dot = base.find_last_of('.');
  std::vector<std::string> candidates;
  if (dot == std::string_view::npos) {
    candidates.push_back(std::string{name} + ".def");
  } else if (base.substr(dot) == ".def") {
    candidates.emplace_back(name);
  } else {
    candidates.emplace_back(name);
    candidates.push_back(std::string{name.substr(0, name.size() - (base.size() - dot))} + ".def");
  }
  for (const std::string& candidate : candidates) {
    if (auto found = cell::search_file(ctx, candidate, ctx.paths.path, &ctx.paths.library)) {
      return found;
    }
  }
  return std::nullopt;
}

cmd::CommandResult read(editor::EditorState& editor, const platform::Path& path, const DefReadOptions& options) {
  platform::LocalFileSystem local;
  platform::FileSystem& fs = options.files != nullptr ? *options.files : local;
  auto source = gds::open_source(fs, path);
  if (!source.ok()) {
    cmd::CommandResult result = cmd::CommandResult::failure(
        diag::make(kCannotOpen, "Cannot open input file " + path.string() + " (" + source.error().text + ")."));
    return result;
  }
  const auto size = fs.size(path);
  return read(editor, *source.value(), path.string(), options, size.ok() ? size.value() : 0);
}

cmd::CommandResult read(editor::EditorState& editor, gds::ByteSource& source, std::string_view file_name,
                        const DefReadOptions& options, std::uint64_t size_hint) {
  if (editor.technology == nullptr) {
    return cmd::CommandResult::failure(diag::make(kCannotOpen, "No technology is loaded."));
  }
  DefReadOptions effective = options;
  effective.labels = options.labels || options.annotate;  // annotate implies labels
  diag::CollectingSink out;
  out.emit(diag::make(kReport, "Reading DEF data from file " + std::string{file_name} + "."));

  const compat::CompatSettings compat = editor.drc.compat_settings();
  detail::ReadContext context{editor.cells,
                              *editor.technology,
                              compat,
                              editor.mask.output_styles.current_style(),
                              editor.drc.rules.get(),
                              editor.lefdef.layers,
                              editor.lefdef.vias,
                              editor.lefdef.rules,
                              editor.grid.internal,
                              editor.grid.lambda,
                              editor.fonts.fonts.size(),
                              editor.edit_cell,
                              effective,
                              &editor.undo,
                              options.typed_name.empty() ? std::string{file_name} : options.typed_name};
  lef::Lexer lexer{source, size_hint};
  lef::MessageCounter counter;
  db::CellDef* design = nullptr;
  db::CellDef* created = nullptr;
  std::optional<db::Rect> before;
  bool die_changed = false;
  {
    detail::Reader reader{lexer, counter, out, context};
    design = reader.run();
    out.emit(diag::make(kReport, "DEF read: Processed " + std::to_string(lexer.line()) + " lines."));
    counter.summarize(out, lef::Dialect::def);
    if (design != nullptr) {
      complete(editor, *design, compat.magic_compat);
    } else {
      out.emit(diag::make(kReport, "DEF read: Design has no contents."));
    }
    if (compat.magic_compat && reader.estimated()) {
      editor.display_status_active = true;  // LefEstimate left GrDisplayStatus off DISPLAY_SUSPEND (Q-LEFDEF-141)
    }
    created = reader.created();
    die_changed = reader.die_change().changed;
    before = reader.die_change().before;
  }
  if (created != nullptr) {
    editor.undo.record(std::make_unique<detail::CreatedCellEvent>(*created));
  } else if (design != nullptr && die_changed) {
    editor.undo.record(std::make_unique<detail::DieAreaEvent>(*design, before, die_area_of(*design)));
  }
  cmd::CommandResult result = cmd::CommandResult::success(
      design != nullptr ? cmd::Value{design->name()} : cmd::Value{std::string{}});
  if (lexer.failure()) {
    out.emit(*lexer.failure());
  }
  result.diagnostics = out.take();
  return result;
}

}  // namespace hefesto::def
