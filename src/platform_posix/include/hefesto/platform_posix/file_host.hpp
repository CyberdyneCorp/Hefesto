// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A cell FileHost for POSIX hosts: the core's local host (std::filesystem)
// plus the crash-recovery facilities it cannot provide portably — file
// ownership (st_uid against the real user id), the status-change time
// (st_ctime) and process liveness without a signal effect (kill(pid, 0)),
// used by `crash recover` (cell-io-safety "Crash recovery", "Import of
// Magic crash backups").
//
// Install: `editor.cell_session.host = hefesto::platform_posix::make_file_host();`
//
// Owner: CELL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <memory>

#include "hefesto/cell/host.hpp"

namespace hefesto::platform_posix {

[[nodiscard]] std::unique_ptr<cell::FileHost> make_file_host();

}  // namespace hefesto::platform_posix
