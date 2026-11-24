// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The folder of the running executable, as the operating system reports it
// (_NSGetExecutablePath on macOS, /proc/self/exe on Linux). hefesto-cli and
// the desktop app derive their default installation root from it
// (capability app-startup "Installation root and search paths", M8-D10);
// argv[0] is not used, since a program started from PATH gets a bare name
// that would resolve against the working directory.

#pragma once

#include "hefesto/platform/file_system.hpp"

namespace hefesto::platform_posix {

/// The absolute, canonical folder holding the running executable; empty
/// when the platform does not say.
[[nodiscard]] platform::Path executable_dir();

}  // namespace hefesto::platform_posix
