// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Curated-pack mode (openspec/changes/m4-scripting/design.md D7;
// capability scripting-pdk-integration "PDKs without bundled scripts in
// curated-pack mode", hefesto-scripting "App Store fallback for iPadOS";
// Q-SCRIPT-46, D-30): when scripts may come only from curated packs, a PDK
// without bundled scripts is refused; there is no declarative profile.
// Unit-tested now, used by the iPad shell in M14.
//
// Owner: PDKINT (design D2).

#pragma once

#include "hefesto/cmd/command.hpp"
#include "hefesto/script/types.hpp"

namespace hefesto::script {

class CuratedPackPolicy {
 public:
  explicit CuratedPackPolicy(bool curated_only) : curated_only_(curated_only) {}

  /// ok when `pdk` may be used (in curated-pack mode with a SCRIPT-PDK-009
  /// note: the bundled curated scripts are used, an imported pack's own
  /// scripts are not executed); failed with SCRIPT-PDK-005 ("only curated
  /// PDKs run in this mode") when curated-pack mode is on and the PDK's
  /// startup and device scripts are not among the bundled curated scripts.
  /// The caller then neither activates the PDK nor runs any of its scripts;
  /// no declarative substitute exists (D-30).
  [[nodiscard]] cmd::CommandResult check(const PdkIdentity& pdk, bool has_bundled_scripts) const;

  [[nodiscard]] bool curated_only() const noexcept { return curated_only_; }

 private:
  bool curated_only_;
};

}  // namespace hefesto::script
