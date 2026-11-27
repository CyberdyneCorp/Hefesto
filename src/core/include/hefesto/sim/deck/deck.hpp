// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Deck preparation (openspec/changes/m9-lvs-simulation design.md D11, D18,
// "Area contracts" SIMDECK; capability simulation-netlist-preparation):
// circuit sources, testbench composition, model descriptors, sanitisation
// and the deck limits.
//
// M9 R2 (task 13.1) implements the `spice` (the spec's `netlist <file>`) and
// `raw` (the spec's `none`) sources, all additive:
//   - `spice`: the file is read, its `.include` files are inlined, and the
//     deck is composed as "Testbench composition" orders it (title,
//     options, temp, params, model lines, DUT text, `Xdut`, stimuli, extra
//     lines, `.save`, analyses from Testbench::lines, `.end`);
//   - `raw`: Testbench::lines is the whole deck; its `.include` files are
//     inlined and nothing else changes apart from sanitisation;
//   - every deck is validated and sanitised ("Deck validation and
//     sanitisation", Q-SIM-9), checked against DeckLimits ("Deck limits",
//     Q-SIM-25), and delivered as lines of at most 65535 bytes ("Deck
//     delivery") with its SHA-256.
// Includes are inlined (between `* hefesto include <path>` and
// `* hefesto end include` comments) so that sanitisation sees every line
// the engine parses and a stored deck re-runs identically. `.lib` lines are
// left to the model descriptors (task 13.2). The layout and schematic
// sources (task 13.4) and ModelDescriptor::load (task 13.2) still fail with
// CORE-NOTIMPL-009.
//
// M9 R2 fix pass (review of PR #132): a raw deck's first line is its title
// and is neither inlined nor sanitised (ngspice never parses it); a deck
// line holding a newline, carriage return or NUL is refused (SIM-DECK-015)
// before sanitisation, so the deck hash covers what the engine parses; a
// relative `.lib <file> <section>` is made absolute against the directory
// of the file that holds it, and a `.lib` file outside the PDK store is
// refused (SIM-DECK-016) until task 13.2 inlines and sanitises libraries;
// includes are counted (SIM-DECK-014); a comment line longer than the
// delivery limit is cut, not continued; the limits are checked
// (SIM-DECK-017: zero or above twice the default) instead of clamped.
//
// Owner: SIMDECK.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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
/// Added in the M9 R2 fix pass: files one deck may include in all, as
/// LVS-READ-025 counts them (standing rule on crafted-input limits).
inline constexpr std::size_t kMaxDeckIncludes = 65536;

enum class ParasiticMode : std::uint8_t { none, c, rc };

/// Added in M9 R2: the deck caps in use. Each may be lowered, or raised up
/// to twice its default (design D18).
struct DeckLimits {
  std::size_t max_deck_bytes = kMaxPreparedDeckBytes;
  /// Levels of files, the top file (the raw deck, or a spice source's DUT
  /// file) being level 1, as LVS counts them.
  std::size_t max_include_depth = kMaxDeckIncludeDepth;
  std::size_t max_includes = kMaxDeckIncludes;  // Added in the M9 R2 fix pass
};

/// Added in the M9 R2 fix pass (replaces `clamped`): SIM-DECK-017 when a
/// limit is zero or above twice its default.
[[nodiscard]] diag::Status check_limits(const DeckLimits& limits);

/// Added in M9 R2: "Testbench composition": DUT port `port` is bound to
/// testbench node `node`.
struct PortBinding {
  std::string port;
  std::string node;
};

/// Added in M9 R2: "Stimulus definitions". A name starting with `V` is a
/// voltage source, with `I` a current source.
struct Stimulus {
  enum class Kind : std::uint8_t { dc, pulse, sin, pwl };
  std::string name;
  std::string plus;
  std::string minus;
  Kind kind = Kind::dc;
  /// dc: v; pulse: v1 v2 td tr tf pw per; sin: vo va freq [td [theta
  /// [phase]]]; pwl: t1 v1 t2 v2 ...
  std::vector<double> values;
  std::optional<double> ac_magnitude;
  std::optional<double> ac_phase;
};

/// The source line `<name> <n+> <n-> <text>`, numbers in `%.12g`; an error
/// naming the source (SIM-DECK-010) when it is invalid.
[[nodiscard]] diag::Outcome<std::string> stimulus_line(const Stimulus& stimulus);

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
  /// raw: the whole deck. spice: the analysis lines (M9 R2: they follow the
  /// `.save` line, as "Testbench composition" orders them).
  std::vector<std::string> lines;

  // Added in M9 R2 (task 13.1), for the spice source:
  std::string subckt;                                   // the DUT `.subckt` in the file
  std::vector<PortBinding> bindings;
  std::vector<std::string> options;                     // `.option` texts, in insertion order
  std::optional<double> temperature;                    // `.temp`
  std::vector<std::pair<std::string, double>> params;   // corner parameters (sorted when written)
  std::vector<Stimulus> stimuli;
  std::vector<std::string> extra_lines;
  std::vector<std::string> saves;                       // one `.save` line; empty: none
  /// "Deck validation and sanitisation": keep `.control` sections (the
  /// forbidden commands stay rejected).
  bool allow_control = false;
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
  // Added in M9 R2:
  std::string base_dir;                    // relative spice files and raw-deck includes
  DeckLimits limits;
};

struct PreparedDeck {
  std::vector<std::string> lines;
  NetNameMap names;  // empty for spice and raw sources
  ModelDescriptor descriptor;
  std::vector<diag::Diagnostic> diagnostics;
  /// Added in M9 R2: SHA-256 of the deck text (the lines joined with `\n`,
  /// and a final `\n`), lower-case hex ("Deck delivery").
  std::string sha256;
};

[[nodiscard]] diag::Outcome<PreparedDeck> prepare(const Testbench& testbench, const DeckContext& context);

}  // namespace hefesto::sim
