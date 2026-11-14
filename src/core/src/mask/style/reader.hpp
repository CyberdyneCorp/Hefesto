// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// STYLES-private: the cifoutput reader (CIFTechLine and its helpers), the
// finalisation (CIFTechFinal) and the unit normalisation
// (CIFTechOutputScale), shared by the files of src/core/src/mask/style/.
//
// Owner: STYLES (openspec/changes/m5-mask-gds-cif/design.md D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/mask/style/core.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::mask::style_detail {

using Tokens = std::vector<std::string>;

/// Diagnostic codes of the reader (src/core/src/mask/style/codes.inc).
inline constexpr std::string_view kTechError = "MASK-STYLE-001";
inline constexpr std::string_view kTechWarning = "MASK-STYLE-002";
inline constexpr std::string_view kPrefixShadow = "MASK-STYLE-003";
inline constexpr std::string_view kGridExpander = "MASK-STYLE-004";
inline constexpr std::string_view kAmbiguousStyle = "MASK-STYLE-005";
inline constexpr std::string_view kUnknownStyle = "MASK-STYLE-006";

/// The tech-load messages of one pass over the section, located on the
/// current line with the `cifoutput` section (the magic rendering prints
/// TechPrintLine's context, diag::format_magic).
class Messages {
 public:
  void at(const tech::RawLine& line) { line_ = &line; }
  /// Magic's TechError (MASK-STYLE-001).
  void error(std::string text) { add(kTechError, std::move(text), false); }
  /// A TechError the spec calls a warning: the line still applies (MASK-STYLE-002).
  void warning(std::string text) { add(kTechWarning, std::move(text), false); }
  /// A Hefesto-only warning (dropped by the magic rendering).
  void native(std::string_view code, std::string text) { add(code, std::move(text), true); }

  [[nodiscard]] std::vector<diag::Diagnostic> take() { return std::move(messages_); }

 private:
  void add(std::string_view code, std::string text, bool native);

  const tech::RawLine* line_ = nullptr;
  std::vector<diag::Diagnostic> messages_;
};

/// The result of cifParseLayers: the tile types and earlier mask layers of
/// a layer list.
struct LayerSets {
  tech::TypeMask types;
  std::vector<std::size_t> layers;  // ascending
};

/// cifParseLayers over the layers declared so far. `exclude`: the layer
/// being defined (cifCurLayer), never its own input. `technology` may be
/// null (the loader's numeric pass): names then resolve to nothing, quietly.
struct LayerContext {
  const tech::Technology* technology = nullptr;
  const std::vector<OutputLayer>* layers = nullptr;
  std::optional<std::size_t> exclude;
  Messages* messages = nullptr;
};
[[nodiscard]] LayerSets parse_layers(const LayerContext& ctx, std::string_view list, bool space_ok);

/// DBTechNoisyNameMask on `text`, reporting its error at the current line.
[[nodiscard]] tech::TypeMask noisy_types(const LayerContext& ctx, std::string_view text);

/// Parses one operator statement (CIFTechLine's operator part). nullopt when
/// the keyword is unknown or the statement is rejected (messages reported).
struct OpContext {
  LayerContext layers;
  std::int32_t grid_limit = 0;
};
[[nodiscard]] std::optional<LayerOp> parse_operator(const OpContext& ctx, const Tokens& t);

/// One style load pass (CIFTechStyleInit, CIFTechLine for every line,
/// CIFTechFinal).
struct LoadResult {
  OutputStyle style;
  std::vector<std::string> names;  // the style list seen by the pass
  std::vector<StyleStateMachine::Shadowed> shadowed;
  bool section_failed = false;
  bool loaded = false;  // the wanted style's `style` line was found
};
[[nodiscard]] LoadResult load_style(const tech::Technology* technology, const tech::RawSection& section,
                                    const std::string& wanted);

/// CIFTechFinal after the lines: the empty-layer check, the unit
/// normalisation, the stacked contacts, the reducer and the halo.
void finalize(const tech::Technology* technology, OutputStyle& style, Messages& messages);

/// CIFTechOutputScale(n, d) in place. `recompute_halo_always`: magic-compat
/// OFF (Q-MASK-18). Returns {multiplier, reduced}.
struct ScaleApplied {
  std::int32_t multiplier = 1;
  std::int32_t reduced = 1;
};
ScaleApplied output_scale(OutputStyle& style, std::int64_t n, std::int64_t d, bool recompute_halo_always);

/// The Q-MASK-8 warning after a normalisation: a non-zero gridlimit with an
/// expander other than 10.
void check_grid_expander(const OutputStyle& style, Messages& messages);

}  // namespace hefesto::mask::style_detail
