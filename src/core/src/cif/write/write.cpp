// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The two phases of a CIF write (see hefesto/cif/write/write.hpp). Ports
// Magic 8.3.694 (fc5daa9e) cif/CIFwrite.c CIFWrite (the subtree loading,
// DBFixMismatch, the reducer check and the problem count), CIFWriteFlat,
// cifOutPreamble, and the file handling of commands/CmdCD.c CmdCif
// (outputCIF), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Choices (design D8, D13):
// - the output goes to a temporary file renamed onto the target when the
//   write ends, and is removed when the write is cancelled (Q-MASK-414);
// - a prepare failure after which Magic has already opened the file (the
//   subtree, the reducer) leaves an empty file, as Magic does;
// - a write without an output style fails before any file is touched
//   (Magic dereferences a NULL style);
// - `cif flat` refuses a reducer of 0 too (Magic divides by zero).
// Owner: CIF.

#include "hefesto/cif/write/write.hpp"

#include <chrono>
#include <optional>
#include <utility>

#include "codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/mask/gen/throttle.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/version.hpp"
#include "writer.hpp"

namespace hefesto::cif {

namespace {

namespace codes = write_codes;

// The `@@compiled` value: this file's build time (Magic's MagicCompileTime;
// masked in golden comparisons, Q-MASK-324).
constexpr std::string_view kCompiled = __DATE__ " " __TIME__;

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

diag::Diagnostic subtree_error(const gds::SubtreeResult& result) {
  if (result.too_deep) {
    return diag::make(codes::kDepth, "Failure to read in entire subtree of the cell.\nThe hierarchy is deeper than " +
                                         std::to_string(gds::kMaxSubtreeDepth) + " levels at cell " +
                                         result.failed->name() + ".");
  }
  return diag::make(codes::kSubtree,
                    "Failure to read in entire subtree of the cell.\nFailed on cell " + result.failed->name() + ".");
}

diag::Diagnostic calma_only() {
  return diag::make(codes::kCalmaOnly,
                    "The current CIF output style can only be used for writing\nCalma output.  Try picking another "
                    "output style.");
}

// CmdCif's message when CIFWrite or CIFWriteFlat returns FALSE.
diag::Diagnostic io_error(const CifWriteOptions& options, const platform::Path& path) {
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

std::int64_t now_or_clock(const CifWriteOptions& options) {
  if (options.now) {
    return *options.now;
  }
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// Magic's cd_file is the path without the `.mag` extension.
std::string source_of(const db::CellDef& root) {
  platform::Path file = root.file_path();
  if (file.empty()) {
    return "?";
  }
  if (file.extension() == ".mag") {
    file.replace_extension();
  }
  return file.generic_string();
}

// The `@@version` and `@@techdesc` values. Magic's DBTechVersion and
// DBTechDescription are never cleared by a tech load (DBTechInitVersion is
// empty), so a technology without them shows the previous one's; with
// magic-compat ON a missing value is the start-up technology's, which is
// what a session that loaded one technology shows, and with it OFF it is
// `unknown` and no `@@techdesc` line, as the requirement states.
void set_version(detail::Preamble& p, const tech::TechVersion& v, bool magic_compat) {
  std::shared_ptr<const tech::Technology> builtin;
  const tech::TechVersion* fallback = nullptr;
  if (magic_compat && (v.version_lines.empty() || v.description_lines.empty())) {
    builtin = tech::builtin_technology();
    fallback = builtin != nullptr ? &builtin->version() : nullptr;
  }
  const auto pick = [fallback](const std::vector<std::string>& own,
                               std::vector<std::string> tech::TechVersion::*field) -> std::optional<std::string> {
    if (!own.empty()) {
      return own.back();
    }
    if (fallback != nullptr && !(fallback->*field).empty()) {
      return (fallback->*field).back();
    }
    return std::nullopt;
  };
  p.version = pick(v.version_lines, &tech::TechVersion::version_lines).value_or("unknown");
  p.description = pick(v.description_lines, &tech::TechVersion::description_lines);
}

// cifOutPreamble.
std::string preamble(const PreparedWrite& prepared) {
  detail::Preamble p;
  const CifWriteOptions& options = prepared.options;
  p.user = options.user.empty() ? "?" : options.user;
  p.machine = options.machine.empty() ? "?" : options.machine;
  p.source = source_of(*prepared.root);
  p.tool = "Hefesto " + std::string{version()};
  p.compiled = std::string{kCompiled};
  if (const tech::Technology* tech = prepared.editor->technology.get()) {
    p.technology = tech->name();
    set_version(p, tech->version(), prepared.compat.magic_compat);
  }
  p.style = prepared.style->name;
  p.date = detail::ctime_text(now_or_clock(options), options.time_zone);
  return detail::preamble_text(p);
}

// Reads the cells a write reaches (see prepare_write).
diag::Status load_cells(db::CellDef& root, const CifWriteOptions& options, const compat::CompatSettings& compat) {
  const gds::SubtreeResult loaded = gds::load_subtree(root, options.loader, !options.flat, compat);
  if (!loaded.ok()) {
    return subtree_error(loaded);
  }
  return diag::Done{};
}

}  // namespace

std::string output_file_name(std::string_view name, const compat::CompatSettings& compat) {
  std::string file{name};
  if (compat.magic_compat || !file.ends_with(".cif")) {
    file += ".cif";
  }
  return file;
}

diag::Outcome<PreparedWrite> prepare_write(editor::EditorState& editor, db::CellDef& root,
                                           const CifWriteOptions& options) {
  if (editor.mask.output_styles.current_style() == nullptr || editor.technology == nullptr) {
    return diag::make(codes::kNoStyle, "Error: No CIF output style is set.");
  }
  PreparedWrite prepared;
  prepared.editor = &editor;
  prepared.root = &root;
  prepared.options = options;
  prepared.compat = editor.drc.compat_settings();
  if (diag::Status s = load_cells(root, options, prepared.compat); !s.ok()) {
    return s.error();
  }
  // A finer-grid read while loading rescales the styles: take the style now.
  prepared.style = editor.mask.output_styles.current_shared();
  if (prepared.style == nullptr) {
    return diag::make(codes::kNoStyle, "Error: No CIF output style is set.");
  }
  if (prepared.style->scale.reducer <= 0) {
    return calma_only();
  }
  return prepared;
}

cmd::CommandResult emit_write(const PreparedWrite& prepared, const platform::Path& path,
                              const mask::Progress* progress) {
  platform::LocalFileSystem local;
  platform::FileSystem& fs = prepared.options.files != nullptr ? *prepared.options.files : local;
  mask::OutputFile file(fs, path);
  diag::Outcome<std::unique_ptr<gds::ByteSink>> opened = gds::open_sink(fs, file.temporary(), 0);
  const std::string typed = prepared.options.typed_name.empty() ? path.generic_string() : prepared.options.typed_name;
  if (!opened.ok()) {
    return cmd::CommandResult::failure(diag::make(codes::kOutputFile, "Cannot open " + typed + ".cif to write CIF"));
  }
  gds::ByteSink& sink = *opened.value();
  detail::TextOut out(sink);
  diag::CollectingSink collected;
  CountingSink problems(collected);
  mask::GenerationContext ctx;
  ctx.progress = progress;
  ctx.sink = &problems;
  out.put(preamble(prepared));
  detail::SymbolWriter writer(prepared, out, collected, ctx);
  diag::Status status = prepared.options.flat ? writer.write_flat() : writer.write_hierarchical();
  std::vector<diag::Diagnostic> messages = collected.take();
  if (!status.ok() && status.error().code == codes::kCancelled) {
    (void)sink.finish();
    file.discard();
    return finished(std::move(messages), status.error());
  }
  if (problems.count() > 0) {
    messages.push_back(
        diag::make(codes::kProblems, std::to_string(problems.count()) + " problems occurred.  See feedback entries."));
  }
  // The file is closed and kept even after a failure, as Magic leaves it.
  for (diag::Status s : {out.flush(), sink.finish(), file.commit()}) {
    if (!s.ok() && status.ok()) {
      status = std::move(s);
    }
  }
  if (status.ok()) {
    return finished(std::move(messages), std::nullopt);
  }
  messages.push_back(status.error());
  return finished(std::move(messages), io_error(prepared.options, path));
}

cmd::CommandResult write(editor::EditorState& editor, db::CellDef& root, const platform::Path& path,
                         const CifWriteOptions& options, const mask::Progress* progress) {
  diag::Outcome<PreparedWrite> prepared = prepare_write(editor, root, options);
  if (prepared) {
    return emit_write(prepared.value(), path, progress);
  }
  if (prepared.error().code == codes::kNoStyle) {
    return cmd::CommandResult::failure(prepared.error());
  }
  // CmdCif opened (and truncated) the file before CIFWrite failed.
  platform::LocalFileSystem local;
  platform::FileSystem& fs = options.files != nullptr ? *options.files : local;
  std::vector<diag::Diagnostic> messages{prepared.error()};
  if (diag::Status s = fs.write_atomic(path, {}); !s.ok()) {
    messages.push_back(s.error());
  }
  return finished(std::move(messages), io_error(options, path));
}

}  // namespace hefesto::cif
