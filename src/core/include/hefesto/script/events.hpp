// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// script::ScriptEvents: the observer list of the script layer
// (openspec/changes/m4-scripting/design.md D4). Published:
//   - denied operations of the sandbox (TCL; design D5),
//   - skipped widget statements of PDK menu callbacks (PDKINT; Q-SCRIPT-42),
//   - cross-PDK helper redefinitions (PDKINT; Q-SCRIPT-41),
//   - consent requests (PDKINT; design D7), before ConsentHost::ask,
//   - menu-model changes (PDKINT; the native menu bar rebuilds).
// Consumers: the desktop shell (console, consent and denial states, menu
// bar), the CLI (diagnostics) and tests.
//
// Delivery is synchronous on the publishing thread, which is the command
// thread. Observers are not owned and must be removed before they are
// destroyed (the same contract as drc::DrcEvents).
//
// Owner: INTEGRATION (shared file); published by TCL and PDKINT.

#pragma once

#include <cstddef>
#include <vector>

#include "hefesto/script/types.hpp"

namespace hefesto::script {

class ScriptObserver {
 public:
  ScriptObserver() = default;
  ScriptObserver(const ScriptObserver&) = delete;
  ScriptObserver& operator=(const ScriptObserver&) = delete;
  virtual ~ScriptObserver() = default;

  virtual void on_denied(const DeniedOperation& /*operation*/) {}
  virtual void on_skipped_statement(const SkippedStatement& /*statement*/) {}
  virtual void on_helper_redefined(const HelperRedefinition& /*redefinition*/) {}
  virtual void on_consent_requested(const ConsentRequest& /*request*/) {}
  virtual void on_menu_model_changed() {}
};

class ScriptEvents {
 public:
  /// Adds `observer` once (a second add is ignored).
  void add(ScriptObserver& observer);
  void remove(const ScriptObserver& observer);
  [[nodiscard]] std::size_t size() const noexcept { return observers_.size(); }

  void publish_denied(const DeniedOperation& operation);
  void publish_skipped_statement(const SkippedStatement& statement);
  void publish_helper_redefined(const HelperRedefinition& redefinition);
  void publish_consent_requested(const ConsentRequest& request);
  void publish_menu_model_changed();

 private:
  std::vector<ScriptObserver*> observers_;
};

}  // namespace hefesto::script
