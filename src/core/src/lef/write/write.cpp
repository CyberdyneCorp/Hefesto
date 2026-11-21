// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF write entry points (capability lef-write "Single-cell file layout",
// "lef writeall arguments and scope", "writeall macro order", "Output file
// naming", "Output scale and LEF database units", "Write side effects",
// "Progress messages"; Q-LEFDEF-34, 41, 43): `lef write` writes one macro
// (with the library header when the cell is the edit root), `lef writeall`
// the header and the macros of a cell's subtree in reverse collection
// order. The file is built in memory and written in one piece (a failure
// to open writes nothing and changes nothing).
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/lefWrite.c LefWriteCell,
// LefWriteAll, lefDefPushFunc and lefFileOpen, Copyright (C) 1985, 1990
// Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: LEFW (design D2).

#include "hefesto/lef/write/write.hpp"

#include <set>

#include "hefesto/app/settings_layers.hpp"
#include "hefesto/db/use_index.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/extract/tech/style_view.hpp"
#include "hefesto/gds/write/subtree.hpp"
#include "hefesto/lef/write/macro.hpp"
#include "lef/write/library.hpp"

namespace hefesto::lef {

namespace wd = write_detail;

namespace {

// Magic's lef stack grows in bodies of this many entries; StackEnum visits
// only the newest body (lefGetProperties and lefGetSites see the last
// cells pushed, at most this many).
constexpr std::size_t kStackIncrement = 100;

bool session_compat(const editor::EditorState& editor) {
  return editor.drc.compat_settings().magic_compat;
}

const mask::OutputStyle* output_style(const editor::EditorState& editor) {
  return editor.mask.output_styles.current_style();
}

const db::CellDef* edit_root(const editor::EditorState& editor) {
  return editor.edit_root != nullptr ? editor.edit_root : editor.edit_cell;
}

std::optional<std::string> power_name(const editor::EditorState& editor, std::string_view setting) {
  const auto value = editor.settings.effective(setting);
  if (!value || value->value.empty()) {
    return std::nullopt;
  }
  return value->value;
}

MacroOptions macro_options(const LefWriteOptions& options) {
  MacroOptions m;
  m.setback = options.hide.value_or(-1);
  m.pin_only = options.pin_only.value_or(-1);
  m.top_layer = options.top_layer;
  m.do_master = options.do_master;
  return m;
}

/// One write: the session state it reads and the messages it prints.
class WriteRun {
 public:
  WriteRun(editor::EditorState& editor, const LefWriteOptions& options)
      : editor_(editor),
        options_(options),
        compat_(session_compat(editor)),
        style_(editor.technology != nullptr ? extract::StyleView::from_technology(*editor.technology, quiet_)
                                            : extract::StyleView{}) {}

  // The units a header-less write formats with: the session's with
  // magic-compat ON, recomputed with it OFF (Q-LEFDEF-34).
  [[nodiscard]] LefUnits headerless_units() const {
    return compat_ ? editor_.lefdef.units : database_units(output_style(editor_));
  }

  [[nodiscard]] std::string header(const db::CellDef& def, std::vector<const db::CellDef*> cells, LefUnits units) {
    wd::HeaderInput in;
    in.def = &def;
    in.cells = std::move(cells);
    in.library = &editor_.cells;
    in.layers = &editor_.lefdef.layers;
    in.vias = &editor_.lefdef.vias;
    in.tech = options_.tech;
    in.magic_compat = compat_;
    in.oscale = output_scale(output_style(editor_));
    in.units = units;
    return wd::library_header(in, body_);
  }

  [[nodiscard]] diag::Status macro(const db::CellDef& cell, LefUnits units, std::string& out) {
    MacroContext ctx;
    ctx.tech = editor_.technology.get();
    ctx.layers = &editor_.lefdef.layers;
    ctx.rules = editor_.drc.rules.get();
    ctx.style = &style_;
    ctx.oscale = output_scale(output_style(editor_));
    ctx.units = units;
    ctx.magic_compat = compat_;
    ctx.nocheck = editor_.lefdef.nocheck;
    ctx.vdd = power_name(editor_, app::kVdd);
    ctx.gnd = power_name(editor_, app::kGnd);
    MacroBuilder builder(ctx, cell, macro_options(options_));
    wrote_macro_ = true;
    return builder.write(out, body_);
  }

  /// Writes `text` under the first name that opens and returns the
  /// command's result: the "Generating" line first, then the write's own
  /// messages, or Magic's open error.
  [[nodiscard]] cmd::CommandResult save(const std::vector<std::string>& names, const std::string& text,
                                        const std::string& what) {
    platform::LocalFileSystem local;
    platform::FileSystem& fs = options_.files != nullptr ? *options_.files : local;
    std::string reason;
    for (const std::string& name : names) {
      const diag::Status st = fs.write_atomic(name, text);
      if (st.ok()) {
        return finish(name, what);
      }
      reason = st.error().text;
    }
    const std::string name = names.empty() ? std::string{} : names.back();
    cmd::CommandResult result = cmd::CommandResult::failure(
        diag::make("LEF-WRITE-002", "Cannot open output file " + name + " (" + reason + ")."));
    result.diagnostics.insert(result.diagnostics.begin(), generating(name, what));
    return result;
  }

  [[nodiscard]] bool magic_compat() const noexcept { return compat_; }

 private:
  static diag::Diagnostic generating(const std::string& name, const std::string& what) {
    return diag::make("LEF-WRITE-001", "Generating LEF output " + name + " " + what + ":");
  }

  cmd::CommandResult finish(const std::string& name, const std::string& what) {
    cmd::CommandResult result = cmd::CommandResult::success();
    result.diagnostics.push_back(generating(name, what));
    for (diag::Diagnostic& d : body_) {
      result.diagnostics.push_back(std::move(d));
    }
    // lefWriteMacro ends with SelectClear.
    if (wrote_macro_) {
      editor_.selection.clear();
    }
    return result;
  }

  editor::EditorState& editor_;
  const LefWriteOptions& options_;
  bool compat_;
  diag::NullSink quiet_;
  extract::StyleView style_;
  std::vector<diag::Diagnostic> body_;
  bool wrote_macro_ = false;
};

cmd::CommandResult no_technology() {
  return cmd::CommandResult::failure(diag::make_native("LEF-WRITE-011", "No technology is loaded; nothing is written"));
}

// The cell's file path as the base name. Like Magic's cd_file it holds a
// ".mag" suffix only after a failed read (DBio.c sets "<name>.mag"), which
// lefFileOpen opens as given: with magic-compat ON it is kept, so writeall
// writes the LEF into `<name>.mag` as Magic does; with it OFF the suffix is
// dropped, so a LEF never takes a layout file's name (Q-LEFDEF-138).
std::string cell_file_base(const db::CellDef& cell, bool magic_compat) {
  std::string base = cell.file_path().string();
  if (magic_compat) {
    return base;
  }
  for (const std::string_view suffix : {std::string_view{".mag.gz"}, std::string_view{".mag"}}) {
    if (base.size() > suffix.size() && base.ends_with(suffix)) {
      base.resize(base.size() - suffix.size());
      break;
    }
  }
  return base;
}

/// lefDefPushFunc over the subtree: the cells in collection order.
class Collector {
 public:
  explicit Collector(bool recurse) : recurse_(recurse) {}

  void push(const db::CellDef& def) {
    if (def.flags().internal || !seen_.insert(&def).second) {
      return;
    }
    order_.push_back(&def);
    if (recurse_) {
      children(def);
    }
  }

  // DBCellEnum: every use of `def` in enumeration order (Q-LEFDEF-41).
  void children(const db::CellDef& def) {
    (void)def.use_index().enumerate_all([&](db::CellUse& use) {
      if (use.def != nullptr) {
        push(*use.def);
      }
      return true;
    });
  }

  [[nodiscard]] const std::vector<const db::CellDef*>& order() const noexcept { return order_; }

 private:
  bool recurse_;
  std::set<const db::CellDef*> seen_;
  std::vector<const db::CellDef*> order_;
};

// The cells StackEnum visits for the header tables: all of them with
// magic-compat OFF; with it ON only the newest stack body (the last
// count % 100 cells, or the last 100).
std::vector<const db::CellDef*> header_cells(const std::vector<const db::CellDef*>& order, bool magic_compat) {
  if (!magic_compat || order.empty()) {
    return order;
  }
  const std::size_t first = ((order.size() - 1) / kStackIncrement) * kStackIncrement;
  return {order.begin() + static_cast<std::ptrdiff_t>(first), order.end()};
}

bool cancelled(const LefWriteOptions& options) {
  return options.progress != nullptr && options.progress->cancelled();
}

}  // namespace

namespace write_detail {

std::vector<std::string> output_names(std::string_view base, std::string_view cell_name, bool from_cell_file,
                                      bool magic_compat) {
  std::vector<std::string> names;
  const std::size_t slash = base.rfind('/');
  const std::size_t start = slash == std::string_view::npos ? 0 : slash + 1;
  const std::size_t dot = base.rfind('.');
  if (dot == std::string_view::npos || dot < start) {
    names.push_back(std::string{base} + ".lef");
  } else if (base.substr(dot) == ".lef") {
    names.emplace_back(base);
  } else {
    names.emplace_back(base);
    names.push_back(std::string{base.substr(0, dot)} + ".lef");
  }
  const bool retry = magic_compat ? base != cell_name : from_cell_file;
  const std::string fallback = std::string{cell_name} + ".lef";
  if (retry && names.back() != fallback) {
    names.push_back(fallback);
  }
  return names;
}

}  // namespace write_detail

// LefWriteCell.
cmd::CommandResult write(editor::EditorState& editor, db::CellDef& cell, const platform::Path& path,
                         const LefWriteOptions& options) {
  if (editor.technology == nullptr) {
    return no_technology();
  }
  // DBCellCopyAllPaint(CU_DESCEND_ALL) over the cell's bounding box reads
  // the subcells it reaches that are not read yet (DBTreeSrTiles); a cell
  // that cannot be read contributes nothing, as in Magic.
  if (options.loader.read) {
    (void)gds::load_area(cell, cell.bbox(), options.loader, false, editor.drc.compat_settings());
  }
  WriteRun run(editor, options);
  const bool is_root = &cell == edit_root(editor);
  LefUnits units = run.headerless_units();
  std::string text;
  if (is_root) {
    units = database_units(output_style(editor));
    text = run.header(cell, {&cell}, units);
  }
  if (auto st = run.macro(cell, units, text); !st.ok()) {
    return cmd::CommandResult::failure(st.error());
  }
  text += "END LIBRARY\n\n";
  const std::string base = path.string();
  cmd::CommandResult result = run.save(wd::output_names(base, cell.name(), false, run.magic_compat()), text,
                                       "for cell " + cell.name());
  if (result.ok() && is_root) {
    editor.lefdef.units = units;
  }
  return result;
}

// LefWriteAll.
cmd::CommandResult write_all(editor::EditorState& editor, db::CellDef& root, const platform::Path& path,
                             const LefWriteOptions& options) {
  if (editor.technology == nullptr) {
    return no_technology();
  }
  const gds::SubtreeResult loaded = gds::load_subtree(root, options.loader, true, editor.drc.compat_settings());
  if (!loaded.ok()) {
    const std::string failed = loaded.failed != nullptr ? loaded.failed->name() : root.name();
    cmd::CommandResult result = cmd::CommandResult::failure(
        diag::make("LEF-WRITE-003", "Could not read entire subtree of the cell."));
    result.diagnostics.push_back(diag::make("LEF-WRITE-003", "Failed on cell " + failed + "."));
    return result;
  }
  WriteRun run(editor, options);
  Collector collect(options.recurse);
  if (options.top_cell) {
    collect.push(root);
  }
  collect.children(root);
  const std::vector<const db::CellDef*>& order = collect.order();
  const LefUnits units = database_units(output_style(editor));
  std::string text = run.header(root, header_cells(order, run.magic_compat()), units);
  for (auto it = order.rbegin(); it != order.rend() && !cancelled(options); ++it) {
    if (auto st = run.macro(**it, units, text); !st.ok()) {
      return cmd::CommandResult::failure(st.error());
    }
  }
  text += "END LIBRARY\n\n";
  // lefFileOpen(rootdef, NULL): the cell's file path, else its name.
  const bool derived = path.empty();
  const bool from_file = derived && !root.file_path().empty();
  const std::string base = !derived ? path.string() : (from_file ? cell_file_base(root, run.magic_compat()) : root.name());
  cmd::CommandResult result = run.save(wd::output_names(base, root.name(), from_file, run.magic_compat()), text,
                                       "for hierarchy rooted at cell " + root.name());
  if (result.ok()) {
    editor.lefdef.units = units;
  }
  return result;
}

}  // namespace hefesto::lef
