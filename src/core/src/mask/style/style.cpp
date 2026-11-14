// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The output style set: reading every style of the cifoutput section,
// rescaling, and output style selection ("Output style selection",
// "Style declaration and the style list").
//
// Ported from Magic VLSI 8.3.694 cif/CIFmain.c (CIFSetStyle) and
// cif/CIFtech.c (CIFLoadStyle), Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: STYLES (openspec/changes/m5-mask-gds-cif/design.md D2).

#include "hefesto/mask/style/style.hpp"

#include <utility>

#include "mask/style/reader.hpp"

namespace hefesto::mask {

namespace {

constexpr std::int32_t kCalmaLayerMax = 255;

/// The Q-MASK-10 warnings, emitted with the first style's load messages.
void shadow_warnings(const std::vector<StyleStateMachine::Shadowed>& shadowed, std::vector<diag::Diagnostic>& out) {
  for (const StyleStateMachine::Shadowed& s : shadowed) {
    out.push_back(diag::make_native(style_detail::kPrefixShadow,
                                    "Output style \"" + s.name + "\" is a prefix of the earlier style \"" +
                                        s.existing + "\"; it is registered at the end of the style list (Q-MASK-10)."));
  }
}

/// Emits `messages`; the Q-MASK-29 rounding warning of a style load only
/// with magic-compat OFF (the load itself does not know the mode).
void emit(const std::vector<diag::Diagnostic>& messages, diag::DiagnosticSink& sink,
          const compat::CompatSettings& compat) {
  for (const diag::Diagnostic& d : messages) {
    if (compat.magic_compat && d.code == style_detail::kScaleRounded) {
      continue;
    }
    sink.emit(d);
  }
}

}  // namespace

OutputStyle::OutputStyle() {
  label_layer.fill(kNoLayer);
  port_text.fill(kNoLayer);
  port_layer.fill(kNoLayer);
}

bool OutputLayer::gds_written() const noexcept {
  return !temporary && gds && gds->layer >= 0 && gds->layer <= kCalmaLayerMax;
}

std::shared_ptr<const OutputStyle> OutputStyle::rescale(std::int32_t n, std::int32_t d) const {
  return rescale(n, d, compat::CompatSettings{});
}

std::shared_ptr<const OutputStyle> OutputStyle::rescale(std::int32_t n, std::int32_t d,
                                                        const compat::CompatSettings& compat) const {
  return rescale_step(*this, n, d, compat).style;
}

RescaleStep rescale_step(const OutputStyle& style, std::int32_t n, std::int32_t d,
                         const compat::CompatSettings& compat) {
  auto copy = std::make_shared<OutputStyle>(style);
  copy->load_messages.clear();
  const style_detail::ScaleApplied applied = style_detail::output_scale(*copy, n, d, !compat.magic_compat);
  std::vector<diag::Diagnostic> messages;
  if (auto text = style_detail::scale_rounding_warning(*copy, applied); text && !compat.magic_compat) {
    messages.push_back(diag::make_native(style_detail::kScaleRounded, std::move(*text)));
  }
  if (auto text = style_detail::grid_expander_warning(*copy)) {
    messages.push_back(diag::make_native(style_detail::kGridExpander, std::move(*text)));
  }
  return RescaleStep{std::move(copy), applied.multiplier, applied.reduced, std::move(messages)};
}

std::optional<std::size_t> OutputStyle::find_layer(std::string_view layer) const noexcept {
  for (std::size_t i = 0; i < layers.size(); ++i) {
    if (layers[i].name == layer) {
      return i;
    }
  }
  return std::nullopt;
}

const OutputStyle* OutputStyleSet::current_style() const noexcept {
  return current && *current < styles.size() ? styles[*current].get() : nullptr;
}

std::shared_ptr<const OutputStyle> OutputStyleSet::current_shared() const noexcept {
  return current && *current < styles.size() ? styles[*current] : nullptr;
}

std::vector<std::string> OutputStyleSet::names() const {
  std::vector<std::string> out;
  out.reserve(styles.size());
  for (const auto& style : styles) {
    out.push_back(style->name);
  }
  return out;
}

OutputStyleSet read_output_styles(const tech::Technology& technology, const tech::RawSection& section,
                                  diag::DiagnosticSink& sink, const compat::CompatSettings& compat) {
  OutputStyleSet set;
  // The first pass is the tech load: it declares every style and loads the
  // first one; each other style is one more pass (CIFLoadStyle).
  style_detail::LoadResult first = style_detail::load_style(&technology, section, {});
  set.section_failed = first.section_failed;
  if (first.names.empty()) {
    emit(first.style.load_messages, sink, compat);
    return set;
  }
  shadow_warnings(first.shadowed, first.style.load_messages);
  emit(first.style.load_messages, sink, compat);
  set.styles.push_back(std::make_shared<const OutputStyle>(std::move(first.style)));
  for (std::size_t i = 1; i < first.names.size(); ++i) {
    style_detail::LoadResult other = style_detail::load_style(&technology, section, first.names[i]);
    other.style.name = first.names[i];
    set.styles.push_back(std::make_shared<const OutputStyle>(std::move(other.style)));
  }
  set.base = set.styles;
  set.current = 0;
  return set;
}

namespace {

const tech::RawSection* last_cifoutput(const tech::Technology& technology) {
  const tech::RawSection* last = nullptr;
  for (const tech::RawSection& section : technology.raw_sections()) {
    if (section.name == "cifoutput") {
      last = &section;  // a repeated section restarts the list
    }
  }
  return last;
}

}  // namespace

OutputStyleSet read_output_styles(const tech::Technology& technology, diag::DiagnosticSink& sink,
                                  const compat::CompatSettings& compat) {
  const tech::RawSection* section = last_cifoutput(technology);
  return section != nullptr ? read_output_styles(technology, *section, sink, compat) : OutputStyleSet{};
}

std::optional<OutputStyle> read_first_output_style(const tech::Technology& technology) {
  const tech::RawSection* section = last_cifoutput(technology);
  if (section == nullptr) {
    return std::nullopt;
  }
  style_detail::LoadResult first = style_detail::load_style(&technology, *section, {});
  if (first.names.empty()) {
    return std::nullopt;
  }
  return std::move(first.style);
}

namespace {

/// CIFSetStyle's lookup: the exact name, else the only name `name` starts.
struct Match {
  std::optional<std::size_t> index;
  bool ambiguous = false;
};

Match find_style(const OutputStyleSet& set, std::string_view name) {
  Match m;
  for (std::size_t i = 0; i < set.styles.size(); ++i) {
    const std::string& style = set.styles[i]->name;
    if (style == name) {
      return Match{i, false};
    }
    if (style.compare(0, name.size(), name) == 0 && style.size() >= name.size()) {
      m.ambiguous = m.index.has_value();
      m.index = i;
    }
  }
  return m;
}

}  // namespace

diag::Status select_output_style(OutputStyleSet& set, std::string_view name, std::int32_t lambda_n,
                                 std::int32_t lambda_d, const compat::CompatSettings& compat,
                                 diag::DiagnosticSink& sink) {
  const Match m = find_style(set, name);
  if (m.ambiguous) {
    return diag::make(style_detail::kAmbiguousStyle, "CIF output style \"" + std::string{name} + "\" is ambiguous.");
  }
  if (!m.index) {
    return diag::make(style_detail::kUnknownStyle,
                      "\"" + std::string{name} + "\" is not one of the CIF output styles Magic knows.");
  }
  if (set.current && *set.current == *m.index) {
    return diag::Done{};  // CIFLoadStyle: the style is already loaded
  }
  if (set.current && *set.current < set.base.size()) {
    set.styles[*set.current] = set.base[*set.current];  // the old style is freed
  }
  const std::shared_ptr<const OutputStyle>& base = set.base[*m.index];
  emit(base->load_messages, sink, compat);
  if (lambda_n != 1 || lambda_d != 1) {
    RescaleStep step = rescale_step(*base, lambda_n, lambda_d, compat);
    emit(step.messages, sink, compat);
    set.styles[*m.index] = std::move(step.style);
  } else {
    set.styles[*m.index] = base;
  }
  set.current = m.index;
  return diag::Done{};
}

RescaleStep rescale_output_styles(OutputStyleSet& set, std::int32_t n, std::int32_t d,
                                  const compat::CompatSettings& compat) {
  const OutputStyle* current = set.current_style();
  if (current == nullptr) {
    return RescaleStep{};
  }
  RescaleStep step = rescale_step(*current, n, d, compat);
  set.styles[*set.current] = step.style;
  return step;
}

diag::Status select_output_style(OutputStyleSet& set, std::string_view name, const compat::CompatSettings& compat) {
  diag::NullSink quiet;
  return select_output_style(set, name, 1, 1, compat, quiet);
}

}  // namespace hefesto::mask
