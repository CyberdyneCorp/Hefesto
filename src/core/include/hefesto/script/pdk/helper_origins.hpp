// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Which PDK defined each `magic::` procedure (openspec/changes/m4-scripting/
// design.md D7; capability scripting-api "Script-defined procedures in the
// engine namespace", scripting-pdk-integration "PDK helper procedures in
// the engine namespace"; Q-SCRIPT-41): `proc` inside a `source` of a file
// under a PDK's libs.tech/magic records that PDK; a redefinition from
// another PDK's file is reported (SCRIPT-PDK-003, ScriptEvents) and takes
// effect, as in Tcl. The PDK helper names (`magic::u2i`, `spice2float`, ...)
// are never predefined: the PDK scripts define them.
//
// Owner: PDKINT (design D2).

#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "hefesto/cmd/command.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/script/events.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/script/types.hpp"

namespace hefesto::script {

class HelperOriginTracker {
 public:
  /// Records that `pdk` defined `procedure` (fully qualified, e.g.
  /// "::magic::i2u"); returns the redefinition when a different PDK (or PDK
  /// version) defined it before. The new PDK becomes the origin.
  [[nodiscard]] std::optional<HelperRedefinition> record(std::string_view procedure, const PdkIdentity& pdk,
                                                         const ScriptLocation& location);
  /// The PDK that defined `procedure`, or nullopt.
  [[nodiscard]] std::optional<PdkIdentity> origin(std::string_view procedure) const;

 private:
  std::map<std::string, PdkIdentity, std::less<>> origins_;
};

/// The SCRIPT-PDK-003 warning for `redefinition`: names the procedure and
/// both PDKs.
[[nodiscard]] diag::Diagnostic redefinition_warning(const HelperRedefinition& redefinition);

/// Installs the hook in `interp`: after every successful `proc` whose
/// procedure lands in `::magic::` while a file under a PDK's
/// `libs.tech/magic` is being sourced (`info script`), the PDK is recorded
/// in `tracker`; a redefinition is published on `events` and reported to
/// `sink` (may be nullptr). The trackers and sink must outlive `interp`
/// (they are members of the same editor::EditorState::script).
[[nodiscard]] cmd::CommandResult install_helper_origin_hook(Interpreter& interp, HelperOriginTracker& tracker,
                                                            ScriptEvents& events, diag::DiagnosticSink* sink);

}  // namespace hefesto::script
