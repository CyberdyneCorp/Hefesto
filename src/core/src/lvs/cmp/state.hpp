// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internal state of one comparison (lvs-comparison-algorithm): the
// bipartite graph, the device and net partitions, the seeded generator and
// the verification state. Vertex order is the spec's: devices of circuit 1
// then of circuit 2 in instance order; nets of circuit 1 then of circuit 2
// in increasing net number.
//
// Owner: LVSALG.

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "hefesto/lvs/cmp/compare.hpp"

namespace hefesto::lvs::cmp {

inline constexpr std::uint32_t kNoVertex = 0xFFFF'FFFFU;

/// SplitMix64: the seeded deterministic generator of Q-LVS-4.
class SplitMix64 {
 public:
  explicit SplitMix64(std::uint64_t seed) : state_(seed) {}
  std::uint64_t next() noexcept {
    state_ += 0x9E37'79B9'7F4A'7C15ULL;
    return mix(state_);
  }
  static std::uint64_t mix(std::uint64_t z) noexcept {
    z = (z ^ (z >> 30U)) * 0xBF58'476D'1CE4'E5B9ULL;
    z = (z ^ (z >> 27U)) * 0x94D0'49BB'1331'11EBULL;
    return z ^ (z >> 31U);
  }

 private:
  std::uint64_t state_;
};

/// The comparison graph ("Comparison graph"). Device pins are stored per
/// pin position (kNoVertex for a pin on no net); net pins are the device
/// pins that are on a net.
struct Graph {
  std::uint32_t devices1 = 0;  // devices of circuit 1
  std::uint32_t devices = 0;
  std::uint32_t nets1 = 0;
  std::uint32_t nets = 0;

  std::vector<std::uint32_t> dev_offset;  // devices + 1
  std::vector<std::uint32_t> dev_net;     // net vertex per pin, or kNoVertex
  std::vector<std::uint64_t> dev_label;   // pin label per pin
  std::vector<std::uint32_t> dev_instance;
  std::vector<std::uint64_t> dev_identity;
  std::vector<std::uint32_t> dev_kind;  // interned canonical class name

  std::vector<std::uint32_t> net_offset;  // nets + 1
  std::vector<std::uint32_t> net_dev;     // device vertex per pin
  std::vector<std::uint64_t> net_label;   // the pin's label
  std::vector<NetId> net_id;
  std::vector<bool> net_port;

  [[nodiscard]] bool first_device(std::uint32_t d) const noexcept { return d < devices1; }
  [[nodiscard]] bool first_net(std::uint32_t n) const noexcept { return n < nets1; }
};

struct Class {
  std::vector<std::uint32_t> members;  // vertex order
  std::uint64_t label = 0;
  bool illegal = false;
};

struct Partition {
  std::vector<Class> classes;  // class order
  std::vector<std::uint32_t> class_of;
  std::vector<std::uint64_t> key;
  std::uint32_t boundary = 0;  // the first vertex of circuit 2

  /// (vertices of circuit 1, vertices of circuit 2) in class `c`.
  [[nodiscard]] std::pair<std::size_t, std::size_t> counts(std::size_t c) const;
  [[nodiscard]] bool legal(std::size_t c) const;
  [[nodiscard]] bool matched_pair(std::size_t c) const;
  [[nodiscard]] bool symmetric(std::size_t c) const;
  /// Moves `vertices` (all of one class) into a new class at the end of
  /// class order; false (no change) when they are the whole class.
  bool split_out(const std::vector<std::uint32_t>& vertices, std::uint64_t label);
  /// Combines every illegal class into one class first in class order;
  /// true when there was one.
  bool combine_illegal();
  void rebuild_class_of();
};

}  // namespace hefesto::lvs::cmp

namespace hefesto::lvs {

struct Comparison::State {
  const NetlistDb* db1 = nullptr;
  const NetlistDb* db2 = nullptr;
  const Cell* cell1 = nullptr;
  const Cell* cell2 = nullptr;
  CompareOptions options;
  bool case_insensitive = true;

  cmp::Graph graph;
  cmp::Partition devices;
  cmp::Partition nets;
  cmp::SplitMix64 rng{0};
  bool initialized = false;
  bool exhaustive = false;
  bool bad_match = false;
  bool cancelled = false;
  bool round_limit_reported = false;

  /// Device pairs already property-checked: circuit-1 vertex -> circuit-2 vertex.
  std::map<std::uint32_t, std::uint32_t> checked;
  PropertyState property_state = PropertyState::none;
  std::vector<PropertyError> property_errors;
  std::vector<std::string> property_report;
  std::vector<LogLine> log;
  std::vector<diag::Diagnostic> diagnostics;

  // graph.cpp
  void build();
  [[nodiscard]] const Cell& cell_of_device(std::uint32_t d) const;
  [[nodiscard]] const Instance& instance_of(std::uint32_t d) const;
  [[nodiscard]] const Cell& class_of_device(std::uint32_t d) const;
  [[nodiscard]] std::string device_name(std::uint32_t d) const;
  [[nodiscard]] std::string net_name(std::uint32_t n) const;

  // refine.cpp
  void initial_partition();
  bool round();
  /// Refines until a round converges (or, with `stop_on_mismatch`, the
  /// bad-match condition is raised), polling the cancel token between
  /// rounds; false when cancelled.
  bool refine(bool stop_on_mismatch);
  void converge();

  // verify.cpp
  int classify();
  void check_properties();
  [[nodiscard]] bool port_errors() const;
  [[nodiscard]] std::size_t symmetry_count() const;
  [[nodiscard]] bool properties_match(std::uint32_t a, std::uint32_t b) const;
  [[nodiscard]] PairResult make_result(int value) const;
  [[nodiscard]] Matching matching() const;

  // resolve.cpp
  PairResult resolve();
};

}  // namespace hefesto::lvs
