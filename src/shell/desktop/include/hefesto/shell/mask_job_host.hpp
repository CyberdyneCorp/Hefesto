// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The desktop host of long GDS and CIF reads and writes (capability
// mask-hierarchical-generation "Progress reporting and interruptibility",
// Q-MASK-414; openspec/changes/m5-mask-gds-cif/design.md D4, D16; tasks.md
// 12.2). It reuses the long-running script pattern of m4 D5 (the Tcl
// console's running header, the canvas Interrupt card after 2 s), so no new
// board is needed:
//
//   - a read, and a whole write (run_job: MaskSession::read_host and
//     write_host), run on the command thread, which holds the database lock
//     exclusively; the mask::Progress they get pumps a frame (rate-limited
//     to the pump interval) whenever the reader reports or the write's
//     subtree loading ticks (Progress::tick), and its cancel flag is the
//     Interrupt card's;
//   - a write's emit phase (run_emit: MaskSession::emit_host) runs on a
//     worker through platform::run_on_stack(platform::kDeepRecursionStackBytes),
//     because the generators and writers recurse once per hierarchy level
//     (up to 1024 levels), while the calling thread pumps frames. The
//     command's exclusive hold covers the worker (design "Round 5 contract
//     updates"): the calling thread only pumps frames, which read, until the
//     emit ends, and the background DRC waits. Called without that hold
//     (tests), the worker holds the lock shared;
//   - console lines typed while a job runs wait as behind a script
//     (TclConsoleSession::set_external_busy), so no edit reaches the file;
//   - request_cancel() (the Interrupt card, Ctrl-C) sets the Progress's
//     cancel flag; a cancelled write leaves no output file (the writers'
//     temporary file, mask::OutputFile).
//
// The pump runs on the calling thread only; it must draw and take the
// Interrupt input, never run a command (the window holds a
// ScriptInputBlock while it draws, as for scripts). Toolkit-free and
// unit-tested; the worker never touches Raven.
//
// Owner: SHELL (openspec/changes/m5-mask-gds-cif/design.md D2, "the progress
// card's GDS/CIF host").

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/db/lock.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/shell/script_consent_model.hpp"

namespace hefesto::shell {

/// One phase of a long read or write, given the Progress it reports to and
/// polls for cancellation.
using MaskWork = std::function<cmd::CommandResult(const mask::Progress*)>;

enum class MaskJobKind : unsigned char { read, write };

/// What a running job shows.
struct MaskJobState {
  MaskJobKind kind = MaskJobKind::write;
  std::string label;  // the file, as the card names it ("chip.gds")
  std::chrono::steady_clock::duration elapsed{};
  std::optional<double> fraction;  // the last reported fraction in [0, 1]
  bool cancel_requested = false;
};

/// Magic's informative "Completed N%" (CIFGenSubcells), N rounded down.
[[nodiscard]] std::string completed_text(double fraction);

/// The Interrupt card of a job that ran for kLongRunning: "Writing chip.gds
/// · 14 s" / "Reading chip.gds · 3 s", with "Completed 42%" once a fraction
/// was reported and "Stopping: no file is written" after Interrupt.
[[nodiscard]] std::optional<InterruptCardView> mask_job_card(const MaskJobState& state);

class MaskJobHost {
 public:
  explicit MaskJobHost(db::DatabaseLock& lock);
  ~MaskJobHost();
  MaskJobHost(const MaskJobHost&) = delete;
  MaskJobHost& operator=(const MaskJobHost&) = delete;

  /// Called on the job's calling thread about every `interval` while a job
  /// runs (the shell draws a frame there).
  void set_pump(std::function<void()> pump, std::chrono::milliseconds interval = std::chrono::milliseconds{50});

  /// Runs `work` on the calling thread as a job of `kind` (see the file
  /// comment); nested inside a running job it just runs.
  cmd::CommandResult run_job(MaskJobKind kind, std::string_view label, const MaskWork& work);
  cmd::CommandResult run_read(std::string_view label, const MaskWork& read) {
    return run_job(MaskJobKind::read, label, read);
  }
  /// Runs `emit` on a deep-stack worker while the calling thread pumps;
  /// returns its result and rethrows its exception. Inside run_job it is
  /// part of that job.
  cmd::CommandResult run_emit(std::string_view label, const MaskWork& emit);

  /// Called on the calling thread once a job ended (the window resumes the
  /// DRC commits it held: DrcPump::resume).
  void set_on_end(std::function<void()> on_end) { on_end_ = std::move(on_end); }
  /// Interrupt: true when a job runs (its Progress is then cancelled).
  bool request_cancel() noexcept;

  [[nodiscard]] bool busy() const noexcept { return depth_.load(std::memory_order_acquire) > 0; }
  /// The running job, or nullopt.
  [[nodiscard]] std::optional<MaskJobState> state() const;

 private:
  class Job;
  struct Shared;
  void pump_if_due();
  void job_ended();  // the end callback

  db::DatabaseLock& lock_;
  std::shared_ptr<Shared> shared_;
  std::atomic<int> depth_{0};
  std::function<void()> pump_;
  std::chrono::milliseconds interval_{50};
  std::chrono::steady_clock::time_point last_pump_{};
  std::function<void()> on_end_;
};

}  // namespace hefesto::shell
