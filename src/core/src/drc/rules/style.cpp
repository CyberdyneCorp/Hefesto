// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Loading DRC styles (see style.hpp): the style list, the first load at
// `tech load` (DRCTechInit ... DRCTechFinal), the reload of one style
// (`drc style`, drcLoadStyle), grid rescaling (DRCTechScale, Q-DRC-1) and
// run-time style selection (DRCSetStyle).
//
// Ported from Magic VLSI 8.3.694 drc/DRCtech.c (DRCSetStyle, drcLoadStyle,
// DRCTechScale) and utils/tech.c (TechLoad's post-load scaling), Copyright
// (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Deviations: the drc section is re-read from the technology in memory,
// not from the file on disk; a repeated `drc` section is read as one
// section (Magic re-initializes the rule table at each one).
//
// Owner: RULES (openspec/changes/m3-drc/design.md D2).

#include "hefesto/drc/rules/style.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "drc/rules/compiler.hpp"
#include "drc/rules/text_util.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/drc/rules/cif_catalog.hpp"
#include "hefesto/drc/rules/details.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::drc {

namespace {

using rules_detail::StyleMachine;

std::vector<const tech::RawLine*> drc_lines(const tech::Technology& technology) {
  std::vector<const tech::RawLine*> lines;
  for (const tech::RawSection& section : technology.raw_sections()) {
    if (section.name == "drc") {
      for (const tech::RawLine& line : section.lines) {
        lines.push_back(&line);
      }
    }
  }
  return lines;
}

std::shared_ptr<const RuleSet> compile(const tech::Technology& technology, StyleMachine machine, const CifCatalog& cif,
                                       const compat::CompatSettings& compat, diag::DiagnosticSink& sink,
                                       StyleMachine* out_machine = nullptr) {
  rules_detail::StyleCompiler compiler(rules_detail::CompileInput{technology, cif, compat, sink}, std::move(machine));
  for (const tech::RawLine* line : drc_lines(technology)) {
    compiler.line(*line);
  }
  auto rules = compiler.finish();
  if (out_machine != nullptr) {
    *out_machine = compiler.machine();
  }
  return rules;
}

/// The rules of `src` with every pair list passed through `transform`, and
/// the given halo, step size and scale; everything else copied.
template <typename Transform>
std::shared_ptr<const RuleSet> rebuild(const RuleSet& src, const Halo& halo, std::int32_t step, const ScaleFactor& scale,
                                       Transform&& transform, const StyleDetails* details) {
  RuleSetBuilder builder(src.type_count());
  builder.style_name(src.style_name());
  std::vector<Rule> list;
  for (std::size_t a = 0; a < src.type_count(); ++a) {
    for (std::size_t b = 0; b < src.type_count(); ++b) {
      const auto span = src.rules(static_cast<tech::TileType>(a), static_cast<tech::TileType>(b));
      if (span.empty()) {
        continue;
      }
      list.assign(span.begin(), span.end());
      transform(list);
      for (const Rule& rule : list) {
        builder.add(static_cast<tech::TileType>(a), static_cast<tech::TileType>(b), rule);
      }
    }
  }
  builder.why() = src.why();
  builder.exception_names({src.exception_names().begin(), src.exception_names().end()});
  builder.halo(halo).step_size(step).options(src.options()).scale(scale);
  const std::size_t planes = details != nullptr ? details->plane_count : 0;
  for (std::size_t plane = 0; plane < planes; ++plane) {
    for (std::size_t paint = 0; paint < src.type_count() && paint < tech::kMaxTileTypes; ++paint) {
      builder.paint_row(static_cast<tech::PlaneId>(plane), static_cast<tech::TileType>(paint),
                        src.paint_row(static_cast<tech::PlaneId>(plane), static_cast<tech::TileType>(paint)));
    }
  }
  builder.exact_overlap_types(src.exact_overlap_types());
  if (details != nullptr) {
    builder.details(std::make_shared<const StyleDetails>(*details));
  }
  return builder.build();
}

std::int32_t ceil_scale(std::int64_t value, std::int64_t mul, std::int64_t div) {
  return text::clamp_int((value * mul + div - 1) / div);
}

/// The distances of one rule list rescaled to `scale` (DRCTechScale's
/// drcScaleUp/drcScaleDown; Q-DRC-1 per `magic_compat`).
void rescale_list(std::vector<Rule>& list, ScaleFactor old, ScaleFactor scale, bool magic_compat) {
  if (magic_compat) {  // restore with the one-byte remainders, then rescale
    rules_detail::scale_up(list, old.n);
    rules_detail::scale_down(list, old.d, true);
  } else {  // recompute from the tech-file values
    for (Rule& rule : list) {
      rule.dist = rule.file_dist;
      rule.cdist = rule.file_cdist;
      rule.dist_mod = 0;
      rule.cdist_mod = 0;
    }
  }
  rules_detail::scale_up(list, scale.d);
  rules_detail::scale_down(list, scale.n, magic_compat);
}

/// The style details with the mask-layer rule distances scaled by
/// `cif_factor` (drcCifScale).
std::optional<StyleDetails> scaled_details(const RuleSet& rules, std::pair<std::int32_t, std::int32_t> cif_factor) {
  if (rules.details() == nullptr) {
    return std::nullopt;
  }
  StyleDetails details = *rules.details();
  const auto scale_value = [&](std::int32_t& v) {
    v = v == 0 ? 0 : text::clamp_int(std::int64_t{v} * cif_factor.first / cif_factor.second);
  };
  for (MaskLayerRule& mask : details.mask_layer_rules) {
    scale_value(mask.distance);
    scale_value(mask.value2);
  }
  return details;
}

/// A step size times scale_d / scale_n, rounded down as DRCTechScale does
/// but never below 1: Magic leaves 0 when the grid gets coarser by more than
/// the step, and its checker then divides by zero (Q-DRC-25, fix).
std::int32_t scaled_step(std::int32_t step, std::int32_t scale_n, std::int32_t scale_d) {
  return std::max(1, text::clamp_int(std::int64_t{step} * scale_d / scale_n));
}

/// DRCTechScale (both Q-DRC-1 modes), with the mask-layer rule distances
/// scaled by `cif_factor` (drcCifScale) when given.
std::shared_ptr<const RuleSet> rescale_impl(const RuleSet& rules, std::int32_t scale_n, std::int32_t scale_d,
                                            const compat::CompatSettings& compat,
                                            std::pair<std::int32_t, std::int32_t> cif_factor) {
  if (scale_n == scale_d || scale_n <= 0 || scale_d <= 0) {
    return std::make_shared<const RuleSet>(rules);
  }
  const ScaleFactor old = rules.scale();
  std::int64_t new_d = std::int64_t{old.d} * scale_d;
  std::int64_t new_n = std::int64_t{old.n} * scale_n;
  const std::int64_t gcf = text::find_gcf(new_d, new_n);
  new_d /= gcf;
  new_n /= gcf;
  const ScaleFactor scale{text::clamp_int(new_n), text::clamp_int(new_d)};
  const bool magic_compat = compat.magic_compat;
  const auto transform = [&](std::vector<Rule>& list) { rescale_list(list, old, scale, magic_compat); };
  Halo halo = rules.halo();
  halo.mask_contribution = ceil_scale(halo.mask_contribution, scale_d, scale_n);
  halo.value = magic_compat ? ceil_scale(halo.value, scale_d, scale_n)
                            : std::max(ceil_scale(halo.tech_contribution, scale.d, scale.n), halo.mask_contribution);
  // Rounded down as in Magic, but never to 0, which the checker would divide
  // by (Q-DRC-25, fix in both modes).
  const std::int32_t step = scaled_step(rules.step_size(), scale_n, scale_d);
  const std::optional<StyleDetails> details = scaled_details(rules, cif_factor);
  return rebuild(rules, halo, step, scale, transform, details ? &*details : nullptr);
}

}  // namespace

DrcStyleList style_list(const tech::Technology& technology) {
  StyleMachine machine;
  for (const tech::RawLine* line : drc_lines(technology)) {
    if (line->tokens.empty()) {
      continue;
    }
    if (line->tokens[0] == "style") {
      (void)machine.style_line(line->tokens);
    } else if (machine.list().empty()) {
      (void)machine.admit_rule_line();  // the implicit `default` style
    }
  }
  machine.ensure_default();
  return DrcStyleList{machine.list()};
}

std::shared_ptr<const RuleSet> load_style(const tech::Technology& technology, std::string_view style,
                                          const tech::GridScale& grid, const CifCatalog& cif,
                                          const compat::CompatSettings& compat, diag::DiagnosticSink& sink) {
  StyleMachine machine;
  machine.begin_reload(style_list(technology).names, std::string{style});
  auto rules = compile(technology, std::move(machine), cif, compat, sink);
  return rescale_impl(*rules, static_cast<std::int32_t>(grid.lambda), static_cast<std::int32_t>(grid.internal),
                      compat, {1, 1});
}

std::shared_ptr<const RuleSet> rescale(const RuleSet& rules, std::int32_t scale_n, std::int32_t scale_d,
                                       const compat::CompatSettings& compat) {
  return rescale_impl(rules, scale_n, scale_d, compat, {1, 1});
}

diag::Outcome<std::string> select_style(const DrcStyleList& styles, std::string_view name) {
  const std::string* match = nullptr;
  for (const std::string& style : styles.names) {
    if (!text::strncmp_equal(name, style, name.size())) {
      continue;
    }
    if (match != nullptr) {
      return diag::make("DRC-TECH-003", "DRC style \"" + std::string{name} + "\" is ambiguous.");
    }
    match = &style;
  }
  if (match == nullptr) {
    return diag::make("DRC-TECH-003", "\"" + std::string{name} + "\" is not one of the DRC styles Magic knows.");
  }
  return *match;
}

bool LoadedRules::ok() const noexcept {
  const StyleDetails* details = rules != nullptr ? rules->details() : nullptr;
  return details == nullptr || !details->line_failed;
}

LoadedRules read_rules(const tech::Technology& technology, const compat::CompatSettings& compat,
                       diag::DiagnosticSink& sink) {
  LoadedRules out;
  out.cif = CifCatalog::from_technology(technology, sink);
  // The first load: the first declared style, at the unscaled grid.
  StyleMachine machine;
  out.rules = compile(technology, StyleMachine{}, out.cif, compat, sink, &machine);
  out.styles = DrcStyleList{machine.list()};
  return out;
}

void install_rules(editor::EditorState& editor, LoadedRules loaded) {
  DrcSession& drc = editor.drc;
  drc.cif = std::move(loaded.cif);
  drc.styles = std::move(loaded.styles);
  drc.style = loaded.rules->style_name();
  // TechLoad's post-load scaling of the cifoutput styles and the rules.
  const auto lambda = static_cast<std::int32_t>(editor.grid.lambda);
  const auto internal = static_cast<std::int32_t>(editor.grid.internal);
  std::pair<std::int32_t, std::int32_t> cif_factor{1, 1};
  if (lambda != internal) {
    cif_factor = drc.cif.rescale(lambda, internal);
  }
  drc.rules = rescale_impl(*loaded.rules, lambda, internal, drc.compat_settings(), cif_factor);
  // drcTechFinalStyle: DRCTechHalo and DRCStepSize take the style's values,
  // which drops a `drc *halo` / `drc *stepsize` override.
  drc.halo_override.reset();
  drc.step_size.reset();
  drc.events.publish_style(drc.style);
}

bool reload_rules(editor::EditorState& editor, diag::DiagnosticSink& sink) {
  LoadedRules loaded = read_rules(*editor.technology, editor.drc.compat_settings(), sink);
  const bool ok = loaded.ok();
  install_rules(editor, std::move(loaded));
  return ok;
}

void rescale_rules(editor::EditorState& editor, std::int32_t scale_n, std::int32_t scale_d) {
  DrcSession& drc = editor.drc;
  if (scale_n == scale_d) {
    return;
  }
  const auto cif_factor = drc.cif.rescale(scale_n, scale_d);
  drc.rules = rescale_impl(*drc.rules, scale_n, scale_d, drc.compat_settings(), cif_factor);
  // DRCTechScale also rescales the globals DRCTechHalo (rounded up) and
  // DRCStepSize (rounded down), i.e. the `*halo` / `*stepsize` overrides.
  if (drc.halo_override) {
    drc.halo_override = ceil_scale(*drc.halo_override, scale_d, scale_n);
  }
  if (drc.step_size) {
    drc.step_size = scaled_step(*drc.step_size, scale_n, scale_d);
  }
}

void after_basic_check(editor::EditorState& editor, diag::DiagnosticSink* sink) {
  DrcSession& drc = editor.drc;
  const StyleDetails* details = drc.rules->details();
  if (details == nullptr || details->cif_style.empty() || details->cif_bound) {
    return;  // no cifstyle (drcNeedStyle == NULL), or drcCifStyle already set
  }
  diag::NullSink quiet;
  diag::DiagnosticSink& out = sink != nullptr ? *sink : quiet;
  out.emit(diag::make("DRC-TECH-006", "Loading DRC CIF style."));
  const CifStyleInfo* saved = drc.cif.current();
  const std::string saved_name = saved != nullptr ? saved->name : std::string{};
  if (!drc.cif.select(details->cif_style).ok()) {
    return;
  }
  drc.rules = load_style(*editor.technology, drc.style, editor.grid, drc.cif, drc.compat_settings(), out);
  drc.halo_override.reset();
  drc.step_size.reset();
  const StyleDetails* reloaded = drc.rules->details();
  if (reloaded != nullptr && !reloaded->mask_layer_rules.empty() && !saved_name.empty()) {
    (void)drc.cif.select(saved_name);  // drcCifValid: CIFCurStyle = CIFSaveStyle
  }
  drc.events.publish_style(drc.style);
}

diag::Status select_cif_style(editor::EditorState& editor, std::string_view name, diag::DiagnosticSink& sink) {
  DrcSession& drc = editor.drc;
  if (auto selected = drc.cif.select(name); !selected) {
    return selected;
  }
  const StyleDetails* details = drc.rules->details();
  if (details != nullptr && !details->cif_style.empty()) {
    drc.rules = load_style(*editor.technology, drc.style, editor.grid, drc.cif, drc.compat_settings(), sink);
    drc.events.publish_style(drc.style);
  }
  return diag::Done{};
}

}  // namespace hefesto::drc
