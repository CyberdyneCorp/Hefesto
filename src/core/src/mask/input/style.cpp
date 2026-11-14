// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Input styles: the scale arithmetic of a style ("Scalefactor statement",
// "Scale normalization", "Grid limit", "Input-scale reporting"), the GDS
// layer lookup ("GDS layer and datatype mapping"), and the style list:
// reading every style, selection ("Input style selection", Q-MASK-303),
// the grid rescale and the magic-compat refresh.
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFrdtech.c
// (CIFCalmaLayerToCifLayer, CIFReadLoadStyle, CIFReadGetGrowSize,
// CIFGetInputScale, CIFSetReadStyle, CIFTechInputScale,
// CIFReadTechLimitScale) and cif/CIFrdcl.c (CIFInputRescale), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Deviations: products saturate at Magic's 32-bit int instead of
// overflowing (X-2 tier 1); an exact name wins over prefix matches in
// selection (Q-MASK-303, fix).
//
// Owner: INPUT (openspec/changes/m5-mask-gds-cif/design.md D2).

#include "hefesto/mask/input/style.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>

#include "hefesto/diag/codes.hpp"
#include "mask/input/reader.hpp"
#include "mask/style/text.hpp"

namespace hefesto::mask {

namespace {

using input_detail::LoadResult;

constexpr std::int64_t kIntMax = std::numeric_limits<std::int32_t>::max();

/// a * b saturated to Magic's int (operands are within int).
std::int64_t sat_mul(std::int64_t a, std::int64_t b) noexcept {
  const std::int64_t p = a * b;
  return std::clamp<std::int64_t>(p, -kIntMax, kIntMax);
}

std::int32_t sat_mul32(std::int32_t a, std::int64_t b) noexcept { return static_cast<std::int32_t>(sat_mul(a, b)); }

const tech::RawSection* last_section(const tech::Technology& technology) {
  const tech::RawSection* found = nullptr;
  for (const tech::RawSection& section : technology.raw_sections()) {
    if (section.name == "cifinput") {
      found = &section;  // CIFReadTechInit restarts the list for a repeated section
    }
  }
  return found;
}

void emit_all(diag::DiagnosticSink& sink, const std::vector<diag::Diagnostic>& messages) {
  for (const diag::Diagnostic& d : messages) {
    sink.emit(d);
  }
}

/// The index of the style `name` selects, or the failure.
diag::Outcome<std::size_t> match_style(const std::vector<std::string>& names, std::string_view name) {
  std::vector<std::size_t> matches;
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (style_text::starts_with(names[i], name)) {
      matches.push_back(i);
    }
  }
  if (matches.size() == 1) {
    return matches.front();
  }
  const auto exact = std::find_if(matches.begin(), matches.end(), [&](std::size_t i) { return names[i] == name; });
  if (exact != matches.end()) {
    return *exact;  // Q-MASK-303 (fix): Magic calls this ambiguous
  }
  if (matches.empty()) {
    return diag::make(input_detail::kUnknownStyle,
                      "\"" + std::string{name} + "\" is not one of the CIF input styles Magic knows.");
  }
  return diag::make(input_detail::kAmbiguousStyle, "CIF input style \"" + std::string{name} + "\" is ambiguous.");
}

}  // namespace

// --- InputRule ----------------------------------------------------------------

const LayerOp* InputRule::find(OpKind kind) const noexcept {
  const auto it = std::find_if(ops.begin(), ops.end(), [kind](const LayerOp& op) { return op.kind == kind; });
  return it == ops.end() ? nullptr : &*it;
}

// --- InputStyle ---------------------------------------------------------------

std::optional<std::size_t> InputStyle::find_cif_layer(std::string_view layer_name) const noexcept {
  const auto it = std::find(cif_layers.begin(), cif_layers.end(), layer_name);
  return it == cif_layers.end() ? std::nullopt : std::optional{static_cast<std::size_t>(it - cif_layers.begin())};
}

std::optional<std::size_t> InputStyle::calma_layer(std::int32_t layer, std::int32_t datatype) const noexcept {
  for (const GdsKey key : {GdsKey{layer, datatype}, GdsKey{layer, -1}, GdsKey{-1, datatype}, GdsKey{-1, -1}}) {
    if (const auto it = calma.find(key); it != calma.end()) {
      return it->second;
    }
  }
  return std::nullopt;
}

CifLayerLabels InputStyle::label_of(std::size_t cif_layer) const noexcept {
  return cif_layer < labels.size() ? labels[cif_layer] : CifLayerLabels{};
}

InputScaleStep InputStyle::resolve(const compat::CompatSettings& compat) {
  scale_factor = scale_source.value > 0 ? scale_source.value : 1;  // CIFReadTechFinal: "using 1"
  multiplier = scale_source.decimal;
  if (scale_source.unit != 0) {
    // Q-MASK-300 (compat): Magic sets M to the unit; OFF multiplies, as on output.
    multiplier = compat.magic_compat ? scale_source.unit : sat_mul(multiplier, scale_source.unit);
  }
  multiplier = std::max<std::int64_t>(multiplier, 1);
  std::size_t i = 0;
  for (InputRule& rule : rules) {
    for (LayerOp& op : rule.ops) {
      op.distance = i < source_distances_.size() ? source_distances_[i] : op.distance;
      ++i;
    }
  }
  return technology_scale(1, 1, true);
}

InputScaleStep InputStyle::technology_scale(std::int64_t n, std::int64_t d, bool reduce_fully) {
  scale_factor = sat_mul(scale_factor, n);
  multiplier = sat_mul(multiplier, d);
  std::int64_t lmult = multiplier;
  for (InputRule& rule : rules) {
    for (LayerOp& op : rule.ops) {
      if (op.distance != 0) {
        op.distance = sat_mul32(op.distance, d);
        lmult = style_text::gcf(lmult, style_text::gcf(std::abs(op.distance), multiplier));
      }
    }
  }
  const std::int64_t lgcf = style_text::gcf(scale_factor, multiplier);
  lmult = std::min(lmult, lgcf);  // Magic takes the smaller, not the common factor
  if (lmult == 0) {
    return InputScaleStep{0, false};
  }
  if (!reduce_fully) {
    lmult = (d != 0 && lmult % d == 0) ? d : 1;
  }
  InputScaleStep step{lmult, false};
  if (lmult > 1) {
    step.truncated = scale_factor % lmult != 0;
    scale_factor /= lmult;
    multiplier /= lmult;
    for (InputRule& rule : rules) {
      for (LayerOp& op : rule.ops) {
        step.truncated = step.truncated || op.distance % lmult != 0;
        op.distance = static_cast<std::int32_t>(op.distance / lmult);
      }
    }
  }
  scale_factor = std::max<std::int64_t>(scale_factor, 1);
  multiplier = std::max<std::int64_t>(multiplier, 1);
  return step;
}

void InputStyle::read_rescale(std::int64_t n, std::int64_t d) {
  if (n > 1) {
    scale_factor = sat_mul(scale_factor, n);
    multiplier = sat_mul(multiplier, n);
    for (InputRule& rule : rules) {
      for (LayerOp& op : rule.ops) {
        op.distance = sat_mul32(op.distance, n);
      }
    }
  }
  if (d > 1) {
    scale_factor = std::max<std::int64_t>(scale_factor / d, 1);
    multiplier = std::max<std::int64_t>(multiplier / d, 1);
    for (InputRule& rule : rules) {
      for (LayerOp& op : rule.ops) {
        op.distance = static_cast<std::int32_t>(op.distance / d);
      }
    }
  }
}

bool InputStyle::limit_scale(std::int64_t ns, std::int64_t ds) const noexcept {
  if (grid_limit == 0) {
    return false;
  }
  std::int64_t gridup = 0;
  std::int64_t scaledown = 0;
  if (__builtin_mul_overflow(std::int64_t{grid_limit} * multiplier, ds, &gridup) ||
      __builtin_mul_overflow(scale_factor * 10, ns, &scaledown) || gridup <= 0) {
    return true;  // beyond any grid Magic's int could hold: no subdivision is permitted
  }
  return scaledown / gridup == 0 || scaledown % gridup != 0;
}

float InputStyle::microns_per_unit() const noexcept {
  return static_cast<float>(10 * scale_factor) / static_cast<float>(multiplier * 1000);
}

std::int64_t InputStyle::grow_size(tech::TileType type) const noexcept {
  std::int64_t dist = 0;
  for (const InputRule& rule : rules) {
    if (rule.temporary || rule.type != type) {
      continue;
    }
    dist = 0;
    for (const LayerOp& op : rule.ops) {
      if (op.kind == OpKind::grow || op.kind == OpKind::grow_grid) {
        dist += op.distance;
      } else if (op.kind == OpKind::shrink) {
        dist -= op.distance;
      }
    }
    if (dist > 0) {
      break;
    }
  }
  return dist;
}

// --- InputStyleSet ------------------------------------------------------------

const InputStyle* InputStyleSet::current_style() const noexcept {
  return current && *current < styles.size() ? styles[*current].get() : nullptr;
}

std::shared_ptr<const InputStyle> InputStyleSet::current_shared() const noexcept {
  return current && *current < styles.size() ? styles[*current] : nullptr;
}

std::vector<std::string> InputStyleSet::names() const {
  std::vector<std::string> out;
  out.reserve(styles.size());
  for (const auto& style : styles) {
    out.push_back(style->name);
  }
  return out;
}

void InputStyleSet::replace_current(std::shared_ptr<const InputStyle> style) {
  if (current && *current < styles.size() && style) {
    styles[*current] = std::move(style);
  }
}

InputStyleSet read_input_styles(const tech::Technology& technology, const tech::RawSection& section,
                                const compat::CompatSettings& compat, diag::DiagnosticSink& sink) {
  InputStyleSet set;
  set.magic_compat = compat.magic_compat;
  LoadResult first = input_detail::load_style(technology, section, {}, compat, true);
  set.section_failed = first.section_failed;
  emit_all(sink, first.style.load_messages);
  if (!first.loaded) {
    return set;
  }
  for (const std::string& name : first.names) {
    std::shared_ptr<const InputStyle> style;
    if (name == first.style.name) {
      style = std::make_shared<const InputStyle>(first.style);
    } else {
      style = std::make_shared<const InputStyle>(
          input_detail::load_style(technology, section, name, compat, false).style);
    }
    set.styles.push_back(style);
    set.base.push_back(style);
    if (name == first.style.name && !set.current) {
      set.current = set.styles.size() - 1;
    }
  }
  return set;
}

InputStyleSet read_input_styles(const tech::Technology& technology, const compat::CompatSettings& compat,
                                diag::DiagnosticSink& sink) {
  const tech::RawSection* section = last_section(technology);
  if (section == nullptr) {
    InputStyleSet empty;
    empty.magic_compat = compat.magic_compat;
    return empty;
  }
  return read_input_styles(technology, *section, compat, sink);
}

std::vector<diag::Diagnostic> rescale_input_styles(InputStyleSet& set, std::int64_t n, std::int64_t d,
                                                   const compat::CompatSettings& compat) {
  std::vector<diag::Diagnostic> messages;
  const InputStyle* current = set.current_style();
  if (current == nullptr || n <= 0 || d <= 0 || (n == 1 && d == 1)) {
    return messages;
  }
  auto scaled = std::make_shared<InputStyle>(*current);
  const InputScaleStep step = scaled->technology_scale(n, d, true);
  if (auto warning = input_detail::truncation_warning(*scaled, step, compat)) {
    messages.push_back(std::move(*warning));
  }
  set.replace_current(std::move(scaled));
  return messages;
}

diag::Status select_input_style(InputStyleSet& set, std::string_view name, std::int64_t lambda_n,
                                std::int64_t lambda_d, const compat::CompatSettings& compat,
                                diag::DiagnosticSink& sink) {
  const diag::Outcome<std::size_t> match = match_style(set.names(), name);
  if (!match.ok()) {
    return match.error();
  }
  const std::size_t index = match.value();
  if (set.current && *set.current == index) {
    return diag::Done{};  // CIFReadLoadStyle: already loaded
  }
  if (index >= set.base.size() || !set.base[index]) {
    return diag::make(input_detail::kUnknownStyle,
                      "\"" + std::string{name} + "\" is not one of the CIF input styles Magic knows.");
  }
  emit_all(sink, set.base[index]->load_messages);
  auto loaded = std::make_shared<InputStyle>(*set.base[index]);
  if (lambda_n > 0 && lambda_d > 0 && !(lambda_n == 1 && lambda_d == 1)) {
    const InputScaleStep step = loaded->technology_scale(lambda_n, lambda_d, true);
    if (auto warning = input_detail::truncation_warning(*loaded, step, compat)) {
      sink.emit(*warning);
    }
  }
  if (set.current && *set.current < set.styles.size()) {
    set.styles[*set.current] = set.base[*set.current];  // the old style goes back to as read
  }
  set.styles[index] = std::move(loaded);
  set.current = index;
  return diag::Done{};
}

diag::Status select_input_style(InputStyleSet& set, std::string_view name, const compat::CompatSettings& compat) {
  diag::NullSink quiet;
  return select_input_style(set, name, 1, 1, compat, quiet);
}

void refresh_input_styles(InputStyleSet& set, const tech::Technology& technology, std::int64_t lambda_n,
                          std::int64_t lambda_d, const compat::CompatSettings& compat) {
  if (set.magic_compat == compat.magic_compat) {
    return;
  }
  const InputStyle* current = set.current_style();
  const std::string name = current != nullptr ? current->name : std::string{};
  diag::NullSink quiet;
  InputStyleSet fresh = read_input_styles(technology, compat, quiet);
  const std::vector<std::string> names = fresh.names();
  const auto kept = std::find(names.begin(), names.end(), name);
  if (kept != names.end() && fresh.current != static_cast<std::size_t>(kept - names.begin())) {
    const std::size_t index = static_cast<std::size_t>(kept - names.begin());
    fresh.styles[*fresh.current] = fresh.base[*fresh.current];
    fresh.current = index;
  }
  set = std::move(fresh);
  (void)rescale_input_styles(set, lambda_n, lambda_d, compat);
}

std::optional<float> input_scale(const InputStyleSet& set) noexcept {
  const InputStyle* style = set.current_style();
  if (style == nullptr) {
    return std::nullopt;
  }
  return style->microns_per_unit();
}

}  // namespace hefesto::mask
