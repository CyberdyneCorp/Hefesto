// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// AtomicOutput: writes an extraction output file completely or not at all
// (Q-EXTRACT-27, resolution: fix; capability extract-commands "Output path
// and file location", extresist-output "Overlay file grammar and order";
// openspec/changes/m7-extraction/design.md D12). The target is resolved
// as ExtFileOpen does (ExtLocalPath, then the directory of cd_file, then
// the retry with the path stripped); the temporary file is created in the
// target's directory and renamed over it on commit(). A symbolic link is
// resolved and its target replaced; a directory where no temporary file
// can be created is written in place, as Magic does. Used by EXTCMD for
// `.ext` and by RESCMD for `.res.ext`, `.res.lump` and `.fh`.
//
// Scaffold state (tasks.md 3.3): open() reports CORE-NOTIMPL-008 until
// EXTNODE lands it (tasks.md 5.5).
//
// Owner: INTEGRATION (this header); EXTNODE (the implementation).

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/diag/outcome.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::extract {

class AtomicOutput {
 public:
  /// Resolves `<cell><suffix>` (suffix ".ext", ".res.ext", ".res.lump",
  /// ".fh") and opens the temporary file.
  [[nodiscard]] static diag::Outcome<AtomicOutput> open(const cell::LookupContext& ctx, const db::CellDef& cell,
                                                        std::string_view suffix);

  /// Appends text to the temporary file.
  [[nodiscard]] diag::Status write(std::string_view text);
  /// Renames the temporary file over the target.
  [[nodiscard]] diag::Status commit();
  /// Removes the temporary file; the target is unchanged.
  void abandon() noexcept;

  /// The resolved target path.
  [[nodiscard]] const std::string& target() const noexcept { return target_; }

 private:
  std::string target_;
  std::optional<std::string> temporary_;
};

}  // namespace hefesto::extract
