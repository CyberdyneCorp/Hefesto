// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Progress reporting and the temporary output file of a long write
// (capability mask-hierarchical-generation "Progress reporting and
// interruptibility", Q-MASK-414, platform; design D8, D16):
// - ProgressThrottle reports the fraction of interaction squares done to a
//   mask::Progress at most about every five seconds, where Magic prints
//   `Completed N%` (CIFGenSubcells): only once more than one square is
//   done, when the fraction grew by more than 1 % or the last square is
//   done, and 100 % at the end when anything was reported;
// - OutputFile is the temporary file a GDS or CIF write goes to, renamed
//   onto the target when the write succeeds (or fails where Magic leaves a
//   terminated file, Q-MASK-226 ON) and removed when it is cancelled, so a
//   cancelled write leaves no file.
//
// Owner: GEN (openspec/changes/m5-mask-gds-cif/design.md D2); the writers
// (GDSW, CIF) and the shell host them.

#pragma once

#include <chrono>
#include <cstdint>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/platform/clock.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::mask {

class ProgressThrottle {
 public:
  /// Magic's SigSetTimer(5).
  static constexpr std::chrono::seconds kInterval{5};

  /// Reports to `progress` (nullptr: never) timed by `clock` (nullptr: the
  /// steady clock).
  ProgressThrottle(const Progress* progress, platform::Clock* clock,
                   std::chrono::nanoseconds interval = kInterval);

  /// `done` of `total` squares are done.
  void step(std::uint64_t done, std::uint64_t total);
  /// The end of the pass: reports 1.0 when anything was reported.
  void finish();

  /// True once a fraction was reported (Magic's `longTime`).
  [[nodiscard]] bool reported() const noexcept { return reported_; }

 private:
  [[nodiscard]] std::chrono::steady_clock::time_point now() const;

  const Progress* progress_;
  platform::Clock* clock_;
  std::chrono::nanoseconds interval_;
  std::chrono::steady_clock::time_point deadline_;
  double last_ = 0.0;
  bool reported_ = false;
};

class OutputFile {
 public:
  /// The temporary file `<dir>/.<name>.hefesto-tmp` next to `target`.
  OutputFile(platform::FileSystem& fs, platform::Path target);
  /// Removes the temporary file unless it was committed or discarded.
  ~OutputFile();
  OutputFile(const OutputFile&) = delete;
  OutputFile& operator=(const OutputFile&) = delete;

  [[nodiscard]] const platform::Path& target() const noexcept { return target_; }
  [[nodiscard]] const platform::Path& temporary() const noexcept { return temporary_; }

  /// Renames the temporary file onto the target (replacing it).
  [[nodiscard]] diag::Status commit();
  /// Removes the temporary file; the target is untouched.
  void discard();

 private:
  platform::FileSystem& fs_;
  platform::Path target_;
  platform::Path temporary_;
  bool done_ = false;
};

}  // namespace hefesto::mask
