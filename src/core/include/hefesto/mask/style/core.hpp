// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The shared style/variant/scale core of the cifoutput and cifinput readers
// (capability mask-output-styles; openspec/changes/m5-mask-gds-cif/design.md
// D5 "Shared core"): Magic's `style` / `variant` state machine (CIFTechLine,
// CIFReadTechLine) and CIFParseScale, written once and used by STYLES and
// INPUT.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); STYLES fills
// in the implementation in src/core/src/mask/style/core.cpp (design D2).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::mask {

/// CIFParseScale: "n" or "n.d" as the reduced fraction numerator / expander.
struct ScaleValue {
  std::int64_t numerator = 1;
  std::int64_t expander = 1;

  friend bool operator==(const ScaleValue&, const ScaleValue&) = default;
};

/// Parses a `scalefactor` operand; nullopt when it is not a number.
[[nodiscard]] std::optional<ScaleValue> parse_scale(std::string_view text);

/// The style/variant state of a cif section being read: which styles a
/// `style` statement declared, which variant names are active for the
/// statements that follow, and whether the style being loaded is the one
/// the reader wants (only that style's statements are parsed in full).
class StyleStateMachine {
 public:
  /// `wanted`: the style to load in full; empty = the first one.
  explicit StyleStateMachine(std::string wanted = {});

  enum class LineKind : std::uint8_t {
    style,      // a `style` statement (declares names; may start the wanted style)
    variant,    // a `variant` statement (changes the active variants)
    statement,  // any other statement: apply it when applies() is true
  };

  /// Feeds one line of the section. Reports Magic's tech messages to `sink`.
  [[nodiscard]] LineKind feed(const tech::RawLine& line, diag::DiagnosticSink& sink);

  /// True while the statements read belong to the wanted style and one of
  /// the active variants.
  [[nodiscard]] bool applies() const noexcept;

  /// Every style name declared so far, in declaration order (variants
  /// expanded as base name + variant).
  [[nodiscard]] const std::vector<std::string>& names() const noexcept { return names_; }

  /// The name of the style being loaded in full; empty before its `style`.
  [[nodiscard]] const std::string& loading() const noexcept { return loading_; }

 private:
  std::string wanted_;
  std::string loading_;
  std::vector<std::string> names_;
  std::vector<std::string> active_variants_;
  bool applies_ = false;
};

}  // namespace hefesto::mask
