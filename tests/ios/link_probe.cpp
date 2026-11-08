// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Links the core's main entry points into an iOS executable (see
// CMakeLists.txt). It runs one command script through the Dispatcher over the
// portable services, so the command API, diagnostics, platform services,
// settings and the Magic hash table all have to resolve at link time.

#include <cstdio>
#include <string>

#include "hefesto/cmd/builtins.hpp"
#include "hefesto/cmd/command_log.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/script.hpp"
#include "hefesto/diag/format.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/platform/session_log.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/version.hpp"

int main() {
  using namespace hefesto;
  platform::PortableServices services({.storage_root = platform::default_storage_dir(), .pdk_roots = {}});
  settings::ProjectSettings project;
  cmd::ScriptSession session{project};
  cmd::CommandRegistry registry;
  cmd::register_builtins(registry);
  cmd::CommandLog log;
  cmd::Dispatcher dispatcher(registry, services.view(), project, session);
  dispatcher.add_observer(log);
  const cmd::ScriptRun run =
      cmd::run_script(dispatcher, "version; hefesto::debug::hashorder 8 +a +b; setting", cmd::Origin::script);
  std::printf("hefesto %s: %zu commands, %s\n", std::string{version()}.c_str(), run.commands_run,
              run.error ? diag::format_text(*run.error).c_str() : "ok");
  return run.ok() ? 0 : 1;
}
