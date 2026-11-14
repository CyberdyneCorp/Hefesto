// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The shared style/variant/scale core of the cifoutput and cifinput readers
// (capability mask-output-styles; openspec/changes/m5-mask-gds-cif/design.md
// D5 "Shared core"): Magic's `style` / `variant` state machine (CIFTechLine,
// CIFReadTechLine) and CIFParseScale, written once and used by STYLES and
// INPUT.
//
// A section is read once per style load. The machine decides, line by line,
// whether a line belongs to the style being loaded (Magic's TECH_NOT_LOADED,
// TECH_PENDING, TECH_SUSPENDED and TECH_LOADED states) and keeps the list of
// declared style names.
//
// (Magic ref: cif/CIFtech.c:507-543 CIFParseScale, cif/CIFtech.c:582-715 and
// 794-838 CIFTechLine; cif/CIFrdtech.c:494-720 CIFReadTechLine)
//
// Owner: STYLES (src/core/{include/hefesto,src}/mask/style/, design D2; the
// header was created by the M5 scaffold).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hefesto::mask {

/// CIFParseScale: "n" or "n.d" as the reduced fraction numerator / expander.
struct ScaleValue {
  std::int64_t numerator = 1;
  std::int64_t expander = 1;

  friend bool operator==(const ScaleValue&, const ScaleValue&) = default;
};

/// Parses a `scalefactor` operand as CIFParseScale does: without a `.`,
/// atoi(text) over 1; with one, (I * 10^p + F) / 10^p reduced, where I and F
/// are atoi of the text before and after the `.` and p the number of
/// characters after it. nullopt only for an empty operand.
[[nodiscard]] std::optional<ScaleValue> parse_scale(std::string_view text);

/// The style/variant state of a cif section being read.
class StyleStateMachine {
 public:
  /// When a `style` line names a style that is already declared.
  enum class Registration : std::uint8_t {
    /// Magic: some declared name starts with the line's name (strncmp over
    /// strlen(name)); cifinput with magic-compat ON (Q-MASK-302).
    prefix,
    /// A declared name equals the name (or base+variant); a name Magic's
    /// prefix test drops is appended at the end of the list with a warning.
    /// cifoutput in both modes (Q-MASK-10), cifinput with magic-compat OFF.
    exact,
  };
  /// How a `variant` line suffix v matches the style name S.
  enum class VariantRule : std::uint8_t {
    shorter,   // cifoutput: len(S) > len(v) and S ends with v
    not_longer,  // cifinput: len(S) >= len(v) and S ends with v
  };
  enum class State : std::uint8_t { not_loaded, pending, suspended, loaded };

  struct Options {
    std::string wanted;  // the style to load; empty = the first declared one
    Registration registration = Registration::exact;
    VariantRule variant_rule = VariantRule::shorter;
  };

  /// A style name that Magic's prefix test would drop, registered because
  /// registration is exact (the tech warning of Q-MASK-10).
  struct Shadowed {
    std::string name;      // the new name
    std::string existing;  // the declared name it is a prefix of
  };

  StyleStateMachine() : StyleStateMachine(Options{}) {}
  explicit StyleStateMachine(Options options);

  enum class LineKind : std::uint8_t {
    style,      // a `style` statement (declares names; may start the wanted style)
    variant,    // a `variant` statement while the style is loading or suspended
    statement,  // any other statement
  };

  struct Fed {
    LineKind kind = LineKind::statement;
    /// Magic's wrong-number-of-arguments error for a `style` or `variant`
    /// line: "Wrong number of arguments in <word> statement."
    bool wrong_arguments = false;
  };

  /// Feeds one line of the section (already split into tokens).
  [[nodiscard]] Fed feed(const std::vector<std::string>& tokens);

  /// PENDING: the statements read belong to the wanted style and one of the
  /// active variants.
  [[nodiscard]] bool applies() const noexcept { return state_ == State::pending; }
  /// PENDING or SUSPENDED: the style's block is being read (scalefactor,
  /// units, stepsize and grid statements apply even under a non-matching
  /// variant).
  [[nodiscard]] bool active() const noexcept { return state_ == State::pending || state_ == State::suspended; }
  /// No style has started loading: Magic's CIFCurStyle == NULL, where a
  /// statement fails the section (Q-MASK-17).
  [[nodiscard]] bool no_style() const noexcept { return loading_.empty(); }
  [[nodiscard]] State state() const noexcept { return state_; }

  /// Every style name declared so far: Magic's list, then the names only
  /// exact registration keeps (Q-MASK-10).
  [[nodiscard]] std::vector<std::string> names() const;

  /// The name of the style being loaded; empty before its `style` line on a
  /// first load.
  [[nodiscard]] const std::string& loading() const noexcept { return loading_; }

  /// The names registered only because registration is exact, with the
  /// declared name each is a prefix of, in declaration order.
  [[nodiscard]] const std::vector<Shadowed>& shadowed() const noexcept { return shadowed_; }

 private:
  [[nodiscard]] std::optional<std::string> declare(const std::vector<std::string>& tokens);
  [[nodiscard]] std::optional<std::string> declare_exact(const std::vector<std::string>& tokens);
  [[nodiscard]] bool declared(std::string_view name) const;
  void match_wanted(const std::vector<std::string>& tokens);
  void variant_line(const std::string& list);

  Options options_;
  std::string loading_;
  std::vector<std::string> names_;     // Magic's list
  std::vector<std::string> appended_;  // exact registration only
  std::vector<Shadowed> shadowed_;
  State state_ = State::not_loaded;
};

}  // namespace hefesto::mask
