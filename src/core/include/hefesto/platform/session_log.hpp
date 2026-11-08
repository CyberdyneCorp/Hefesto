// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Per-user app storage and the per-session rotating log, shared by every host
// (hefesto-cli and the desktop shell). Capability hefesto-diagnostics,
// requirement "Diagnostic presentation and logging": each session keeps a
// rotating log file within the app's storage.

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/platform/logger.hpp"
#include "hefesto/platform/services.hpp"

namespace hefesto::platform {

/// The per-user app storage directory (portable default; uses only the
/// environment):
///   macOS  ~/Library/Application Support/Hefesto
///   Linux  $XDG_STATE_HOME/hefesto, else ~/.local/state/hefesto
///   other  <temp>/hefesto
[[nodiscard]] Path default_storage_dir();

/// Rotation used for session logs: 1 MiB per file, 5 files.
inline constexpr RotationPolicy kSessionLogRotation{.max_bytes = 1024 * 1024, .max_files = 5};

struct SessionLog {
  Logger* logger = nullptr;                // the installed logger, owned by the services
  std::optional<diag::Diagnostic> warning;  // set when the log could not be opened
};

/// Creates `directory` and installs a RotatingFileLogger writing
/// `<directory>/<base_name>.log` into `services`. When the directory cannot be
/// created, the services keep their NullLogger and the result carries a
/// warning with `warning_code`, so a missing log never fails the session.
/// Call it before taking services.view().
[[nodiscard]] SessionLog install_session_log(PortableServices& services, const Path& directory,
                                             std::string_view base_name, std::string_view warning_code);

}  // namespace hefesto::platform
