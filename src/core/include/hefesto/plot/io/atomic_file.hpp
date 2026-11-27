// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Atomic plot output files and the default extension rule (capability
// plot-output "Plot output safety and limits"; Q-MISC-53 fix, Q-MISC-55
// compat; openspec/changes/m10-plot-route/design.md D5, D7, "Area
// contracts" PLOTCMD). Every plot writer writes through an AtomicFile: a
// temporary file `.<name>.hefesto-tmp-<pid>-<n>` in the target directory,
// renamed onto the target by commit() and removed by the destructor when
// the plot is interrupted or fails.
//
// Owner: PLOTCMD.

#pragma once

#include <filesystem>
#include <fstream>
#include <string_view>

#include "hefesto/diag/outcome.hpp"

namespace hefesto::plot {

class AtomicFile {
 public:
  AtomicFile() = default;
  AtomicFile(std::filesystem::path target, std::filesystem::path temporary);
  AtomicFile(AtomicFile&& other) noexcept;
  AtomicFile& operator=(AtomicFile&& other) noexcept;
  AtomicFile(const AtomicFile&) = delete;
  AtomicFile& operator=(const AtomicFile&) = delete;
  /// Removes the temporary file unless commit() succeeded.
  ~AtomicFile();

  /// The stream the writer writes to.
  [[nodiscard]] std::ostream& stream() noexcept { return out_; }
  [[nodiscard]] const std::filesystem::path& target() const noexcept { return target_; }
  /// Flushes, syncs and renames the temporary file onto the target.
  [[nodiscard]] diag::Status commit();

 private:
  void discard() noexcept;

  std::filesystem::path target_;
  std::filesystem::path temporary_;
  std::ofstream out_;
  bool committed_ = false;
};

/// Opens a temporary file next to `target`.
[[nodiscard]] diag::Outcome<AtomicFile> open_atomic(const std::filesystem::path& target);

/// `path` with `ext` (e.g. ".ps") appended when it has no extension:
/// magic-compat ON looks for a dot anywhere in the path as Magic does, OFF
/// only in the last component (Q-MISC-55, branch point of design D5).
[[nodiscard]] std::filesystem::path with_default_extension(const std::filesystem::path& path, std::string_view ext,
                                                           bool magic_compat);

}  // namespace hefesto::plot
