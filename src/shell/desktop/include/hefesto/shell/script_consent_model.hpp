// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Script consent, trust and the sandbox on the desktop (board
// docs/design/mockups/M4-Script-Consent.dc.html; openspec/changes/
// m4-scripting/design.md D7/D13; capability hefesto-security "Consent
// before implicit script execution", decision X-3).
//
//   - the consent prompt: what runs, its origin (ConsentRequest) and three
//     answers mapped to script::ConsentDecision; a script found outside the
//     PDK store has no Trust answer and Don't run as its default;
//   - DesktopConsentHost: the script::ConsentHost of the desktop, asking
//     once per PDK version in a session;
//   - the declined state: read-only chip, banner and Review scripts...;
//   - sandbox denials announced in the status bar;
//   - the long-running script card (after 2 s, with Interrupt).
//
// Toolkit-free and unit-tested; the Raven dialog is in consent_dialog.cpp.
//
// Owner: SHELL (design D2); ConsentHost and the trust store are PDKINT's.

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/platform/file_system.hpp"
#include "hefesto/script/events.hpp"
#include "hefesto/script/pdk/consent.hpp"
#include "hefesto/script/progress.hpp"
#include "hefesto/script/types.hpp"
#include "hefesto/shell/editing_models.hpp"
#include "hefesto/shell/view_models.hpp"

namespace hefesto::shell {

// --- consent prompt -------------------------------------------------------------------

/// What the prompt knows besides the request.
struct ConsentContext {
  std::string document;  // the file being opened ("ota_5t"); empty: the project
  /// The script lies in the managed PDK store (a Trust answer is offered).
  bool in_store = true;
  /// The store release's digest verified (pdk install checks SHA-256).
  bool digest_verified = false;
  /// The scripts the startup file sources next, when known ("sky130A.tcl").
  std::string then_runs;
};

struct ConsentAnswer {
  std::string label;  // "Don't run", "Trust sky130A 5740c4a", "Run once"
  script::ConsentDecision decision = script::ConsentDecision::decline;
  bool primary = false;  // the default: focused, Return gives it
};

struct ConsentPromptView {
  std::string title;  // "Run the sky130A PDK scripts?"
  std::string body;   // what runs and what the sandbox allows
  std::vector<std::pair<std::string, std::string>> origin;  // Script, PDK, Release, Then runs / Found via
  std::string script_path;  // the whole path (tooltip, selectable)
  std::vector<ConsentAnswer> answers;  // in display order
  bool warning = false;  // outside the PDK store: the ▲ release row

  /// The answer of Return (the primary one).
  [[nodiscard]] script::ConsentDecision default_decision() const;
  /// Escape always answers Don't run.
  [[nodiscard]] static script::ConsentDecision escape_decision() noexcept {
    return script::ConsentDecision::decline;
  }
};

/// The first 7 characters of a version hash ("5740c4a"), or "version unknown".
[[nodiscard]] std::string short_version(std::string_view version);

/// `path` shortened in the middle to at most `max` characters with "…".
[[nodiscard]] std::string middle_ellipsis(std::string_view path, std::size_t max);

[[nodiscard]] ConsentPromptView consent_prompt(const script::ConsentRequest& request, const ConsentContext& context);

/// The prompt as the shell's question dialog: one button per answer, the
/// default answer primary. `run_dialog`'s result (answer index + 1, 0 when
/// dismissed) maps back through consent_decision().
[[nodiscard]] PromptDialog consent_dialog(const ConsentPromptView& view);
[[nodiscard]] script::ConsentDecision consent_decision(const ConsentPromptView& view, int dialog_result) noexcept;

// --- the desktop ConsentHost ------------------------------------------------------------

/// Asks through `ask` (the dialog) once per PDK version in a session: a
/// second script of the same version (or a second window opening it)
/// reuses the answer. Add it to the session's ScriptEvents so that the
/// prompt knows an auto-discovered script. `in_store` tells whether a script path lies in the
/// PDK store. forget() drops an answer (Review scripts...).
class DesktopConsentHost final : public script::ConsentHost, public script::ScriptObserver {
 public:
  using Ask = std::function<script::ConsentDecision(const ConsentPromptView&)>;
  using InStore = std::function<bool(const platform::Path&)>;

  DesktopConsentHost(Ask ask, InStore in_store) : ask_(std::move(ask)), in_store_(std::move(in_store)) {}

  [[nodiscard]] script::ConsentDecision ask(const script::PdkIdentity& pdk, const platform::Path& script) override;
  /// ScriptEvents: the request about to be asked (it carries `implicit`).
  void on_consent_requested(const script::ConsentRequest& request) override { pending_ = request; }
  /// Sets what the next prompts show (the document being opened).
  void set_document(std::string document) { document_ = std::move(document); }
  void forget(const script::PdkIdentity& pdk) { answers_.erase(pdk); }
  /// The last answer given for `pdk` in this session.
  [[nodiscard]] std::optional<script::ConsentDecision> answer(const script::PdkIdentity& pdk) const;
  /// The script last asked about for `pdk` (Review scripts... asks again).
  [[nodiscard]] std::optional<platform::Path> script_of(const script::PdkIdentity& pdk) const;
  /// The PDK whose scripts were declined last (nullopt: none declined).
  [[nodiscard]] const std::optional<script::PdkIdentity>& declined() const noexcept { return declined_; }
  /// Review scripts... said yes but the layout could not be reopened: the
  /// PDK counts as declined again (the core's TrustStore declines it too).
  void restore_decline(const script::PdkIdentity& pdk) { declined_ = pdk; }

 private:
  Ask ask_;
  InStore in_store_;
  std::string document_;
  std::map<script::PdkIdentity, script::ConsentDecision> answers_;
  std::map<script::PdkIdentity, platform::Path> scripts_;
  std::optional<script::PdkIdentity> declined_;
  std::optional<script::ConsentRequest> pending_;
};

// --- declined, denials and long-running scripts ---------------------------------------------

struct DeclinedView {
  std::string chip;    // "▲ read-only"
  std::string banner;  // "▲ The sky130A scripts were not run, so ota_5t opened read-only ..."
  std::string action;  // "Review scripts..."
  std::string place_device_tooltip;
};

[[nodiscard]] DeclinedView declined_view(const script::PdkIdentity& pdk, std::string_view document);

/// The ▲ read-only chip next to the cell name (board tile 6), shown while
/// the window's layout is read-only.
[[nodiscard]] StatusChip read_only_chip();

/// The top bar's subtitle with " · read-only" after the technology
/// ("minimum · read-only · magic-compat on"; board tile 6).
[[nodiscard]] std::string read_only_subtitle(std::string_view subtitle);

/// The tool-rail buttons that edit the layout (Paint, Wire, Label, Place
/// device): disabled while the layout is read-only; Box, Select, Measure
/// and Zoom to fit stay.
[[nodiscard]] bool rail_tool_edits(std::string_view icon) noexcept;
/// The tooltip of an editing tool disabled by a read-only layout.
[[nodiscard]] std::string read_only_tool_tooltip(std::string_view label);

/// What Review scripts... does with the new answer (board note 6): nothing
/// on Don't run; otherwise the logged `pdk trust <variant> <version>` for
/// Trust (a store version only), then reopen the declined layout
/// (cell::reopen_declined: the PDK's script runs, the cells are read again).
struct ReviewPlan {
  bool reopen = false;
  std::optional<std::string> trust_command;
};
[[nodiscard]] ReviewPlan review_plan(const script::PdkIdentity& pdk, script::ConsentDecision decision);

/// The status-bar text of a sandbox denial ("■ Sandbox blocked socket in
/// fetch_models.tcl:12"), the Show link opening the Diagnostics tab.
[[nodiscard]] std::string denial_status(const script::DeniedOperation& denied);

/// Collects what the script layer reports for the window (denials, skipped
/// widget statements, consent requests, menu-model changes); the window
/// drains it after each command.
class ScriptEventCollector final : public script::ScriptObserver {
 public:
  void on_denied(const script::DeniedOperation& operation) override { denied.push_back(operation); }
  void on_skipped_statement(const script::SkippedStatement& statement) override { skipped.push_back(statement); }
  void on_consent_requested(const script::ConsentRequest& request) override { consent = request; }
  void on_menu_model_changed() override { menus_changed = true; }

  std::vector<script::DeniedOperation> denied;
  std::vector<script::SkippedStatement> skipped;
  std::optional<script::ConsentRequest> consent;  // the last one
  bool menus_changed = false;
};

/// The non-modal card of a script running for more than `kLongRunning`.
inline constexpr std::chrono::milliseconds kLongRunning{2000};

struct RunningScript {
  std::string script;  // the file evaluated ("sky130A.tcl"); empty: typed text
  std::string current_file;
  std::uint32_t line = 0;
  std::chrono::steady_clock::duration elapsed{};
};

struct InterruptCardView {
  std::string title;   // "Running sky130A.tcl · 14 s"
  std::string detail;  // "source sky130A-res.tcl · line 2378"
  std::string action = "Interrupt";
  std::string shortcut = "Ctrl-C";
};

/// The card, shown only once the script ran for `kLongRunning`.
[[nodiscard]] std::optional<InterruptCardView> interrupt_card(const RunningScript& running);

/// The running script of a progress report of the project's interpreter
/// (script::ProgressReporting; TclConsoleSession::progress): the sourced
/// file, the file and line being executed, the time it has run. The
/// Interrupt button and Ctrl-C call script::Interpreter::request_interrupt.
[[nodiscard]] RunningScript running_script(const script::ScriptProgress& progress);

}  // namespace hefesto::shell
