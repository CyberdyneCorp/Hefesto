// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/script_consent_model.hpp"

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace hefesto::shell {

namespace {

using script::ConsentDecision;

std::string file_name(std::string_view path) {
  const std::size_t slash = path.find_last_of('/');
  return std::string{slash == std::string_view::npos ? path : path.substr(slash + 1)};
}

std::string where(const script::ScriptLocation& location) {
  std::string text = file_name(location.file);
  if (text.empty()) text = "typed text";
  if (location.line > 0) text += ":" + std::to_string(location.line);
  return text;
}

std::string pdk_name(const script::PdkIdentity& pdk) {
  return pdk.variant.empty() ? pdk.family : pdk.variant;
}

std::string store_body(const script::PdkIdentity& pdk, const ConsentContext& context) {
  const std::string name = pdk_name(pdk);
  const std::string opening = context.document.empty() ? "This project" : "Opening " + context.document;
  return opening + " needs the " + name +
         " technology. Its startup file and device scripts are Tcl code shipped with the PDK. Hefesto runs them in "
         "the sandbox: no programs, no network, and files only in this project, the PDK store and a temporary "
         "folder.";
}

std::string outside_body(const script::PdkIdentity& pdk, const script::ConsentRequest& request,
                         const ConsentContext& context) {
  const std::string name = pdk_name(pdk);
  const std::string document = context.document.empty() ? "The layout" : context.document;
  return document + " declares technology " + name + ". Hefesto found " + file_name(request.script) +
         (request.implicit ? " by technology auto-discovery" : "") +
         ", outside the PDK store. This script is not part of a verified release.";
}

std::vector<std::pair<std::string, std::string>> origin_rows(const script::ConsentRequest& request,
                                                             const ConsentContext& context) {
  std::vector<std::pair<std::string, std::string>> rows;
  rows.emplace_back("Script", middle_ellipsis(request.script, 72));
  rows.emplace_back("PDK", pdk_name(request.pdk) + " · " + short_version(request.pdk.version));
  if (context.in_store) {
    rows.emplace_back("Release", "ciel " + short_version(request.pdk.version) +
                                     (context.digest_verified ? " · ● digest verified" : " · ◆ digest not checked"));
    if (!context.then_runs.empty()) rows.emplace_back("Then runs", context.then_runs);
  } else {
    rows.emplace_back("Release", "▲ not in the PDK store · not verified");
    if (request.implicit) rows.emplace_back("Found via", "technology auto-discovery");
  }
  return rows;
}

}  // namespace

// --- consent prompt ---------------------------------------------------------------------

ConsentDecision ConsentPromptView::default_decision() const {
  for (const ConsentAnswer& a : answers) {
    if (a.primary) return a.decision;
  }
  return ConsentDecision::decline;
}

std::string short_version(std::string_view version) {
  if (version.empty()) return "version unknown";
  return std::string{version.substr(0, std::min<std::size_t>(version.size(), 7))};
}

std::string middle_ellipsis(std::string_view path, std::size_t max) {
  if (path.size() <= max || max < 5) return std::string{path};
  const std::size_t keep = max - 1;  // "…" counts as one character
  const std::size_t tail = keep / 2 + keep % 2;
  const std::size_t head = keep - tail;
  return std::string{path.substr(0, head)} + "…" + std::string{path.substr(path.size() - tail)};
}

ConsentPromptView consent_prompt(const script::ConsentRequest& request, const ConsentContext& context) {
  ConsentPromptView view;
  const std::string name = pdk_name(request.pdk);
  view.script_path = request.script;
  view.origin = origin_rows(request, context);
  view.warning = !context.in_store;
  if (context.in_store) {
    view.title = "Run the " + name + " PDK scripts?";
    view.body = store_body(request.pdk, context);
    view.answers = {{"Don't run", ConsentDecision::decline, false},
                    {"Trust " + name + " " + short_version(request.pdk.version), ConsentDecision::trust_version, false},
                    {"Run once", ConsentDecision::run_once, true}};
  } else {
    // Trust is recorded only for a PDK version in the store (board tile 4).
    view.title = "Run a startup script from a downloaded folder?";
    view.body = outside_body(request.pdk, request, context);
    view.answers = {{"Don't run", ConsentDecision::decline, true}, {"Run once", ConsentDecision::run_once, false}};
  }
  return view;
}

PromptDialog consent_dialog(const ConsentPromptView& view) {
  PromptDialog dialog;
  dialog.title = view.title;
  dialog.text = view.body + "\n";
  for (const auto& [key, value] : view.origin) dialog.text += "\n" + key + ":  " + value;
  for (std::size_t i = 0; i < view.answers.size(); ++i) {
    dialog.buttons.push_back({view.answers[i].label, i, view.answers[i].primary});
  }
  return dialog;
}

ConsentDecision consent_decision(const ConsentPromptView& view, int dialog_result) noexcept {
  if (dialog_result <= 0 || static_cast<std::size_t>(dialog_result) > view.answers.size()) {
    return ConsentPromptView::escape_decision();
  }
  return view.answers[static_cast<std::size_t>(dialog_result) - 1].decision;
}

bool release_digest_verified(const pdk::Provenance& provenance) noexcept {
  if (provenance.source == "external" || provenance.assets.empty()) return false;
  return std::ranges::all_of(provenance.assets, [](const auto& asset) { return !asset.second.empty(); });
}

std::string then_runs_text(std::string_view main_script, std::size_t part_files) {
  if (main_script.empty()) return {};
  std::string text{main_script};
  if (part_files > 0) {
    text += " + " + std::to_string(part_files) + (part_files == 1 ? " part file" : " part files") +
            " (device generators)";
  }
  return text;
}

std::string startup_then_runs(const platform::Path& script, std::string_view variant) {
  if (variant.empty()) return {};
  const std::filesystem::path folder = script.parent_path();
  const std::string main_script = std::string{variant} + ".tcl";
  std::error_code ec;
  if (!std::filesystem::is_regular_file(folder / main_script, ec)) return {};
  const std::string part_prefix = std::string{variant} + "-";
  std::size_t parts = 0;
  for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
    const std::string name = entry.path().filename().string();
    if (name.starts_with(part_prefix) && name.ends_with(".tcl") && entry.is_regular_file(ec)) ++parts;
  }
  return then_runs_text(main_script, parts);
}

// --- the desktop ConsentHost ----------------------------------------------------------------

ConsentDecision DesktopConsentHost::ask(const script::PdkIdentity& pdk, const platform::Path& script) {
  if (const auto known = answers_.find(pdk); known != answers_.end()) return known->second;
  // request_consent publishes the request (with `implicit`) before asking.
  const bool implicit = pending_ && pending_->pdk == pdk && pending_->script == script.string() && pending_->implicit;
  pending_.reset();
  const script::ConsentRequest request{pdk, script.string(), implicit};
  ConsentContext context;
  context.document = document_;
  context.in_store = in_store_ ? in_store_(script) : false;
  if (context.in_store && origin_) origin_(pdk, script, context);
  const ConsentPromptView view = consent_prompt(request, context);
  const ConsentDecision decision = ask_ ? ask_(view) : ConsentDecision::decline;
  answers_[pdk] = decision;
  scripts_[pdk] = script;
  if (decision == ConsentDecision::decline) {
    declined_ = pdk;
  } else if (declined_ == pdk) {
    declined_.reset();
  }
  return decision;
}

std::optional<ConsentDecision> DesktopConsentHost::answer(const script::PdkIdentity& pdk) const {
  const auto found = answers_.find(pdk);
  if (found == answers_.end()) return std::nullopt;
  return found->second;
}

std::optional<platform::Path> DesktopConsentHost::script_of(const script::PdkIdentity& pdk) const {
  const auto found = scripts_.find(pdk);
  if (found == scripts_.end()) return std::nullopt;
  return found->second;
}

// --- declined, denials and long-running scripts ---------------------------------------------

DeclinedView declined_view(const script::PdkIdentity& pdk, std::string_view document) {
  const std::string name = pdk_name(pdk);
  const std::string doc = document.empty() ? std::string{"the layout"} : std::string{document};
  DeclinedView view;
  view.chip = "▲ read-only";
  view.banner = "▲ The " + name + " scripts were not run, so " + doc +
                " opened read-only with the minimum technology. Its " + name +
                " layers are not shown, and editing and saving are off.";
  view.action = "Review scripts...";
  view.place_device_tooltip = "No PDK device scripts (the " + name + " scripts were not run)";
  return view;
}

StatusChip read_only_chip() {
  return StatusChip{StatusKind::warning, "read-only"};
}

std::string read_only_subtitle(std::string_view subtitle) {
  // header_text: "<technology>[ · <version>] · magic-compat on|off".
  constexpr std::string_view kCompat = " · magic-compat";
  const std::size_t at = subtitle.rfind(kCompat);
  if (at == std::string_view::npos) return std::string{subtitle} + " · read-only";
  return std::string{subtitle.substr(0, at)} + " · read-only" + std::string{subtitle.substr(at)};
}

bool rail_tool_edits(std::string_view icon) noexcept {
  return icon == "paint" || icon == "wire" || icon == "label" || icon == "place";
}

std::string read_only_tool_tooltip(std::string_view label) {
  return std::string{label} + ": off, the layout is read-only";
}

ReviewPlan review_plan(const script::PdkIdentity& pdk, ConsentDecision decision) {
  ReviewPlan plan;
  if (decision == ConsentDecision::decline) return plan;
  plan.reopen = true;
  if (decision == ConsentDecision::trust_version && !pdk.version.empty()) {
    plan.trust_command = "pdk trust " + pdk_name(pdk) + " " + pdk.version;
  }
  return plan;
}

std::string denial_status(const script::DeniedOperation& denied) {
  return "■ Sandbox blocked " + denied.operation + " in " + where(denied.location) + " · Show";
}

std::optional<InterruptCardView> interrupt_card(const RunningScript& running) {
  if (running.elapsed < kLongRunning) return std::nullopt;
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(running.elapsed).count();
  InterruptCardView card;
  const std::string name = running.script.empty() ? std::string{"a script"} : file_name(running.script);
  card.title = "Running " + name + " · " + std::to_string(seconds) + " s";
  if (!running.current_file.empty()) {
    card.detail = "source " + file_name(running.current_file);
    if (running.line > 0) card.detail += " · line " + std::to_string(running.line);
  } else if (running.line > 0) {
    card.detail = "line " + std::to_string(running.line);  // typed text
  }
  return card;
}

std::optional<Rgba> script_dim(bool card_shown, const ThemeTokens& canvas_tokens) noexcept {
  if (!card_shown) return std::nullopt;
  Rgba scrim = canvas_tokens.canvas;
  scrim.a = kScriptDimAlpha;
  return scrim;
}

RunningScript running_script(const script::ScriptProgress& progress) {
  RunningScript running;
  running.script = progress.script;
  running.current_file = progress.location.file;
  running.line = progress.location.line;
  running.elapsed = progress.elapsed;
  return running;
}

}  // namespace hefesto::shell
