// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The comparison of one cell pair (openspec/changes/m9-lvs-simulation
// design.md D8, "Area contracts" LVSALG; capability lvs-comparison-algorithm).
// Single-threaded per pair (Q-LVS-34); the pseudo-random labels come from a
// SplitMix64 generator seeded per pair, and a structural-signature check
// splits any class whose keys collide (Q-LVS-4 fix, both modes), so no
// result depends on a global, a hash collision or the platform.
//
// M9 R2 fills the scaffold contract. Differences from the R0 contract
// (design "As landed (LVSALG, LVSRED, LVSPROP; M9 R2)"): compare_pair takes
// the two databases and cell ids (instances name their class by CellId);
// PairResult also carries the verification value, the symmetry count, the
// property state and errors and the log lines; Comparison exposes the
// stepping commands (`iterate`, `restart`, `exhaustive`, `symmetries`,
// `verify`, `equate`).
//
// Owner: LVSALG.

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/lvs/cancel.hpp"
#include "hefesto/lvs/net/netlist_db.hpp"
#include "hefesto/lvs/prop/properties.hpp"

namespace hefesto::lvs {

struct CompareOptions {
  std::uint64_t seed = 0;     // fixed per pair (design D8)
  bool netgen_compat = true;  // design D5
  CancelToken cancel;
  /// `property topology strict` (lvs-property-matching).
  bool strict_topology = false;
  /// `equate classes` pairs: (circuit-1 class name, circuit-2 class name)
  /// that share one class identity.
  std::vector<std::pair<std::string, std::string>> equated_classes;
  /// Test hook: the refinement keys are ANDed with this mask, so a test can
  /// force every key to collide and check that the structural-signature
  /// check alone keeps the partition correct (Q-LVS-4). Always ~0 in use.
  std::uint64_t key_mask = ~std::uint64_t{0};
};

/// The status of a compared pair (lvs-reports "Result classification").
enum class PairStatus : std::uint8_t {
  match,
  match_property_errors,
  match_port_errors,
  match_port_property_errors,
  symmetric,
  black_box,
  mismatch,
};

/// The matched devices and nets: (instance index in cell 1, instance index
/// in cell 2) and (NetId in cell 1, NetId in cell 2), sorted.
struct Matching {
  std::vector<std::pair<std::uint32_t, std::uint32_t>> devices;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> nets;
};

/// One output line: `console_only` lines are not written to the log file
/// (lvs-reports "Log and console routing").
struct LogLine {
  std::string text;
  bool console_only = false;

  friend bool operator==(const LogLine&, const LogLine&) = default;
};

struct PairResult {
  std::string cell1;
  std::string cell2;
  PairStatus status = PairStatus::mismatch;
  Matching matching;
  bool cancelled = false;
  /// The last verification: −1 mismatch, −2 port errors, k > 0
  /// symmetries, 0 unique ("Match verification").
  int verify = -1;
  PropertyState property_state = PropertyState::none;
  std::vector<PropertyError> property_errors;
  std::vector<LogLine> log;
  std::vector<diag::Diagnostic> diagnostics;
};

/// `verify` options ("verify command").
enum class VerifyOption : std::uint8_t { none, nodes, elements, properties, only, all, equivalent, unique };

/// The option named by unique prefix of `nodes elements properties only all
/// equivalent unique`; nullopt when unknown or ambiguous.
[[nodiscard]] std::optional<VerifyOption> parse_verify_option(std::string_view word);

/// What `verify` printed and returned.
struct VerifyOutput {
  std::optional<int> value;  // set for `equivalent` and `unique`
  std::vector<LogLine> lines;
  bool net_report = false;     // print the net mismatch report (lvs-reports)
  bool device_report = false;  // print the device mismatch report
  std::vector<std::string> bad_nets;      // `-list`: badnets
  std::vector<std::string> bad_elements;  // `-list`: badelements
};

/// Q-LVS-20 (Resolution: compat), the one branch point of the symmetry
/// verdict: whether `Circuits match with <k> symmetries.` is written to the
/// log file. netgen-compat ON: console only (netgen's log ends with
/// `Final result: `); OFF: also in the log.
[[nodiscard]] bool symmetry_verdict_in_log(bool netgen_compat) noexcept;

/// Result of `equate nodes|elements`.
struct EquateOutput {
  int value = 0;
  std::string message;
};

/// Counts of `summary`.
struct PartitionSummary {
  std::size_t device_vertices = 0;  // both circuits
  std::size_t net_vertices = 0;
  std::size_t device_classes = 0;
  std::size_t net_classes = 0;
  std::size_t illegal_device_classes = 0;
  std::size_t illegal_net_classes = 0;
};

/// The comparison state of one pair. The databases must outlive it and
/// must not change while it exists.
class Comparison {
 public:
  Comparison(const NetlistDb& db1, CellId cell1, const NetlistDb& db2, CellId cell2, CompareOptions options);
  ~Comparison();
  Comparison(const Comparison&) = delete;
  Comparison& operator=(const Comparison&) = delete;
  Comparison(Comparison&&) noexcept;
  Comparison& operator=(Comparison&&) noexcept;

  /// False when a circuit had no devices or nets (see diagnostics()).
  [[nodiscard]] bool initialized() const noexcept;
  [[nodiscard]] const std::vector<diag::Diagnostic>& diagnostics() const noexcept;

  /// `run converge`: refine to convergence, then again in exhaustive mode.
  void converge();
  /// `run resolve` (the default `run`): converge, verify, resolve
  /// symmetries and print the verdict; returns the pair's result.
  [[nodiscard]] PairResult resolve();
  /// The pair's result for the current partition (verifies once).
  [[nodiscard]] PairResult result();

  /// `iterate`: one refinement round; true when it converged.
  bool iterate();
  /// `restart`: back to the initial partition; the message when there is
  /// no partition (`Need to initialize data structures first!`).
  std::optional<std::string> restart();
  /// `exhaustive [on|off]`: sets the mode when given, returns the message.
  std::string exhaustive(std::optional<bool> on);
  /// `symmetries`: the symmetric classes, each as its vertex names
  /// (`(1)name` / `(2)name`).
  [[nodiscard]] std::vector<std::vector<std::string>> symmetries() const;
  /// `symmetry`: deprecated.
  [[nodiscard]] static std::string symmetry_command();
  [[nodiscard]] PartitionSummary summary() const;

  [[nodiscard]] VerifyOutput verify(VerifyOption option, bool list);
  EquateOutput equate_nodes(std::string_view net1, std::string_view net2);
  EquateOutput equate_elements(std::string_view device1, std::string_view device2);

  [[nodiscard]] Matching matching() const;

  struct State;

 private:
  std::unique_ptr<State> state_;
};

/// Compares two flat, reduced cells (`run resolve`).
[[nodiscard]] PairResult compare_pair(const NetlistDb& db1, CellId cell1, const NetlistDb& db2, CellId cell2,
                                      const CompareOptions& options);

}  // namespace hefesto::lvs
