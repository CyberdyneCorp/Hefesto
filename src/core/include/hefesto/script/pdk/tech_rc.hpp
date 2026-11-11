// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `<tech>.magicrc` step of technology auto-discovery
// (openspec/changes/m4-scripting/design.md D7; capability
// scripting-pdk-integration "Technology auto-discovery from layout files";
// Q-SCRIPT-10): run behind consent like any PDK script.
//
// Owner: PDKINT (design D2).

#pragma once

#include "hefesto/cmd/command.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/script/types.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::script {

class ConsentHost;

/// Runs `rc_file` of `pdk` in the project's interpreter after consent
/// (`consent` nullptr: the session TrustStore's host). ok when the file was
/// evaluated (its Tcl errors are ignored, as Magic's Tcl_EvalFile result
/// is); failed with SCRIPT-TRUST-001 when consent was declined (the caller
/// then opens the layout read-only with the `minimum` technology), or
/// SCRIPT-PDK-008 without an interpreter. Consent notes (SCRIPT-TRUST-*)
/// are in the result's diagnostics.
[[nodiscard]] cmd::CommandResult run_tech_magicrc(editor::EditorState& editor, const PdkIdentity& pdk,
                                                  const platform::Path& rc_file, ConsentHost* consent);

}  // namespace hefesto::script
