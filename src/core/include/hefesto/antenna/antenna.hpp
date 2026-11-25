// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The antenna check and the shared antenna rules (capability antenna-check;
// openspec/changes/m7-extraction/design.md D5, D17). RuleView is the view
// of an ExtStyle's antenna rules and tie-down types that `antennacheck` and
// `lef write` (ANTENNAGATEAREA, ANTENNADIFFAREA) read. check() is the
// command's run path: EFantenna.c over EXTFLAT's flattened devices and
// connect::search_hierarchical. The command matches its first word with
// Magic's table and the first-word rule of netlist/first_word.hpp
// (Q-MISC-91). The measurement and the ratio test are in
// antenna/measure.hpp.
//
// Owner: INTEGRATION (the registration); ANTENNA (RuleView, check(), the
// options and the implementation).

#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/extract/tech/style.hpp"

namespace hefesto::db {
class CellDef;
}
namespace hefesto::editor {
struct EditorState;
}
namespace hefesto::extract {
class StyleView;
}
namespace hefesto::platform {
class FileSystem;
}

namespace hefesto::antenna {

/// The antenna rules of one extraction style (exts_antennaRatio,
/// exts_antennaModel, exts_antennaTieTypes, exts_planeOrder, exts_thick and
/// ExtGetGateTypesMask), after EXTTECH's unit conversion ("Antenna
/// coefficient unit conversion").
class RuleView {
 public:
  RuleView() = default;
  [[nodiscard]] static RuleView from_style(const extract::ExtStyle& style);
  /// The gate and tie-down sets of a StyleView (M6's `lef write` source);
  /// every other rule is empty.
  [[nodiscard]] static RuleView from_view(const extract::StyleView& view);

  /// The rule of `type`, nullopt when the style declares none.
  [[nodiscard]] std::optional<extract::AntennaRule> rules(tech::TileType type) const;
  /// The `tiedown` types (D).
  [[nodiscard]] tech::TypeMask tiedown_types() const noexcept { return tiedown_; }
  /// The types of mosfet, fet, asymmetric and msubcircuit device lines (G).
  [[nodiscard]] tech::TypeMask gate_types() const noexcept { return gates_; }

  /// The `model` flags (extract::kAntenna*).
  [[nodiscard]] std::uint8_t model() const noexcept { return model_; }
  [[nodiscard]] bool partial() const noexcept { return (model_ & extract::kAntennaPartial) != 0; }

  /// The calculation type of `type` (AntennaModel::none without a rule).
  [[nodiscard]] extract::AntennaModel calc_type(tech::TileType type) const noexcept;
  /// ratioGate, ratioDiffB (the constant diffusion ratio, +inf for `none`)
  /// and ratioDiffA (the proportional one), single precision as Magic
  /// stores them; 0 for a type without a rule.
  [[nodiscard]] float ratio_gate(tech::TileType type) const noexcept;
  [[nodiscard]] float ratio_diff_const(tech::TileType type) const noexcept;
  [[nodiscard]] float ratio_diff_prop(tech::TileType type) const noexcept;

  /// False after `noplaneordering` (antennacheck refuses to run).
  [[nodiscard]] bool has_plane_order() const noexcept { return plane_ordering_; }
  /// The `planeorder` index of `plane`, -1 when it has none.
  [[nodiscard]] std::int32_t plane_order(tech::PlaneId plane) const noexcept;
  [[nodiscard]] std::size_t plane_count() const noexcept { return plane_order_.size(); }
  /// exts_thick of `type` (internal units).
  [[nodiscard]] float thickness(tech::TileType type) const noexcept;

 private:
  [[nodiscard]] const extract::AntennaRule* find(tech::TileType type) const noexcept;

  std::vector<extract::AntennaRule> rules_;
  tech::TypeMask tiedown_;
  tech::TypeMask gates_;
  std::uint8_t model_ = 0;
  bool plane_ordering_ = true;
  std::vector<std::int32_t> plane_order_;
  std::array<float, tech::kMaxTileTypes> thickness_{};
};

/// One run's options.
struct AntennaOptions {
  std::vector<std::string> args;  // the run arguments (EFArgs)
  bool debug = false;             // armed by `antennacheck debug`
  bool magic_compat = true;
  /// The console (Magic's TxPrintf and TxError); unset: silent.
  std::function<void(diag::Diagnostic)> emit;
  /// Where `-S` files are read; nullptr: the local file system.
  platform::FileSystem* files = nullptr;
  /// Called when the run reaches its end ("antennacheck finished."), where
  /// Magic clears efAntennaDebug; an early return leaves it armed.
  std::function<void()> finished;
};

/// The run: `root` is the cell of the window the command applies to
/// (feedback and the net traces), nullptr when there is none.
[[nodiscard]] cmd::CommandResult check(editor::EditorState& editor, db::CellDef* root,
                                       const AntennaOptions& options);

/// cmdAntennaCheckOption, in Magic's order: [run], debug, help.
[[nodiscard]] std::span<const std::string_view> antennacheck_options() noexcept;

/// Registers `antennacheck` in the layout client.
bool register_antenna_command(cmd::CommandRegistry& registry);

}  // namespace hefesto::antenna
