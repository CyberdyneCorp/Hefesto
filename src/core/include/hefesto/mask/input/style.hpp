// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Input styles (capability mask-input-styles;
// openspec/changes/m5-mask-gds-cif/design.md D4, D9): one parsed cifinput
// style (CIFReadStyle) with its values in both their raw and resolved
// forms. The mode-dependent cifinput decisions (Q-MASK-300, 302, 306, 307)
// are resolved when a style is selected and again when magic-compat
// changes, because magic-compat is a session setting and the technology
// file is read once.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); INPUT fills
// in the implementation in src/core/src/mask/input/ (design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::mask {

/// One input layer (CIFReadLayer): `layer` or `templayer` with its
/// operators, the GDS layer/datatype pairs mapped to it (`calma`), and
/// the label handling.
struct InputLayer {
  std::string name;
  bool temporary = false;               // templayer
  tech::TileType type = tech::kSpace;   // the layout type painted (layer rules)
  std::vector<LayerOp> ops;
  std::vector<GdsLayer> gds;            // calma statement (datatype -1 = any)
  bool labels = false;                  // labels statement
  bool sticky_labels = false;           // labels ... sticky (Q-MASK-301)

  friend bool operator==(const InputLayer&, const InputLayer&) = default;
};

/// The mode-dependent values of a style, recomputed by resolve().
struct InputResolved {
  std::int64_t scale_factor = 1;        // crs_scaleFactor after Q-MASK-300
  std::int64_t multiplier = 1;          // crs_multiplier
  bool euclidean_grow = false;          // Q-MASK-306: input grow follows the output style
  bool layer_rule_hints = false;        // Q-MASK-307: boundary/mask-hints in layer rules

  friend bool operator==(const InputResolved&, const InputResolved&) = default;
};

class InputStyle {
 public:
  std::string name;
  std::string scale_text;               // the scalefactor operand as written
  std::string scale_unit;               // "", "nanometers" or "angstroms"
  std::int32_t grid_limit = 0;          // gridlimit
  std::vector<InputLayer> layers;       // in declaration order

  /// The mode-dependent values; valid after resolve().
  [[nodiscard]] const InputResolved& resolved() const noexcept { return resolved_; }

  /// Recomputes resolved() for `compat` (on selection and when
  /// magic-compat changes).
  void resolve(const compat::CompatSettings& compat);

 private:
  InputResolved resolved_;
};

/// Every cifinput style in declaration order, and the current one
/// (cifCurReadStyle).
struct InputStyleSet {
  std::vector<std::shared_ptr<const InputStyle>> styles;
  std::optional<std::size_t> current;

  /// The current style, or nullptr when none is loaded.
  [[nodiscard]] const InputStyle* current_style() const noexcept;
};

/// Reads the `cifinput` section; the first style is current and resolved
/// for `compat`. Reports Magic's CIF-reader tech messages to `sink`.
[[nodiscard]] InputStyleSet read_input_styles(const tech::RawSection& section, const compat::CompatSettings& compat,
                                              diag::DiagnosticSink& sink);

/// `cif istyle <name>`: selects and resolves a style (Q-MASK-302 prefix
/// rules, Q-MASK-303). Fails when no style matches.
[[nodiscard]] diag::Status select_input_style(InputStyleSet& set, std::string_view name,
                                              const compat::CompatSettings& compat);

}  // namespace hefesto::mask
