// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Commands of the PDK area, registered by cmd::register_all_commands():
//   pdk root|list|remote|updates|install|enable|uninstall|remove-library|pin|
//       info|export|import ...
// (capability hefesto-pdk-manager "Headless and scripted operation"). The
// same commands back the desktop PDK manager panel and hefesto-cli.
//
// Owner: PDK (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/pdk/manager.hpp"
#include "hefesto/platform/services.hpp"

namespace hefesto::pdk {

/// Host mechanisms the `pdk` command uses. An empty member takes the
/// default: a curl Fetcher and the default Decompressor when the platform
/// can run processes (an unsupported Fetcher otherwise), and the local
/// install host (std::filesystem symbolic links, the process environment).
struct CommandHosts {
  std::function<std::unique_ptr<Fetcher>(platform::PlatformServices&)> fetcher;
  std::function<std::unique_ptr<Decompressor>(platform::PlatformServices&)> decompressor;
  std::function<std::unique_ptr<InstallHost>()> install_host;
};

/// Adds the PDK commands to `registry`. Returns false if a name was taken.
bool register_pdk_commands(cmd::CommandRegistry& registry, CommandHosts hosts = {});

/// Install progress through the command (front ends: the PDK manager panel).
/// `pdk install ... -progress` and `pdk import ... -progress` emit, through
/// the dispatcher's diagnostic sinks while the command runs, info
/// diagnostics with code PDK-PROGRESS-001 (codes::kProgress) whose text is
///   phase=<download|verify|unpack|finalize> part=<i>/<n> bytes=<b> total=<t> asset=<file name>
/// (asset last; it may be empty for finalize). Downloads are throttled to
/// whole percent steps (1 MiB steps when the total is unknown); every phase
/// change is emitted. During a download the diagnostic may be emitted from
/// the fetcher's worker thread (see InstallProgressFn); the command thread
/// is blocked meanwhile, so emissions never overlap.
[[nodiscard]] std::string format_install_progress(const InstallProgress& progress);
/// The event of a PDK-PROGRESS-001 diagnostic; std::nullopt for any other
/// diagnostic or a malformed text.
[[nodiscard]] std::optional<InstallProgress> parse_install_progress(const diag::Diagnostic& diagnostic);

}  // namespace hefesto::pdk
