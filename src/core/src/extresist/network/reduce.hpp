// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Reducer: ResDoSimplify on a working network (simplify.cpp: path
// resistance, capacitance, delay and pruning; squish.cpp: ResSimplifyNet
// and ResScrunchNet).
//
// Owner: RESNET (openspec/changes/m7-extraction design D3).

#pragma once

#include <memory>

#include "extresist/network/graph.hpp"
#include "extresist/network/heap.hpp"
#include "hefesto/extresist/network/network.hpp"

namespace hefesto::extresist::detail {

class Reducer {
 public:
  /// `heap_built` is HeapState::built (used with magic-compat ON);
  /// `driver_source` the origin of last resort (Network::driver_source).
  Reducer(Graph& g, bool magic_compat, std::int32_t heap_built, Id driver_source, float node_cap)
      : g_(g), compat_(magic_compat), driver_source_(driver_source), node_cap_(node_cap) {
    if (compat_) {
      magic_ = std::make_unique<MagicHeap>(heap_built);
    }
  }

  void run(const Tolerances& tol);

  [[nodiscard]] std::int32_t heap_built() const { return magic_ ? magic_->built() : 0; }
  [[nodiscard]] float max_res() const { return max_res_; }
  [[nodiscard]] float tdi() const { return tdi_; }
  [[nodiscard]] float node_cap() const { return node_cap_; }

  // simplify.cpp
  bool set_path_res();
  void distribute_capacitance(float total);
  float child_capacitance(Id me);
  void calc_tdi(Id node, Id via);
  void prune(Id node, float min_tdi);
  // squish.cpp
  void simplify_step(float tol);  // ResSimplifyNet
  void scrunch(float tol);        // ResScrunchNet

 private:
  void path_node(Id node);
  void path_res(Id r);
  void delay(float min_delay);
  void dead_end(Id node, Id r1, float tol);
  void series(Id node, Id r1, Id r2, float tol);
  void propagate(Id node, float tol);
  void reschedule(Id node);
  void move_devices(Id from, Id to);
  void sort_resistors();
  bool scrunch_one(Id current);

  /// The resistors of a node popped by ResSimplifyNet, oriented.
  struct Directions {
    int drive = 0;
    int receive = 0;
    Id r1 = kNil;
    Id r2 = kNil;  // the last resistor of the list
  };
  Directions orient(Id node);
  /// The counts ResSimplifyNet's dead-end step takes at the far end.
  struct FarEnd {
    int marked = 0;
    int unmarked = 0;
    int drivers = 0;
    int pending = 0;
    Id big = kNil;
  };
  [[nodiscard]] FarEnd far_end(Id other, Id r1, float tol) const;
  /// The smallest adjacent resistor ResScrunchNet folds into.
  struct FoldTarget {
    Id working = kNil;
    Id node1 = kNil;
    int c1 = 0;
    int c2 = 0;
  };
  [[nodiscard]] FoldTarget fold_target(Id current) const;

  Graph& g_;
  bool compat_;
  Id driver_source_;
  float node_cap_;
  float max_res_ = 0.0F;
  float tdi_ = 0.0F;
  std::unique_ptr<MagicHeap> magic_;
  StableQueue stable_;
};

}  // namespace hefesto::extresist::detail
