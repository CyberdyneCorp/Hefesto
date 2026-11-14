// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// cif::read (capability cif-format-io "cif read, write and flat options",
// "cif drccheck option", "Read diagnostics"; design D4, D9, D13): the
// parser (parser.cpp) driving the shared read engine (mask::ReadEngine)
// through an adapter from CifReadTarget. Ports the set-up and clean-up of
// Magic 8.3.694 (fc5daa9e) cif/CIFrdutils.c CIFReadFile and
// cif/CIFrdcl.c CIFReadCellCleanup, and the placement of CIFParseCall,
// cifParseUser94 and cifParseUser95, Copyright (C) 1985, 1990 Regents of
// the University of California (see THIRD_PARTY_NOTICES.md).
//
// The adapter:
// - calls place a child with the use id of a pending 91 and the
//   translation converted exactly (or, Q-MASK-335 ON, unscaled); with
//   magic-compat OFF a call first copies the child's saved copyup planes
//   (Q-MASK-309);
// - labels carry the parser's resolved type and stickiness, are placed
//   with Magic's automatic justification and go straight into the cell
//   (DBPutLabel), not through the read planes;
// - the end of the file paints the top-level planes into the edit cell.
// The read holds the exclusive database lock and detaches the undo
// recorder (UndoDisable). Every cell the read made is queued for a DRC
// check unless `cif drccheck no`, and marked modified.
// Owner: CIF.

#include "hefesto/cif/read/read.hpp"

#include <algorithm>
#include <utility>

#include "hefesto/cif/read/parser.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/drc/engine/marking.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/mask/input/engine.hpp"

namespace hefesto::cif {

namespace {

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

/// The read engine's messages as the CIF reader prints them: the grid
/// messages are CIFReadWarning and the boundary redefinition CIFReadError,
/// at the parser's line; the units rescale is printed by the parser itself.
class EngineMessages final : public diag::DiagnosticSink {
 public:
  EngineMessages(ReadMessages& messages, diag::DiagnosticSink& console) : messages_(messages), console_(console) {}

  void emit(const diag::Diagnostic& d) override {
    if (d.code == "MASK-INPUT-012") {
      return;
    }
    if (d.code == "MASK-INPUT-010" || d.code == "MASK-INPUT-011") {
      messages_.warning(messages_.current_line(), d.text);
    } else if (d.code == "MASK-INPUT-013") {
      messages_.error(messages_.current_line(), d.text);
    } else {
      console_.emit(d);
    }
  }

 private:
  ReadMessages& messages_;
  diag::DiagnosticSink& console_;
};

/// The parser's view of the current input style.
ParseStyle parse_style(const mask::InputStyle& style) {
  ParseStyle out;
  out.name = style.name;
  out.multiplier = style.multiplier;
  for (std::size_t i = 0; i < style.cif_layers.size(); ++i) {
    const mask::CifLayerLabels labels = style.label_of(i);
    out.layers.push_back(ParseLayer{style.cif_layers[i], true, labels.type, labels.kind != mask::LabelKind::none});
  }
  out.rule_count = style.rules.size();
  out.has_copyup = std::any_of(style.rules.begin(), style.rules.end(),
                               [](const mask::InputRule& r) { return r.find(mask::OpKind::copyup) != nullptr; });
  return out;
}

/// The CifReadTarget over the read engine.
class EngineTarget final : public CifReadTarget {
 public:
  EngineTarget(editor::EditorState& editor, mask::ReadEngine& engine, bool copyup_on_call)
      : editor_(editor), engine_(engine), copyup_on_call_(copyup_on_call) {}

  diag::Status begin_cell(std::string_view name) override { return engine_.begin_cell(name); }
  diag::Status paint(std::size_t cif_layer, const mask::ReadShape& shape) override {
    return engine_.paint(cif_layer, shape);
  }
  diag::Status label(const mask::ReadLabel& l) override { return engine_.label(l); }
  diag::Status call(std::string_view child, const db::Transform& transform, const db::ArrayInfo& array) override {
    return engine_.call(child, transform, array);
  }
  diag::Status property(std::string_view key, std::string_view value) override {
    return engine_.property(key, value);
  }
  diag::Status end_cell() override { return engine_.end_cell(); }

  [[nodiscard]] bool cell_exists(std::string_view name) const override { return editor_.cells.find(name) != nullptr; }
  [[nodiscard]] std::string edit_cell_name() const override {
    return editor_.edit_cell != nullptr ? editor_.edit_cell->name() : std::string{};
  }
  diag::Status rename_cell(std::string_view from, std::string_view to) override {
    db::CellDef* def = editor_.cells.find(from);
    if (def == nullptr || to.empty()) {
      return diag::make(read_codes::kError, "cannot rename cell " + std::string{from});
    }
    return editor_.cells.rename_definition(*def, to);  // DBCellRenameDef
  }
  diag::Status rescale(std::int64_t factor) override {
    engine_.rescale(factor, 1);
    return diag::Done{};
  }
  diag::Status place(const CifCall& c) override;
  diag::Status cif_label(const CifLabel& l) override;
  diag::Status end_top() override { return engine_.finish(); }

 private:
  diag::Outcome<db::CellDef*> find_or_create(std::string_view name) {
    if (db::CellDef* def = editor_.cells.find(name)) {
      return def;
    }
    return editor_.cells.create(name, editor_.technology->plane_count());
  }
  void translate(db::Transform& t);

  editor::EditorState& editor_;
  mask::ReadEngine& engine_;
  bool copyup_on_call_;
};

// The translation in internal units, exactly; when converting f subdivides
// the grid, c (converted before it) is multiplied by the same factor.
void EngineTarget::translate(db::Transform& t) {
  const std::int64_t c = engine_.scale_coord(t.c, mask::Snap::exact);
  const std::int64_t before = std::max<std::int64_t>(engine_.style().scale_factor, 1);
  const std::int64_t f = engine_.scale_coord(t.f, mask::Snap::exact);
  const std::int64_t after = std::max<std::int64_t>(engine_.style().scale_factor, 1);
  const std::int64_t factor = before != after ? before / after : 1;
  t.c = static_cast<db::Coord>(std::clamp<std::int64_t>(c * factor, db::kPlaneMin, db::kPlaneMax));
  t.f = static_cast<db::Coord>(std::clamp<std::int64_t>(f, db::kPlaneMin, db::kPlaneMax));
}

diag::Status EngineTarget::place(const CifCall& c) {
  const diag::Outcome<db::CellDef*> child = find_or_create(c.child);
  if (!child.ok()) {
    return child.error();
  }
  if (copyup_on_call_ && c.child_defined) {
    engine_.copy_saved_planes(*child.value(), c.transform, db::ArrayInfo{});  // Q-MASK-309 OFF
  }
  db::Transform t = c.transform;
  if (c.translation_in_read_units) {
    translate(t);
  }
  const auto placed = engine_.place(*child.value(), t, db::ArrayInfo{}, c.use_id);
  if (!placed.ok()) {
    return placed.error();
  }
  return diag::Done{};
}

diag::Status EngineTarget::cif_label(const CifLabel& l) {
  db::CellDef* cell = engine_.current_cell() != nullptr ? engine_.current_cell() : editor_.edit_cell;
  if (cell == nullptr || l.label.text.empty()) {
    return diag::Done{};
  }
  db::Label label;
  label.text = l.label.text;
  label.type = static_cast<tech::TileType>(std::max(l.type, 0));
  label.rect = engine_.scale_rect(l.label.area, mask::Snap::any);
  label.sticky = l.sticky;
  const auto put = db::put_label(*cell, std::move(label), /*auto_justify=*/true);
  if (!put.ok()) {
    return put.error();
  }
  return diag::Done{};
}

// CIFReadCellCleanup's per-cell step, for every cell the read made, then
// the edit cell (whose labels and bounding box the engine's finish() set).
void clean_up(editor::EditorState& editor, const ReadSession& session, const CifReadOptions& options) {
  for (const ReadCell& made : session.symbols.cells()) {
    db::CellDef* def = editor.cells.find(made.name);
    if (def == nullptr) {
      continue;
    }
    if (options.settings.drccheck) {
      drc::check_this(editor, *def, def->bbox());
    }
    db::mark_modified(*def);
    if (def->timestamp() != 0) {
      def->flags().needs_new_timestamp = false;
    }
  }
  if (editor.edit_cell != nullptr) {
    db::mark_modified(*editor.edit_cell);
  }
}

}  // namespace

cmd::CommandResult read_text(editor::EditorState& editor, std::string_view text, const CifReadOptions& options,
                             const mask::Progress* progress) {
  const mask::InputStyle* style = editor.mask.input_styles.current_style();
  if (style == nullptr || editor.technology == nullptr) {
    return cmd::CommandResult::failure(
        diag::make(read_codes::kNoStyle, "Don't know how to read CIF:  nothing in tech file."));
  }
  const db::DatabaseLock::Exclusive lock(editor.database_lock);
  const UndoDisabled no_undo(editor.cells);
  const compat::CompatSettings compat = editor.drc.compat_settings();
  diag::CollectingSink console;
  diag::CollectingSink redirected;
  console.emit(diag::make(read_codes::kNotUndoable, "Warning: CIF reading is not undoable!  I hope that's OK."));
  ReadMessages messages(options.warnings.level, compat.magic_compat, console, &redirected);
  EngineMessages engine_messages(messages, console);
  mask::ReadEngine::Options engine_options;
  engine_options.top = editor.edit_cell;
  engine_options.rescale_allowed = options.settings.rescale;
  engine_options.copyup_on_call = !compat.magic_compat;  // Q-MASK-309
  engine_options.sink = &engine_messages;
  mask::ReadEngine engine(editor, *style, compat, engine_options);
  ParseOptions parse_options;
  parse_options.magic_compat = compat.magic_compat;
  parse_options.rescale_limit = options.settings.limit;
  parse_options.rescale_allowed = options.settings.rescale;
  parse_options.cancel = progress != nullptr ? progress->cancel : nullptr;
  EngineTarget target(editor, engine, engine_options.copyup_on_call);
  ReadSession session;
  parse(text, parse_style(*style), parse_options, target, messages, session);
  editor.mask.input_styles.replace_current(std::make_shared<const mask::InputStyle>(engine.style()));
  clean_up(editor, session, options);
  cmd::CommandResult result = cmd::CommandResult::success();
  result.diagnostics = console.take();
  const mask::WarningSettings& warnings = options.warnings;
  if (warnings.level == mask::WarningLevel::redirect && !warnings.redirect_file.empty() && !compat.magic_compat) {
    std::string body;
    for (const diag::Diagnostic& d : redirected.take()) {
      body += d.text + "\n";
    }
    platform::LocalFileSystem local;
    platform::FileSystem& fs = options.files != nullptr ? *options.files : local;
    if (diag::Status s = fs.write_atomic(warnings.redirect_file, body); !s.ok()) {
      result.diagnostics.push_back(s.error());
    }
  }
  return result;
}

cmd::CommandResult read(editor::EditorState& editor, const platform::Path& path, const CifReadOptions& options,
                        const mask::Progress* progress) {
  platform::LocalFileSystem local;
  platform::FileSystem& fs = options.files != nullptr ? *options.files : local;
  const std::string name = options.typed_name.empty() ? path.generic_string() : options.typed_name;
  if (const diag::Outcome<std::uint64_t> size = fs.size(path); size.ok() && size.value() > options.max_file_bytes) {
    return cmd::CommandResult::failure(diag::make_native(
        read_codes::kTooLarge, "Cannot read " + name + ".cif: the file has " + std::to_string(size.value()) +
                                   " bytes, more than the " + std::to_string(options.max_file_bytes) +
                                   " a CIF read accepts."));
  }
  diag::Outcome<std::string> text = fs.read(path);
  if (!text.ok()) {
    return cmd::CommandResult::failure(diag::make(read_codes::kNoFile, "Cannot open " + name + ".cif to read CIF."));
  }
  return read_text(editor, text.value(), options, progress);
}

}  // namespace hefesto::cif
