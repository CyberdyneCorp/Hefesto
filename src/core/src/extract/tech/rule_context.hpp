// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// What the keyword handlers of the `extract` section reader share
// (openspec/changes/m7-extraction/design.md D7): the technology, the
// ExtStyle being built and the pass state of StyleReader (style_reader.cpp),
// which calls one handler per keyword from its switch. Each handler group
// lives in its own file: resist_rules.cpp, cap_rules.cpp, default_caps.cpp,
// device_rules.cpp, antenna_rules.cpp, scale_rules.cpp; finalize.cpp is the
// parse half of extTechFinalStyle and conversion.cpp the conversion half
// and ExtTechScale.
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtTech.c, Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: EXTTECH.

#pragma once

#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/extract/tech/style.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::extract::detail {

using Tokens = std::vector<std::string>;

// The style's diagnostic codes (codes.inc).
inline constexpr std::string_view kCodeMalformed = "EXTRACT-STYLE-001";
inline constexpr std::string_view kCodeBadNumber = "EXTRACT-STYLE-007";
inline constexpr std::string_view kCodeBadCapRule = "EXTRACT-STYLE-008";
inline constexpr std::string_view kCodeNoPlaneOrder = "EXTRACT-STYLE-009";
inline constexpr std::string_view kCodeBadDevice = "EXTRACT-STYLE-010";
inline constexpr std::string_view kCodeBadLimit = "EXTRACT-STYLE-011";
inline constexpr std::string_view kCodeBadAntenna = "EXTRACT-STYLE-012";
inline constexpr std::string_view kCodeBadModel = "EXTRACT-STYLE-013";
inline constexpr std::string_view kCodeBadResist = "EXTRACT-STYLE-014";
inline constexpr std::string_view kCodeBadStep = "EXTRACT-STYLE-015";
inline constexpr std::string_view kCodeZeroStep = "EXTRACT-STYLE-016";
inline constexpr std::string_view kCodeContactSize = "EXTRACT-STYLE-017";
inline constexpr std::string_view kCodeNonElectrical = "EXTRACT-STYLE-018";
inline constexpr std::string_view kCodeBadPlane = "EXTRACT-STYLE-019";

// Magic's FRINGE_MULT (ExtTech.c:78).
inline constexpr double kFringeMult = 0.02;

/// How Magic prints a message: TechError (with the file, line and section
/// prefix) or TxError / TxPrintf (the text alone).
enum class Print : std::uint8_t { tech_error, plain };

/// The reader state a handler reads and writes.
struct RuleContext {
  const tech::Technology& tech;
  ExtStyle& style;
  tech::TypeMask& mentioned;               // allExtractTypes
  const tech::TypeMask& types1;            // the first type argument (ExtTechLine's types1)
  const std::vector<std::int32_t>& plane_order;  // exts_planeOrder as read so far
  const bool& plane_order_seen;            // exts_planeOrderStatus == seenPlaneOrder
  std::function<void(std::string_view code, std::string text, Print print)> report;

  /// DBTechNoisyNameMask: the types of `text` (empty and reported when the
  /// list does not parse), recorded as mentioned.
  tech::TypeMask noisy_types(std::string_view text) const;
  /// DBTechNameMask: the types of `text`, nullopt when it does not parse.
  [[nodiscard]] std::optional<tech::TypeMask> quiet_types(std::string_view text) const;
  /// DBTechNoisyNamePlane: the plane, nullopt (reported) when unknown or
  /// ambiguous.
  [[nodiscard]] std::optional<tech::PlaneId> noisy_plane(std::string_view name) const;
  /// aToCap: `%lf`, 0 and reported when it is not a number.
  [[nodiscard]] double cap_value(const std::string& text) const;
  /// The home plane of a type (DBPlane); 0 for space.
  [[nodiscard]] tech::PlaneId plane_of(tech::TileType type) const;
  /// TT_TECHDEPBASE .. DBNumTypes (the real types, stacked ones included).
  [[nodiscard]] tech::TileType type_end() const { return static_cast<tech::TileType>(tech.type_count()); }
  [[nodiscard]] bool magic_compat() const noexcept { return style.magic_compat; }
};

// Number parsing as Magic's helpers do it.
[[nodiscard]] bool str_is_numeric(const std::string& s);  // StrIsNumeric: strtod consumed something
[[nodiscard]] bool str_is_int(std::string_view s) noexcept;  // StrIsInt
/// sscanf("%lf" / "%lg"): the value, nullopt when nothing converts.
[[nodiscard]] std::optional<double> scan_double(const std::string& s);
/// sscanf("%d"): the value, nullopt when nothing converts.
[[nodiscard]] std::optional<std::int32_t> scan_int(const std::string& s);

/// Magic's `(int)v`, made total (Q-EXTRACT-30, fixed in both modes): the
/// value truncated toward zero, saturated to the int range, NaN as 0.
/// Magic's conversion of an out-of-range or NaN value is undefined.
[[nodiscard]] inline std::int32_t saturate_int32(double v) noexcept {
  constexpr double kMax = std::numeric_limits<std::int32_t>::max();
  constexpr double kMin = std::numeric_limits<std::int32_t>::min();
  if (std::isnan(v)) {
    return 0;
  }
  if (v >= kMax) {
    return std::numeric_limits<std::int32_t>::max();
  }
  if (v <= kMin) {
    return std::numeric_limits<std::int32_t>::min();
  }
  return static_cast<std::int32_t>(v);
}

/// saturate_int32 of a rounded value (std::lround made total).
[[nodiscard]] inline std::int32_t saturate_round(double v) noexcept {
  return saturate_int32(std::round(v));
}

// The handlers, one per keyword group (wired in StyleReader::apply).
void resist_rule(RuleContext& ctx, const Tokens& t);    // resist
void contact_rule(RuleContext& ctx, const Tokens& t);   // contact
void devresist_rule(RuleContext& ctx, const Tokens& t); // devresist, fetresist
void areacap_rule(RuleContext& ctx, const Tokens& t);   // areacap
void perimc_rule(RuleContext& ctx, const Tokens& t);    // perimc
void overlap_rule(RuleContext& ctx, const Tokens& t);   // overlap
void sidewall_rule(RuleContext& ctx, const Tokens& t);  // sidewall
void sideoverlap_rule(RuleContext& ctx, const Tokens& t);  // sideoverlap
void default_areacap_rule(RuleContext& ctx, const Tokens& t);
void default_perimeter_rule(RuleContext& ctx, const Tokens& t);
void default_sidewall_rule(RuleContext& ctx, const Tokens& t);
void default_overlap_rule(RuleContext& ctx, const Tokens& t);
void default_sideoverlap_rule(RuleContext& ctx, const Tokens& t);
/// device, after StyleReader checked the class (`class_name`, devTable's
/// full name) and the argument count.
void device_rule(RuleContext& ctx, const Tokens& t, std::string_view class_name);
void fet_rule(RuleContext& ctx, const Tokens& t);       // fet
void antenna_rule(RuleContext& ctx, const Tokens& t);   // antenna
void model_rule(RuleContext& ctx, const Tokens& t);     // model
void tiedown_rule(RuleContext& ctx, const Tokens& t);   // tiedown
void substrate_rule(RuleContext& ctx, const Tokens& t); // substrate
void connect_rule(RuleContext& ctx, const Tokens& t, bool connect);  // connect, disconnect
void cscale_rule(RuleContext& ctx, const Tokens& t);
void rscale_rule(RuleContext& ctx, const Tokens& t);
/// step: false when the value is not positive (Magic aborts the tech load).
bool step_rule(RuleContext& ctx, const Tokens& t);
void sidehalo_rule(RuleContext& ctx, const Tokens& t);

/// extTechStyleInit for `style` (the defaults of "Style defaults").
void init_style(ExtStyle& style, const tech::Technology& tech, bool magic_compat);

/// The parse half of extTechFinalStyle (ExtTech.c:3627-3800): substrate
/// device types, resistive connectivity, the contact residues and the
/// coupling-table completion, masked by the active types; then the
/// derived lists (CapRules lists, ranges, resist classes, device info).
void finalize_style(ExtStyle& style, const tech::Technology& tech);

/// ExtGetDevInfo's table of a finalized style.
[[nodiscard]] std::vector<DeviceInfo> build_device_info(const ExtStyle& style, const tech::Technology& tech);

/// CapRules' lists from CapTables (the reader's form).
void derive_rule_lists(ExtStyle& style, const tech::Technology& tech);

}  // namespace hefesto::extract::detail
