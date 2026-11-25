// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Deck preparation (openspec/changes/m9-lvs-simulation design.md D11, D18,
// "Area contracts" SIMDECK; capability simulation-netlist-preparation):
// circuit sources, testbench composition, model descriptors, sanitisation
// and the deck limits.
//
// Owner: SIMDECK.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/sim/probe/net_name_map.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::platform {
class FileSystem;
}

namespace hefesto::sim {

/// Deck caps of design D18 (Q-SIM-25): settings that may be lowered, never
/// raised past twice the default.
inline constexpr std::size_t kMaxPreparedDeckBytes = std::size_t{256} << 20;  // 256 MiB
inline constexpr std::size_t kMaxDeckIncludeDepth = 32;

enum class ParasiticMode : std::uint8_t { none, c, rc };

/// "Circuit sources": `layout <cell>`, `schematic <file>`, `spice <file>`
/// or `raw`.
struct CircuitSource {
  enum class Kind : std::uint8_t { layout, schematic, spice, raw };
  Kind kind = Kind::spice;
  std::string name;  // the cell (layout) or the file (schematic, spice); empty for raw
  ParasiticMode parasitics = ParasiticMode::none;  // layout only
};

struct Testbench {
  std::string name;
  CircuitSource dut;
  std::string pdk;
  std::string corner;
  std::vector<std::string> lines;  // stimuli, analyses and saves (or the whole deck for raw)
};

/// "Simulation model descriptor in the PDK store".
struct ModelDescriptor {
  std::string pdk;
  std::vector<std::string> init_commands;   // "PDK initialisation commands"
  std::vector<std::string> model_libraries;  // "Model library resolution"
  std::vector<std::string> corners;

  /// The bundled descriptor of `pdk` (sky130, gf180mcu, ihp-sg13g2).
  [[nodiscard]] static diag::Outcome<ModelDescriptor> load(std::string_view pdk);
};

struct DeckContext {
  platform::FileSystem* files = nullptr;   // includes and the PDK store; required
  std::string pdk_store;                   // the PDK store root
  editor::EditorState* editor = nullptr;   // required for layout and schematic sources
};

struct PreparedDeck {
  std::vector<std::string> lines;
  NetNameMap names;  // empty for spice and raw sources
  ModelDescriptor descriptor;
  std::vector<diag::Diagnostic> diagnostics;
};

[[nodiscard]] diag::Outcome<PreparedDeck> prepare(const Testbench& testbench, const DeckContext& context);

}  // namespace hefesto::sim
