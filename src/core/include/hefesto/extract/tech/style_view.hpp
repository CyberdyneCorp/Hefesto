// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// StyleView (capability extract-tech-section, the subset that `lef write`
// and `def write` read; openspec/changes/m6-lef-def-oasis/design.md D4, D7,
// D9, D10; Open Question 2, decided (a)): the current extraction style's
// device classes per type, antenna tie-down types (exts_antennaTieTypes),
// substrate types and active types, as a view over the tables of the one
// `extract-tech-section` reader. LEF write takes its gate set G and
// diffusion set D from it (lef-write "Macro source geometry and layer
// masks", ExtTech.c:514-562), DEF write the active types of BLOCKAGES
// (defWrite.c:816, 2538). With no extraction style every query answers
// std::nullopt, which the writers treat as empty sets (Q-LEFDEF-40). M7
// re-sources the view from its full reader; the interface stays.
//
// Scaffold stub: from_section returns a view with no style until EXTSUB
// lands the reader (the stub body is inline so that the header needs no
// area sources).
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); EXTSUB
// fills in the implementation in src/core/src/extract/tech/ (design D2).

#pragma once

#include <optional>
#include <string>
#include <utility>

#include "hefesto/diag/sink.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::extract {

/// The type sets of one extraction style.
struct StyleTypes {
  std::string style;      // the style's name
  tech::TypeMask gate;    // types of a FET-class `device` line's gate (G)
  tech::TypeMask diff;    // their source/drain and antenna tie-down types (D)
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
  /// (the technology's raw `extract` section) for the current style; every
  /// other keyword is skipped silently (design D10).
  [[nodiscard]] static StyleView from_section(const tech::RawSection& /*section*/, const tech::Technology& /*tech*/,
                                              diag::DiagnosticSink& /*sink*/) {
    return StyleView{};
  }

  [[nodiscard]] bool has_style() const noexcept { return current_.has_value(); }
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
