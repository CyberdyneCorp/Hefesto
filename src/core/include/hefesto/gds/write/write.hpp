// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GDS write (capability gds-write; openspec/changes/m5-mask-gds-cif/design.md
// D4 "Commands and progress", D7, D12). Magic's write mutates the database
// before it generates anything, so a write has two phases:
// - prepare_write, on the command thread under the exclusive database lock:
//   checks the style (Q-MASK-219) before any file is opened, loads the
//   subtree with Magic's recursive area search through UseIndex::search
//   (which can rebuild bins), resolves timestamp mismatches with the
//   DBReComputeBbox re-placement, and returns the checked options and root;
// - emit_write, under the shared lock: generates (mask::generate_hierarchical)
//   and writes. It mutates nothing in the layout (Q-MASK-208); the only
//   state it changes is the use index's bins, under the index mutex.
// write() calls both in sequence; the shell runs emit_write on a worker
// (D16). The structure body's element output goes through the emitters of
// gds/write/{emit,polygons,labels}.hpp, the subtree loading through
// gds/write/subtree.hpp (shared with the CIF writer). A cancelled write leaves no output file: the writer writes to a
// temporary file in the target directory and renames it on success.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); GDSW fills
// in the implementation in src/core/src/gds/write/ (design D2).

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/gds/command/settings.hpp"
#include "hefesto/gds/write/emit.hpp"
#include "hefesto/gds/write/subtree.hpp"
#include "hefesto/mask/gen/generate.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/mask/plane/plane.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::gds {

/// The options of one write: the session's `gds` settings at the time of
/// the call, and the name the user typed (for the extension rule, Q-MASK-210).
struct GdsWriteOptions {
  GdsSettings settings;
  std::string typed_name;
  /// How the prepare phase reads unavailable cells and fixes timestamp
  /// mismatches (gds::command_subtree_loader in a command); empty members:
  /// unavailable cells cannot be read and no mismatch is processed.
  SubtreeLoader loader;
  /// The file system the output is written on; null means the local one.
  platform::FileSystem* files = nullptr;
  /// The zone dates are converted in (a POSIX TZ string, Q-MASK-132);
  /// empty: the host zone.
  std::string time_zone;
  /// The current time written as the modification date under magic-compat
  /// ON (Q-MASK-202); nullopt: the system clock at each record.
  std::optional<std::int64_t> now;
};

/// The result of the prepare phase: everything emit_write needs.
struct PreparedWrite {
  editor::EditorState* editor = nullptr;
  db::CellDef* root = nullptr;
  GdsWriteOptions options;
  std::shared_ptr<const mask::OutputStyle> style;  // the current output style after subtree loading
  compat::CompatSettings compat;                   // the session's compat settings
  /// The session's `cif *hier`, `*array` and `unfracture` switches, with
  /// FIXED_BBOX enlarging the generation area (GDS).
  mask::HierarchyOptions hierarchy;
};

/// "Output scale factors" of `style`: Wscale and Pscale, the errors Magic
/// prints for units that are not a whole number of nanometers (or
/// angstroms with `units angstroms`), and whether the write is refused
/// (magic-compat OFF, Q-MASK-225).
struct ScaleResult {
  OutputScale scale;
  std::vector<diag::Diagnostic> errors;
  bool refused = false;
};
[[nodiscard]] ScaleResult output_scale(const mask::OutputStyle& style, const compat::CompatSettings& compat);

/// The twelve I2 fields of a BGNLIB or BGNSTR record ("Date records"): the
/// creation date `creation`, then the modification date, which is `now`
/// with magic-compat ON and time 0 with it OFF (Q-MASK-202), both in the
/// host zone or `zone` (Q-MASK-203).
[[nodiscard]] std::array<std::int16_t, 12> structure_dates(std::int64_t creation, std::int64_t now,
                                                           const compat::CompatSettings& compat,
                                                           std::string_view zone);

/// The prepare phase (exclusive lock; see the file comment).
[[nodiscard]] diag::Outcome<PreparedWrite> prepare_write(editor::EditorState& editor, db::CellDef& root,
                                                         const GdsWriteOptions& options);

/// The emit phase (shared lock; see the file comment).
[[nodiscard]] cmd::CommandResult emit_write(const PreparedWrite& prepared, const platform::Path& path,
                                            const mask::Progress* progress = nullptr);

/// prepare_write then emit_write. When the prepare phase fails reading the
/// subtree (GDS-WRITE-003), the output file is left empty, as Magic leaves
/// the file it opened before CalmaWrite; the other prepare failures (no
/// style, Q-MASK-219; refused scale, Q-MASK-225 OFF; abstract views,
/// Q-MASK-226 OFF) leave the file untouched.
[[nodiscard]] cmd::CommandResult write(editor::EditorState& editor, db::CellDef& root, const platform::Path& path,
                                       const GdsWriteOptions& options, const mask::Progress* progress = nullptr);

/// The merged polygons of a mask plane (`gds merge yes`; Magic's merge and
/// collinear passes, slits included, Q-MASK-217), in output order.
[[nodiscard]] std::vector<mask::Polygon> merge_polygons(const mask::MaskPlane& plane);

}  // namespace hefesto::gds
