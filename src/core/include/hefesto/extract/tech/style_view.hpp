// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// StyleView (capability extract-tech-section, the subset that `lef write`
// and `def write` read; openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D4, D7,
// D9, D10; Open Question 2, decided (a)): the current extraction style's
// device classes per type, antenna tie-down types (exts_antennaTieTypes),
// substrate types and active types, as a view over the tables of the one
// `extract-tech-section` reader. LEF write takes its gate set G and
// diffusion set D from it (lef-write "Macro source geometry and layer
// masks", ExtTech.c:514-562), DEF write the active types of BLOCKAGES
// (defWrite.c:816, 2538). With no extraction style every query answers
// std::nullopt, which the writers treat as empty sets (Q-LEFDEF-40).
//
// M7 re-sources the view from ExtStyle (openspec/changes/m7-extraction/
// design.md D7, D20, tasks.md 4.7); the interface stays. from_style() is
// the view of a parsed style: `lef write` and `def write` take it from the
// session's current style (EditorState::extract.current, Magic's
// ExtCurStyle, lefWrite.c:1268, defWrite.c:816, 2538), so `extract style`
// changes what they read. from_section() runs the one reader
// (src/core/src/extract/tech/) on a raw section; it reports only
// malformed `style`, `variants`, `device`, `fet`, `tiedown` and
// `substrate` lines of the selected style (Magic prints the section's
// messages when the technology loads).
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); EXTSUB
// filled in the implementation in src/core/src/extract/tech/ (M6 design
// D2); EXTTECH re-sourced it (M7 design D7).

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "hefesto/diag/sink.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::extract {

class ExtStyle;

/// The type sets of one extraction style.
struct StyleTypes {
  std::string style;      // the style's name
  tech::TypeMask gate;    // types of a FET-class `device` line's gate (G)
  tech::TypeMask diff;    // the antenna tie-down types (D, ExtGetDiffTypesMask)
  tech::TypeMask active;  // the active types (DEF BLOCKAGES filter)
  tech::TypeMask substrate;  // the `substrate` line's types
};

class StyleView {
 public:
  /// A view with no extraction style.
  StyleView() = default;
  explicit StyleView(StyleTypes current) : current_(std::move(current)) {}

  /// Parses the style selection, `device` (class and types only),
  /// `antenna` tie-down, `substrate` and active-type parts of `section`
  /// (the technology's raw `extract` section) for the style loaded with
  /// the technology (the first one declared; "default" when the section
  /// declares none); every other keyword is skipped silently (design D10).
  [[nodiscard]] static StyleView from_section(const tech::RawSection& section, const tech::Technology& tech,
                                              diag::DiagnosticSink& sink);
  /// The same for the style named `style` (`extract style NAME`); no style
  /// when the section declares no style of that name.
  [[nodiscard]] static StyleView from_section(const tech::RawSection& section, const tech::Technology& tech,
                                              diag::DiagnosticSink& sink, std::string_view style);
  /// The technology's last `extract` section (a repeated section replaces
  /// the styles of the earlier one), or no style without one.
  [[nodiscard]] static StyleView from_technology(const tech::Technology& tech, diag::DiagnosticSink& sink);
  /// The view of a parsed style (a StyleTable selection, or the session's
  /// current style): gate_types() is ExtGetGateTypesMask, diff_types() the
  /// tie-down types, active_types() the active types after the
  /// non-electrical ones are removed, substrate_types() the `substrate`
  /// line's types.
  [[nodiscard]] static StyleView from_style(const ExtStyle& style);
  /// from_style(*style), or no style when `style` is null.
  [[nodiscard]] static StyleView from_style(const std::shared_ptr<const ExtStyle>& style) {
    return style ? from_style(*style) : StyleView{};
  }

  [[nodiscard]] bool has_style() const noexcept { return current_.has_value(); }
  /// The style's name, when there is one.
  [[nodiscard]] std::optional<std::string> style_name() const {
    return current_ ? std::optional<std::string>{current_->style} : std::nullopt;
  }
  [[nodiscard]] std::optional<tech::TypeMask> gate_types() const { return pick(&StyleTypes::gate); }
  [[nodiscard]] std::optional<tech::TypeMask> diff_types() const { return pick(&StyleTypes::diff); }
  [[nodiscard]] std::optional<tech::TypeMask> active_types() const { return pick(&StyleTypes::active); }
  [[nodiscard]] std::optional<tech::TypeMask> substrate_types() const { return pick(&StyleTypes::substrate); }

 private:
  [[nodiscard]] std::optional<tech::TypeMask> pick(tech::TypeMask StyleTypes::* member) const {
    if (!current_) {
      return std::nullopt;
    }
    return (*current_).*member;
  }

  std::optional<StyleTypes> current_;
};

}  // namespace hefesto::extract
