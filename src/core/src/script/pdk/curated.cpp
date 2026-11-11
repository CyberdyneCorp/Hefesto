// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Curated-pack mode (Q-SCRIPT-46, D-30; scripting-pdk-integration "PDKs
// without bundled scripts in curated-pack mode").

#include "hefesto/script/pdk/curated.hpp"

namespace hefesto::script {

cmd::CommandResult CuratedPackPolicy::check(const PdkIdentity& pdk, bool has_bundled_scripts) const {
  if (!curated_only_) {
    return cmd::CommandResult::success();
  }
  if (has_bundled_scripts) {
    cmd::CommandResult used = cmd::CommandResult::success();
    used.diagnostics.push_back(diag::make("SCRIPT-PDK-009", "Curated-pack mode: " + pdk.variant +
                                                                " uses its bundled curated scripts; the scripts of an "
                                                                "imported pack are not executed."));
    return used;
  }
  return cmd::CommandResult::failure(
      diag::make("SCRIPT-PDK-005", "Only curated PDKs run in this mode: " + pdk.variant +
                                       " has no bundled scripts, so it is not activated and none of its scripts run."));
}

}  // namespace hefesto::script
