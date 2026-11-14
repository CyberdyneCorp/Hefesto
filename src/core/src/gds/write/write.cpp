// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The two phases of a GDS write (see hefesto/gds/write/write.hpp). Ports
// Magic 8.3.694 (fc5daa9e) calma/CalmaWrite.c CalmaWrite (the style check,
// subtree loading, DBFixMismatch, ENDLIB and the problem count), the output
// scale factors of calmaOutFunc and calmaOutDate, and the file handling of
// commands/CmdCD.c CmdCalma, Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md).
//
// Fixes and platform choices (design D8, D12):
// - the style is checked before the file is opened (Q-MASK-219);
// - with magic-compat OFF a mis-scaling style (Q-MASK-225) and a disallowed
//   abstract view (Q-MASK-226) refuse the write before the file is opened;
// - the output goes to a temporary file renamed onto the target when the
//   write ends (also after a failure Magic would leave terminated), and is
//   removed when the write is cancelled (Q-MASK-414).
// Owner: GDSW.

#include "hefesto/gds/write/write.hpp"

#include <cstdio>
#include <unordered_set>
#include <utility>

#include "codes.hpp"
#include "hefesto/db/use_index.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/mask/gen/throttle.hpp"
#include "writer.hpp"

namespace hefesto::gds {

namespace {

// Counts the generation problems (Magic's feedback entries) while passing
// them on.
class CountingSink final : public diag::DiagnosticSink {
 public:
  explicit CountingSink(diag::DiagnosticSink& next) : next_(next) {}
  void emit(const diag::Diagnostic& diagnostic) override {
    ++count_;
    next_.emit(diagnostic);
  }
  [[nodiscard]] std::size_t count() const noexcept { return count_; }

 private:
  diag::DiagnosticSink& next_;
  std::size_t count_ = 0;
};

std::string units_text(std::int32_t units, std::int32_t expander) {
  std::array<char, 64> buffer{};
  const float value = static_cast<float>(units) / static_cast<float>(expander);
  (void)std::snprintf(buffer.data(), buffer.size(), "%2.1f", static_cast<double>(value));
  return buffer.data();
}

// With magic-compat OFF, the abstract views a write would reach with
// `gds abstract disallow` (Q-MASK-226): the first one found, or nullptr.
class AbstractCheck {
 public:
  explicit AbstractCheck(const GdsSettings& settings, const compat::CompatSettings& compat)
      : settings_(settings), compat_(compat) {}

  const db::CellDef* find(const db::CellDef& def, std::size_t depth) {
    if (depth > kMaxSubtreeDepth || !seen_.insert(&def).second || !def.flags().available) {
      return nullptr;
    }
    const detail::CellKind kind = detail::cell_kind(def);
    if (kind.copied() && settings_.addendum) {
      return nullptr;
    }
    if (kind.abstract && !kind.read_only) {
      return &def;
    }
    if (!kind.traverses_children()) {
      return nullptr;
    }
    for (const db::CellUse* use : db::uses_in_search_order(def, compat_)) {
      if (use->def == nullptr) {
        continue;
      }
      if (const db::CellDef* found = find(*use->def, depth + 1)) {
        return found;
      }
    }
    return nullptr;
  }

 private:
  const GdsSettings& settings_;
  compat::CompatSettings compat_;
  std::unordered_set<const db::CellDef*> seen_;
};

diag::Diagnostic subtree_error(const SubtreeResult& result) {
  if (result.too_deep) {
    return diag::make(codes::kDepth, "Failure to read entire subtree of the cell.\nThe hierarchy is deeper than " +
                                         std::to_string(kMaxSubtreeDepth) + " levels at cell " +
                                         result.failed->name() + ".");
  }
  return diag::make(codes::kSubtree,
                    "Failure to read entire subtree of the cell.\nFailed on cell " + result.failed->name() + ".");
}

// The checks made before the output file is opened, after subtree loading.
diag::Status check_before_open(const PreparedWrite& prepared) {
  if (prepared.compat.magic_compat) {
    return diag::Done{};
  }
  const ScaleResult scale = output_scale(*prepared.style, prepared.compat);
  if (scale.refused) {
    return scale.errors.back();
  }
  if (prepared.options.settings.allow_abstract) {
    return diag::Done{};
  }
  AbstractCheck check(prepared.options.settings, prepared.compat);
  if (const db::CellDef* found = check.find(*prepared.root, 0)) {
    return diag::make(codes::kAbstract, "Error:  Cell \"" + found->name() + "\" is an abstract view;  cannot write GDS.");
  }
  return diag::Done{};
}

// CmdCalma's message when CalmaWrite returns FALSE, naming the file as
// typed.
diag::Diagnostic io_error(const GdsWriteOptions& options, const platform::Path& path) {
  const std::string name = options.typed_name.empty() ? path.generic_string() : options.typed_name;
  return diag::make(codes::kOutputFile, "I/O error in writing file " + name + ".\nFile may be incompletely written.");
}

cmd::CommandResult finished(std::vector<diag::Diagnostic> diagnostics, std::optional<diag::Diagnostic> error) {
  cmd::CommandResult result = cmd::CommandResult::success();
  result.diagnostics = std::move(diagnostics);
  if (error) {
    result.status = cmd::Status::failed;
    result.diagnostics.push_back(std::move(*error));
  }
  return result;
}

}  // namespace

ScaleResult output_scale(const mask::OutputStyle& style, const compat::CompatSettings& compat) {
  const std::int32_t units = style.has_option(mask::style_option::angstroms) ? 100 : 10;
  const std::int32_t expander = style.scale.expander;
  ScaleResult out;
  if (expander > 0 && units % expander == 0) {
    out.scale.write = std::int64_t{style.scale.scale_factor} * units / expander;
    out.scale.paint = units / expander;
    return out;
  }
  const std::string nm = expander > 0 ? units_text(units, expander) : std::string{"0.0"};
  if (!compat.magic_compat) {
    out.refused = true;
    out.errors.push_back(diag::make(codes::kScaleUnits,
                                    "Calma output error:  Output scale units are " + nm +
                                        " nanometers, which GDS output cannot represent.  Add \"units "
                                        "angstroms\" to the cifoutput section of the techfile or use a coarser "
                                        "scalegrid."));
    return out;
  }
  std::string text = "Calma output error:  Output scale units are " + nm + " nanometers.\n";
  text += "Magic Calma output will be scaled incorrectly!\n";
  if (units == 10 && expander > 0 && 100 % expander == 0) {
    text += "Please add \"units angstroms\" to the cifoutput section of the techfile.";
  } else {
    text += "Magic GDS output is limited to a minimum dimension of 1 angstrom.";
  }
  out.errors.push_back(diag::make(codes::kScaleUnits, std::move(text)));
  out.scale.write = style.scale.scale_factor;
  out.scale.paint = 1;
  return out;
}

std::array<std::int16_t, 12> structure_dates(std::int64_t creation, std::int64_t now,
                                             const compat::CompatSettings& compat, std::string_view zone) {
  const std::array<std::int16_t, 6> created = encode_date(creation, zone);
  const std::array<std::int16_t, 6> modified = encode_date(compat.magic_compat ? now : 0, zone);
  std::array<std::int16_t, 12> out{};
  for (std::size_t i = 0; i < 6; ++i) {
    out[i] = created[i];
    out[i + 6] = modified[i];
  }
  return out;
}

diag::Outcome<PreparedWrite> prepare_write(editor::EditorState& editor, db::CellDef& root,
                                           const GdsWriteOptions& options) {
  if (editor.mask.output_styles.current_style() == nullptr) {
    return diag::make(codes::kNoStyle, "No CIF/GDS output style set!");
  }
  PreparedWrite prepared;
  prepared.editor = &editor;
  prepared.root = &root;
  prepared.options = options;
  prepared.compat = editor.drc.compat_settings();
  const SubtreeResult loaded = load_subtree(root, options.loader, !options.settings.allow_undefined, prepared.compat);
  if (!loaded.ok()) {
    return subtree_error(loaded);
  }
  // A finer-grid read while loading rescales the styles: take the style now.
  prepared.style = editor.mask.output_styles.current_shared();
  if (prepared.style == nullptr) {
    return diag::make(codes::kNoStyle, "No CIF/GDS output style set!");
  }
  prepared.hierarchy.subcells = editor.mask.cif.hier;
  prepared.hierarchy.arrays = editor.mask.cif.array;
  prepared.hierarchy.unfracture = editor.mask.cif.unfracture;
  prepared.hierarchy.fixed_bbox = true;
  if (diag::Status s = check_before_open(prepared); !s.ok()) {
    return s.error();
  }
  return prepared;
}

cmd::CommandResult emit_write(const PreparedWrite& prepared, const platform::Path& path,
                              const mask::Progress* progress) {
  platform::LocalFileSystem local;
  platform::FileSystem& fs = prepared.options.files != nullptr ? *prepared.options.files : local;
  mask::OutputFile file(fs, path);
  diag::Outcome<std::unique_ptr<ByteSink>> opened = open_sink(fs, file.temporary(), prepared.options.settings.compress);
  if (!opened.ok()) {
    return cmd::CommandResult::failure(
        diag::make(codes::kOutputFile, "Cannot open " + path.generic_string() + " to write GDS-II stream output"));
  }
  ByteSink& sink = *opened.value();
  RecordWriter records(sink);
  diag::CollectingSink collected;
  CountingSink problems(collected);
  mask::GenerationContext ctx;
  ctx.progress = progress;
  ctx.sink = &problems;
  detail::LibraryWriter writer(prepared, records, collected, ctx);
  detail::LibraryWriter::Result result = writer.write();
  if (!result.status.ok() && result.status.error().code == codes::kCancelled) {
    (void)sink.finish();
    file.discard();
    return finished(collected.take(), result.status.error());
  }
  // The file is closed and kept even after a failure, as Magic leaves it.
  const auto keep_first_error = [&result](diag::Status s) {
    if (!s.ok() && result.status.ok()) {
      result.status = std::move(s);
    }
  };
  keep_first_error(records.flush());
  keep_first_error(sink.finish());
  keep_first_error(file.commit());
  std::vector<diag::Diagnostic> out = collected.take();
  if (problems.count() > 0) {
    out.push_back(diag::make(codes::kProblems,
                             std::to_string(problems.count()) + " problems occurred.  See feedback entries."));
  }
  if (!result.status.ok()) {
    out.push_back(result.status.error());
  }
  if (result.good && result.status.ok()) {
    return finished(std::move(out), std::nullopt);
  }
  return finished(std::move(out), io_error(prepared.options, path));
}

cmd::CommandResult write(editor::EditorState& editor, db::CellDef& root, const platform::Path& path,
                         const GdsWriteOptions& options, const mask::Progress* progress) {
  diag::Outcome<PreparedWrite> prepared = prepare_write(editor, root, options);
  if (!prepared) {
    const std::string_view code = prepared.error().code;
    if (code == codes::kSubtree || code == codes::kDepth) {
      // Magic opened (and truncated) the file before CalmaWrite failed.
      platform::LocalFileSystem local;
      platform::FileSystem& fs = options.files != nullptr ? *options.files : local;
      (void)fs.write_atomic(path, {});
    }
    // CmdCalma reports every CalmaWrite failure as an I/O error; the style
    // check keeps that text although it comes before the file (Q-MASK-219).
    const bool calma_write_failed = code == codes::kSubtree || code == codes::kDepth || code == codes::kNoStyle;
    if (!calma_write_failed) {
      return cmd::CommandResult::failure(prepared.error());
    }
    return finished({prepared.error()}, io_error(options, path));
  }
  return emit_write(prepared.value(), path, progress);
}

}  // namespace hefesto::gds
