// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// RULES-private: the `style` / `variant` state machine Magic's drc and
// cifoutput sections share (drc-rule-language "DRC styles and variants").
// A section is read once per style load; the machine decides, line by
// line, whether the line belongs to the style being loaded.
//
//   - The style list grows while the section is read for the first time
//     (a `style` line adds its name, or one name per variant, unless a
//     declared name already starts with it: Magic's strncmp existence test).
//   - The style being loaded is the first declared one on a first load, or
//     the requested one on a reload; rule lines between its `style` line and
//     the next `style` line are loaded unless a `variant` line excludes them.
//
// (Magic ref: drc/DRCtech.c DRCTechLine (style, variant handling);
// cif/CIFtech.c CIFTechLine (the same machine for cifoutput))
//
// Owner: RULES (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hefesto::drc::rules_detail {

using Tokens = std::span<const std::string>;

/// TECH_NOT_LOADED, TECH_PENDING, TECH_SUSPENDED, TECH_LOADED.
enum class LoadStatus : std::uint8_t { not_loaded, pending, suspended, loaded };

/// The comma list loop of Magic (`while (*tptr) { cptr = strchr(tptr, ',') ... }`):
/// "a,,b" gives "a", "", "b"; a trailing comma adds no empty piece.
[[nodiscard]] std::vector<std::string> split_comma_list(std::string_view text);

class StyleMachine {
 public:
  /// `cif_variant_rule`: the cifoutput variant test, which also requires
  /// the suffix to be shorter than the style name.
  explicit StyleMachine(bool cif_variant_rule = false) : cif_(cif_variant_rule) {}

  /// Starts a reload of `style` with the complete style list (drcLoadStyle).
  void begin_reload(std::vector<std::string> list, std::string style);

  /// A `style` line; false when its argument count is wrong (the caller
  /// reports "Wrong number of arguments in style statement.").
  bool style_line(Tokens tokens);
  /// A `variant` line while the style is loading or suspended; false on a
  /// wrong argument count.
  bool variant_line(Tokens tokens);
  /// Called for every non-`style` line of the drc section before it is
  /// processed: the implicit `default` style and the single-style rule.
  /// Returns false when the line is outside the loaded style.
  [[nodiscard]] bool admit_rule_line();
  /// The single-style rule without the implicit style (cifoutput).
  [[nodiscard]] bool active() const noexcept {
    return status_ == LoadStatus::pending || status_ == LoadStatus::suspended;
  }
  [[nodiscard]] bool loading() const noexcept { return status_ == LoadStatus::pending; }

  [[nodiscard]] const std::vector<std::string>& list() const noexcept { return list_; }
  [[nodiscard]] const std::optional<std::string>& current() const noexcept { return current_; }
  [[nodiscard]] LoadStatus status() const noexcept { return status_; }
  /// DRCTechFinal with no style line: the implicit `default` style.
  void ensure_default();

 private:
  [[nodiscard]] std::optional<std::string> declare(Tokens tokens);
  void match_reload(Tokens tokens);

  std::vector<std::string> list_;
  std::optional<std::string> current_;
  LoadStatus status_ = LoadStatus::not_loaded;
  bool cif_ = false;
};

}  // namespace hefesto::drc::rules_detail
