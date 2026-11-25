// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Long-lived worker processes (capability hefesto-architecture, requirement
// "Long-lived worker processes"; openspec/changes/m9-lvs-simulation design
// D9 "Platform service", Area contract PLATFORM).
//
// A WorkerHost starts a worker executable and returns a WorkerChannel that
// exchanges length-prefixed binary frames over the worker's standard input
// and output. The core declares the interface only: the hosts that create
// processes (hefesto-cli, the desktop shell) implement it with posix_spawn
// and pipes (src/cli/worker_host_posix.hpp); the core default reports it
// unsupported (iPadOS, scenario "No process creation").
//
// Wire format of one frame, in both directions: a 4-byte little-endian
// length N (1 <= N <= the channel's frame limit), then N bytes. What the
// bytes mean is the caller's protocol (sim/engine/frames.hpp for the
// simulation worker); the channel only delimits frames.
//
// Owner: PLATFORM.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/diag/outcome.hpp"

namespace hefesto::platform {

/// The largest frame a channel accepts by default, in bytes (64 MiB;
/// simulation-engine-embedding "Worker channel limits", Q-SIM-25).
inline constexpr std::uint32_t kDefaultMaxFrameBytes = 64U * 1024U * 1024U;

/// How to start a worker.
///
/// The host runs `program` with `args`, then `--job-dir=<job_dir>` when
/// `job_dir` is set and one `--readonly-root=<root>` per entry of
/// `readonly_roots`, so the worker can confine itself to them (design D9
/// "Confinement"). The working directory is `job_dir` when set. The
/// environment is exactly `env` (empty: an empty environment), so nothing of
/// the user's environment reaches the worker.
struct WorkerSpec {
  /// An absolute or relative path, or a bare name that the host resolves
  /// next to its own executable (the install's bin/ directory). Never looked
  /// up on PATH.
  std::string program;
  std::vector<std::string> args;
  std::vector<std::pair<std::string, std::string>> env;
  std::string job_dir;                      // the worker's only writable directory
  std::vector<std::string> readonly_roots;  // e.g. the PDK store and the installed libraries
  std::uint32_t max_frame_bytes = kDefaultMaxFrameBytes;  // received frames larger than this end the worker
};

/// How a worker ended.
struct WorkerExit {
  int status = 0;    // the exit status, when it exited (0-255)
  int signal = 0;    // the signal that killed it, or 0
  /// The channel ended the worker because it broke the frame format (a zero
  /// length, or a length above WorkerSpec::max_frame_bytes).
  bool frame_error = false;
  /// When frame_error is set: "empty frame", or "oversize frame of <n> bytes
  /// (limit <limit>)".
  std::string detail;

  [[nodiscard]] bool exited() const noexcept { return signal == 0; }
};

/// One running worker. Callbacks run on the channel's reader thread, one at a
/// time and in order: every frame the worker sent, then the exit. A frame
/// that arrives before on_frame() is set waits for it; the exit waits for
/// on_exit(). The callbacks must not destroy the channel.
///
/// Destroying the channel ends the worker (as terminate()) and joins the
/// reader thread.
class WorkerChannel {
 public:
  using FrameHandler = std::function<void(std::span<const std::byte> frame)>;
  using ExitHandler = std::function<void(const WorkerExit& exit)>;

  WorkerChannel() = default;
  WorkerChannel(const WorkerChannel&) = delete;
  WorkerChannel& operator=(const WorkerChannel&) = delete;
  virtual ~WorkerChannel() = default;

  /// Writes one frame (its length prefix is added here). Thread-safe. Fails
  /// with PLATFORM-WORKER-001 when the frame is empty or larger than the
  /// frame limit, and with PLATFORM-WORKER-002 when the worker has exited or
  /// was terminated, or the write fails; it never blocks on a dead worker.
  [[nodiscard]] virtual diag::Status send(std::span<const std::byte> frame) = 0;
  virtual void on_frame(FrameHandler handler) = 0;
  virtual void on_exit(ExitHandler handler) = 0;
  /// Ends the worker now (SIGKILL on POSIX). Idempotent; the exit callback
  /// still follows.
  virtual void terminate() = 0;
  /// The operating system's id of the worker process (0 when unknown).
  [[nodiscard]] virtual std::int64_t process_id() const noexcept = 0;
};

class WorkerHost {
 public:
  WorkerHost() = default;
  WorkerHost(const WorkerHost&) = delete;
  WorkerHost& operator=(const WorkerHost&) = delete;
  virtual ~WorkerHost() = default;

  /// False on platforms without process creation (iPadOS).
  [[nodiscard]] virtual bool supported() const noexcept = 0;
  /// Starts a worker. PLATFORM-UNSUPPORTED-001 when !supported(),
  /// PLATFORM-PROCESS-001 when the program cannot be started.
  [[nodiscard]] virtual diag::Outcome<std::unique_ptr<WorkerChannel>> start(const WorkerSpec& spec) = 0;
};

/// The core default: no worker service.
class UnsupportedWorkerHost final : public WorkerHost {
 public:
  [[nodiscard]] bool supported() const noexcept override { return false; }
  [[nodiscard]] diag::Outcome<std::unique_ptr<WorkerChannel>> start(const WorkerSpec& spec) override;
};

}  // namespace hefesto::platform
