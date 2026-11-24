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
// Write order: open() creates the temporary file (`.<name>.hefesto-tmp`
// next to the target), write() appends to it, commit() renames it over
// the target, abandon() (and the destructor of an uncommitted output)
// removes it. In the in-place fallback write() appends to the target,
// truncated by the first write, and abandon() leaves what was written, as
// Magic does.
//
// Owner: INTEGRATION (this header); EXTNODE (the implementation).

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::extract {

/// Added in M7 R3: where ExtFileOpen looks first.
struct OutputLocation {
  /// ExtLocalPath (`extract path`): `.` writes `<cell><suffix>` in the
  /// current directory; another directory is created when missing.
  std::optional<std::string> local_path;
  /// The directory relative names resolve against (Magic's current
  /// directory); empty = the process's current directory.
  std::string current_directory;
};

class AtomicOutput {
 public:
  AtomicOutput() = default;
  AtomicOutput(AtomicOutput&& other) noexcept;
  AtomicOutput& operator=(AtomicOutput&& other) noexcept;
  AtomicOutput(const AtomicOutput&) = delete;
  AtomicOutput& operator=(const AtomicOutput&) = delete;
  /// An output neither committed nor abandoned is abandoned.
  ~AtomicOutput();

  /// Resolves `<cell><suffix>` (suffix ".ext", ".res.ext", ".res.lump",
  /// ".fh") and opens the temporary file.
  [[nodiscard]] static diag::Outcome<AtomicOutput> open(const cell::LookupContext& ctx, const db::CellDef& cell,
                                                        std::string_view suffix);
  /// Added in M7 R3: open() with ExtLocalPath and the current directory.
  [[nodiscard]] static diag::Outcome<AtomicOutput> open(const cell::LookupContext& ctx, const db::CellDef& cell,
                                                        std::string_view suffix, const OutputLocation& where);

  /// Appends text to the temporary file.
  [[nodiscard]] diag::Status write(std::string_view text);
  /// Renames the temporary file over the target.
  [[nodiscard]] diag::Status commit();
  /// Removes the temporary file; the target is unchanged.
  void abandon() noexcept;

  /// The resolved target path (a symbolic link stays the link's path; the
  /// file it points to receives the output).
  [[nodiscard]] const std::string& target() const noexcept { return target_; }
  /// Added in M7 R3: the temporary file, when one is open.
  [[nodiscard]] const std::optional<std::string>& temporary() const noexcept { return temporary_; }
  /// Added in M7 R3: true when no temporary file could be created and the
  /// target is written in place.
  [[nodiscard]] bool in_place() const noexcept { return in_place_; }

 private:
  std::string target_;
  std::optional<std::string> temporary_;
  std::string destination_;  // target_ with symbolic links resolved
  platform::FileSystem* fs_ = nullptr;
  bool in_place_ = false;
  bool truncated_ = false;
};

}  // namespace hefesto::extract
