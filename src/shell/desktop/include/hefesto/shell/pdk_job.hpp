// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A long PDK operation of the PDK manager panel (`pdk install ... -progress`,
// `pdk import ... -progress`) run off the UI thread (capability
// hefesto-pdk-manager "Headless and scripted operation": the panel runs the
// same `pdk` commands as scripts). The job runs the command through a
// Dispatcher of its own (a headless command session with the PDK commands,
// a copy of the editor's layered settings and the session's storage), so it
// never touches the UI session's state concurrently. The command's
// PDK-PROGRESS-001 diagnostics reach the job live through a Dispatcher live
// sink (Dispatcher::add_live_sink), downloads included, with the asset
// count known; the panel polls the progress snapshot every frame, and the
// finished Invocation and CommandResult are handed back to the UI thread,
// which records them in the session's command log and Console.
// Toolkit-free and unit-tested.
//
// Owner: SHELL (openspec/changes/m1-core-viewer/design.md, D2 and D6).

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "hefesto/app/settings_stack.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/pdk/commands.hpp"
#include "hefesto/pdk/manager.hpp"
#include "hefesto/platform/services.hpp"

namespace hefesto::shell {

/// How the panel shows one progress event: the bar's fill (0..1, the asset
/// parts weighted equally, bytes within the current part) and its text,
/// e.g. "Downloading sky130_fd_sc_hd.tar.zst (2/3) 45%" (state is text +
/// bar, never color only).
struct PdkProgressView {
  double fraction = 0.0;
  std::string text;
};
[[nodiscard]] PdkProgressView pdk_progress_view(const pdk::InstallProgress& progress);

/// The command line with `-progress` appended for install and import
/// (other lines unchanged).
[[nodiscard]] std::string with_progress(std::string_view line);

class PdkJob {
 public:
  struct Setup {
    platform::PortableOptions options;  // the job session's services (storage root)
    app::SettingsStack settings;        // a copy of the editor's settings (pdk-root, ...)
    pdk::CommandHosts hosts;            // empty members: the defaults
  };
  struct Finished {
    cmd::Invocation invocation;
    cmd::CommandResult result;
  };

  PdkJob();
  ~PdkJob();  // waits for a running command
  PdkJob(const PdkJob&) = delete;
  PdkJob& operator=(const PdkJob&) = delete;

  /// Starts `line` (one `pdk` command, origin `gui`) on a worker thread.
  /// False when a job is running or the line is not one command.
  bool start(std::string_view line, Setup setup);
  [[nodiscard]] bool running() const;
  /// The latest progress event into `out`; false before the first one.
  bool progress(pdk::InstallProgress& out) const;
  /// The finished command into `out`, once (UI thread); false while none.
  bool take_finished(Finished& out);
  /// Blocks until the running command ends (tests, shutdown).
  void wait();

 private:
  struct State;
  std::shared_ptr<State> state_;
};

}  // namespace hefesto::shell
