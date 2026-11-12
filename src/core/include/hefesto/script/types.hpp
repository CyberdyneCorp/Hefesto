// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared script types (openspec/changes/m4-scripting/design.md D1/D4): the
// identity of an installed PDK version (consent, trust, the cross-PDK
// helper warning, native generator registration) and the records the
// script layer reports through script::ScriptEvents.
//
// Owner: INTEGRATION (shared file); workstreams request changes through
// their open items.

#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::script {

/// One installed PDK version, as consent and trust are recorded
/// ("Consent before implicit script execution": per PDK and version).
struct PdkIdentity {
  std::string family;   // "sky130", "gf180mcu", "ihp-sg13g2"
  std::string variant;  // "sky130A", "gf180mcuD", "ihp-sg13g2"
  std::string version;  // full hash of the ciel release (pdk::Provenance::version)

  /// "family/variant@version" (used in diagnostics and the trust store).
  [[nodiscard]] std::string to_string() const;

  friend bool operator==(const PdkIdentity&, const PdkIdentity&) = default;
  friend auto operator<=>(const PdkIdentity&, const PdkIdentity&) = default;
};

/// Where a script statement is: the file being sourced (empty for typed or
/// `-c` text) and its line (from Tcl's `info frame`; 0 = unknown).
using ScriptLocation = diag::SourceLocation;

/// An operation the sandbox refused (design D5): the Tcl error names
/// `operation`; SCRIPT-SANDBOX-* diagnostics carry `location`.
struct DeniedOperation {
  std::string operation;  // "exec", "socket", "open", "file delete", "cd", "source", ...
  std::string target;     // the normalized path or command line; empty when none
  ScriptLocation location;
  /// The script absorbed the Tcl error (`catch`, `try`) and went on: the
  /// evaluation did not end with this denial's error. Known when the
  /// top-level evaluation ends, which is when the event is published
  /// (board M4-Script-Consent: the row is marked "caught"). False for a
  /// denial no command raised as an error (a path the filesystem layer
  /// refused under a command that ignores it, such as `file exists`).
  bool caught = false;

  friend bool operator==(const DeniedOperation&, const DeniedOperation&) = default;
};

/// A Tk widget statement of a PDK menu callback that the interceptor could
/// not map and skipped (Q-SCRIPT-42; design D7).
struct SkippedStatement {
  std::string statement;  // the command words as evaluated
  ScriptLocation location;
  /// The fully qualified procedure the statement ran in ("::sky130::addtechmenu";
  /// board M4-Menu-Bar note 11); empty at a script's top level.
  std::string procedure{};

  friend bool operator==(const SkippedStatement&, const SkippedStatement&) = default;
};

/// A `magic::` procedure redefined by a script of another PDK (Q-SCRIPT-41).
struct HelperRedefinition {
  std::string procedure;  // fully qualified, e.g. "::magic::spice2float"
  PdkIdentity first;      // the PDK whose script defined it first
  PdkIdentity second;     // the PDK whose script redefined it
  ScriptLocation location;

  friend bool operator==(const HelperRedefinition&, const HelperRedefinition&) = default;
};

/// A request to run a PDK script that needs consent (design D7).
struct ConsentRequest {
  PdkIdentity pdk;
  std::string script;  // absolute path of the startup or <tech>.magicrc file
  bool implicit = false;  // true: technology auto-discovery's <tech>.magicrc

  friend bool operator==(const ConsentRequest&, const ConsentRequest&) = default;
};

}  // namespace hefesto::script
