// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Commands of the PDK area, registered by cmd::register_all_commands():
//   pdk root|list|remote|install|enable|uninstall|remove-library|pin|info ...
// (capability hefesto-pdk-manager "Headless and scripted operation"). The
// same commands back the desktop PDK manager panel and hefesto-cli.
//
// Owner: PDK (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <functional>
#include <memory>

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

}  // namespace hefesto::pdk
