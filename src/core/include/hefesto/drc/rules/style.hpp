// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Loading DRC styles (drc-rule-language; openspec/changes/m3-drc/design.md
// D5): the style list of the technology's raw `drc` section, the compiler
// from one style to a frozen drc::RuleSet, rescaling on a grid change, and
// run-time style selection (`drc style`).
//
//   - style_list(): the declared style names (variants expanded) in
//     declaration order; the first is the default; `default` when rule
//     lines precede any `style` line or the section is empty.
//   - load_style(): a port of drc/DRCtech.c (DRCTechStyleInit, DRCTechLine,
//     DRCTechFinal) for one style: keyword parsing with Magic's messages to
//     `sink`, scaling by the grid, ordering, illegal-edge pruning, covered-
//     rule elimination (both Q-DRC-5 modes), halo and step size (Q-DRC-2),
//     the DRC paint table, mask-layer statements (D13) and lint warnings
//     (DRC-LINT-*, native only). Never fails: a broken line is reported
//     and skipped, as in Magic.
//   - rescale(): a new RuleSet for a grid change by n/d (Q-DRC-1: Magic's
//     one-byte-remainder restore with magic-compat ON, recomputation from
//     the tech-file values OFF).
//   - select_style(): `drc style <name>` by unique prefix over the list.
//
// Callers keep the result in editor::EditorState::drc.rules (DrcSession);
// a RuleSet is never mutated (design D4).
//
//   - reload_rules() / rescale_rules() / select_cif_style(): the session
//     hooks of `tech load`, the grid rescales and the cifoutput style
//     switch.
//
// (Magic ref: drc/DRCtech.c DRCTechStyleInit, DRCTechLine, DRCTechAddRule,
// DRCTechFinal, DRCTechScale, DRCSetStyle, DRCLoadStyle)
//
// Owner: RULES (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/drc/rules/cif_catalog.hpp"
#include "hefesto/drc/types.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::diag {
class DiagnosticSink;
}

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::drc {

/// The DRC styles a technology declares (DRCStyleList).
struct DrcStyleList {
  std::vector<std::string> names;  // declaration order, variants expanded
  /// The default style: the first declared one.
  [[nodiscard]] const std::string& default_style() const { return names.front(); }

  friend bool operator==(const DrcStyleList&, const DrcStyleList&) = default;
};

/// The style names of `technology`'s drc section (never empty).
[[nodiscard]] DrcStyleList style_list(const tech::Technology& technology);

/// Compiles style `style` (exact name from style_list()) of `technology`'s
/// drc section under the grid scale `grid`, with `cif` providing the
/// cifoutput styles. Messages go to `sink`.
[[nodiscard]] std::shared_ptr<const RuleSet> load_style(const tech::Technology& technology, std::string_view style,
                                                        const tech::GridScale& grid, const CifCatalog& cif,
                                                        const compat::CompatSettings& compat,
                                                        diag::DiagnosticSink& sink);

/// DRCTechScale: the rule set after the internal grid is scaled by
/// `scale_n`/`scale_d` (Q-DRC-1 per `compat`).
[[nodiscard]] std::shared_ptr<const RuleSet> rescale(const RuleSet& rules, std::int32_t scale_n, std::int32_t scale_d,
                                                     const compat::CompatSettings& compat);

/// The outcome of `drc style <name>`: the selected style's exact name, or
/// the diagnostic for an ambiguous or unknown name (drc-rule-language
/// "Style selection at run time").
[[nodiscard]] diag::Outcome<std::string> select_style(const DrcStyleList& styles, std::string_view name);

// --- Session hooks (called by other areas; RULES implements them) -----------

/// The drc section as `tech load` reads it (DRCTechInit ... DRCTechFinal),
/// before the session takes it: the cifoutput catalog, the declared styles
/// and the default style compiled at the unscaled grid.
struct LoadedRules {
  CifCatalog cif;
  DrcStyleList styles;
  std::shared_ptr<const RuleSet> rules;

  /// False when a rule line of the loaded style failed (a negative
  /// distance: DRCTechAddRule returns FALSE). TechLoad then lists `drc`
  /// among the sections that contained errors and the load fails.
  [[nodiscard]] bool ok() const noexcept;
};

/// Reads `technology`'s cifoutput catalog and drc section for a first load
/// (the first declared style), with Magic's load messages to `sink`.
/// Touches no session state, so `tech load` can fail before installing it.
[[nodiscard]] LoadedRules read_rules(const tech::Technology& technology, const compat::CompatSettings& compat,
                                     diag::DiagnosticSink& sink);

/// Installs `loaded` into editor.drc: TechLoad's post-load scaling of the
/// cifoutput styles and the rules to editor.grid, the style list and
/// current style; drops the `*halo`/`*stepsize` overrides and publishes the
/// style through editor.drc.events. Emits no message.
void install_rules(editor::EditorState& editor, LoadedRules loaded);

/// install_rules(editor, read_rules(*editor.technology, ...)): rebuilds
/// editor.drc from editor.technology under editor.grid, with Magic's load
/// messages to `sink`. Returns LoadedRules::ok() (false: the technology
/// load must fail, as Magic's does).
bool reload_rules(editor::EditorState& editor, diag::DiagnosticSink& sink);

/// drcCifCheck's style bookkeeping, run after every basic check (Magic
/// calls drcCifCheck at the end of DRCBasicCheck; the mask-layer checks
/// themselves are drc::mask_layer_check). When the current style binds a `cifstyle` that
/// is not the current cifoutput style (drcCifStyle == NULL), it prints
/// `Loading DRC CIF style.` (DRC-TECH-006) to `sink` (if any), selects that
/// cifoutput style and reloads the DRC style (CIFLoadStyle ->
/// DRCReloadCurStyle, which drops the `*halo`/`*stepsize` overrides); when
/// the reloaded style has mask-layer rules (drcCifValid) the previous
/// cifoutput style is made current again. Otherwise it does nothing.
/// Command thread only (it replaces editor.drc.rules).
void after_basic_check(editor::EditorState& editor, diag::DiagnosticSink* sink);

/// DRCTechScale after the internal grid was scaled by scale_n/scale_d
/// (`scalegrid`, finer-grid `.mag`/CIF/GDS reads): replaces
/// editor.drc.rules by rescale(). The call sites (commands/CmdRS.c
/// CmdScalegrid, database/DBio.c, cif/CIFrdutils.c in Magic) are wired by
/// INTEGRATION (tasks.md 4.7 open item).
void rescale_rules(editor::EditorState& editor, std::int32_t scale_n, std::int32_t scale_d);

/// The cifoutput-style switch hook (`cif ostyle <name>`: CIFLoadStyle, then
/// DRCReloadCurStyle): makes `name` the current cifoutput style of
/// editor.drc.cif and, when the current DRC style binds a `cifstyle`,
/// reloads it so its mask-layer rules, halo and why texts follow the new
/// binding (drc-rule-language "Style selection at run time"). Fails
/// (DRC-TECH-003) for an unknown style name. M5's `cif` command calls it.
[[nodiscard]] diag::Status select_cif_style(editor::EditorState& editor, std::string_view name,
                                            diag::DiagnosticSink& sink);

}  // namespace hefesto::drc
