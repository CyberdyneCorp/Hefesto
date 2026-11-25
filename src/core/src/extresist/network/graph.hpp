// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The working network of extresist (capability extresist-network): Magic's
// resNode, resResistor, resDevice, ResJunction, ResContactPoint, Breakpoint
// and resInfo records (resis/resis.h) in arenas indexed by Id, with Magic's
// singly and doubly linked lists kept as links between Ids. List order is
// what the requirements depend on (prepend, head-first iteration, removal
// by search), so every list keeps Magic's structure rather than a sorted
// container; records are never reused, so an Id never dangles.
//
// Owner: RESNET (openspec/changes/m7-extraction design D3).

#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane_stitch.hpp"
#include "hefesto/extresist/network/network.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::extresist::detail {

using Id = std::uint32_t;
inline constexpr Id kNil = std::numeric_limits<Id>::max();

/// INFINITY of Magic's geometry (utils/geometry.h), the tile-plane
/// infinity INF_T of the spec.
inline constexpr std::int32_t kInfinity = (1 << 30) - 4;

/// Sides of a tile (resis.h LEFTEDGE ... OTHERPLANE).
enum Edge : std::uint32_t {
  kLeftEdge = 1,
  kRightEdge = 4,
  kTopEdge = 8,
  kBottomEdge = 16,
  kOtherPlane = 32,
};

/// resInfo ri_status bits (RES_TILE_*).
enum TileStatus : std::uint32_t {
  kTileSubs = 0x01,
  kTileSD = 0x02,
  kTileDev = 0x04,
  kTileDone = 0x08,
  kTileMark = 0x10,
  kTilePushed = 0x20,
};

/// A cell of a singly linked list (resElement, tElement, jElement,
/// cElement): `item` is a resistor, device, junction or contact Id.
struct Cell {
  Id next = kNil;
  Id item = kNil;
};

struct Node {
  Id more = kNil;  // doubly linked membership of the pending queue or done list
  Id less = kNil;
  Id te = kNil;  // device links
  Id re = kNil;  // resistor links
  Id je = kNil;  // junction links
  Id ce = kNil;  // contact links
  double noderes = kResInfinity;
  db::Point loc;
  std::uint32_t why = 0;
  std::uint32_t status = 0;
  float area = 0.0F;  // rn_float: area, then capacitance
  std::string name;
  bool named = false;
  Id delay = kNil;  // rn_client: the RCDelayStuff record
  bool alive = true;
};

struct Resistor {
  Id next = kNil;  // ResResList
  Id last = kNil;
  std::array<Id, 2> node{kNil, kNil};
  float value = 0.0F;
  std::uint32_t status = 0;
  float area = 0.0F;
  std::int32_t cl = 0;
  std::int32_t width = 0;
  tech::TileType tt = 0;
  bool alive = true;
};

/// resDevice. rd_terminals: gate, substrate, then the S/D classes.
struct Device {
  std::vector<Id> terms;
  tech::TileType devtype = 0;
  db::Rect inside;
  /// 64 bits, so the sums over a large device do not overflow; the
  /// Network gets length and width clamped into int32.
  std::int64_t length = 0;
  std::int64_t width = 0;
  std::int64_t perim = 0;
  std::int64_t area = 0;
  std::int32_t tiles = 0;
  /// The gate and S/D capacitance of exts_device[t] for the type of the
  /// device's tile (ResCalculateChildCapacitance).
  double gate_cap = 0.0;
  double sd_cap = 0.0;
};

struct Junction {
  std::array<Id, 2> tile{kNil, kNil};  // TileInfo ids
  db::Point loc;
  bool handled = false;
  Id node = kNil;
};

struct Contact {
  db::Point center;
  db::Rect rect;
  std::array<Id, 4> tile{kNil, kNil, kNil, kNil};  // TileInfo ids (cp_tile)
  std::array<Id, 4> cnode{kNil, kNil, kNil, kNil};
  std::int32_t current = 0;  // cp_currentcontact
  tech::TileType type = 0;
  std::int32_t width = 0;
  std::int32_t height = 0;
  bool handled = false;  // cp_status
};

struct Breakpoint {
  Id node = kNil;
  db::Point loc;
  Id crect = kNil;  // the contact whose rectangle this breakpoint carries
};

struct Port {
  std::string name;
  db::Point loc;
  std::size_t connect = 0;  // the input port it came from
};

/// resInfo of one tile of a scratch plane (its client slot holds the Id).
struct TileInfo {
  tech::PlaneId plane = 0;
  db::StitchTile tile;
  Id contacts = kNil;              // contact links (cElement list)
  Id device = kNil;                // deviceList
  std::vector<Port> ports;         // portList, head first
  std::vector<Breakpoint> breaks;  // breakList, head first
  std::uint32_t source_edge = 0;
  std::uint32_t status = 0;
};

struct Delay {
  float cdown = 0.0F;
  float tdi = 0.0F;
};

/// The network and its construction state. The member functions are the
/// list and node primitives of ResMerge.c, ResUtils.c and ResSimple.c; the
/// reductions of ResDoneWithNode are in reductions.cpp.
class Graph {
 public:
  std::vector<Node> nodes;
  std::vector<Resistor> res;
  std::vector<Device> devices;  // creation order; ResDevList is the reverse
  std::vector<Junction> junctions;
  std::vector<Contact> contacts;
  std::vector<TileInfo> tiles;
  std::vector<Cell> cells;
  std::vector<Delay> delays;
  Id pending = kNil;      // ResNodeQueue
  Id done = kNil;         // ResNodeList
  Id res_list = kNil;     // ResResList
  Id origin = kNil;       // ResNodeAtOrigin
  Id current = kNil;      // resCurrentNode
  bool geometry = false;  // ResOpt_Geometry: no construction-time reductions
  bool magic_compat = true;
  const std::set<std::string, std::less<>>* port_nodes = nullptr;
  std::vector<std::string> messages;

  // --- records ---
  Id new_node(db::Point loc, std::uint32_t why);  // InitializeResNode
  /// A resistor between a and b, prepended to ResResList and to a's and b's
  /// lists (the order of ResCalcEastWest and ResDoContacts).
  Id new_resistor(Id a, Id b);
  void new_break(Id tile, Id node, db::Point loc, Id crect);  // ResNewBreak

  // --- singly linked lists ---
  void push(Id& head, Id item);
  bool erase(Id& head, Id item);
  [[nodiscard]] Id next(Id cell) const { return cells[cell].next; }
  [[nodiscard]] Id item(Id cell) const { return cells[cell].item; }

  // --- the node lists (ResAddToQueue, ResRemoveFromQueue) ---
  void add_to_queue(Id node, Id& list);
  void remove_from_queue(Id node, Id& list);

  // --- ResMerge.c ---
  void merge_nodes(Id keep,
                   Id gone);               // ResMergeNodes(node1 = keep, node2 = gone)
  void delete_res_pointer(Id node, Id r);  // ResDeleteResPointer
  void eliminate_resistor(Id r);           // ResEliminateResistor from ResResList
  void clean_node(Id node,
                  bool info);                             // ResCleanNode(node, info, done, pending)
  void fix_breakpoint(Id tile, Id orig, Id replacement);  // ResFixBreakPoint
  void done_with_node(Id node);                           // ResDoneWithNode (reductions.cpp)
  /// done_with_node's first step: a loop resistor or a zero-value one on
  /// `node` is removed (the latter merging its two nodes) and the walk
  /// restarts; true when one was found.
  bool drop_trivial_resistor(Id node);

  [[nodiscard]] Id other_end(Id r, Id node) const { return res[r].node[0] == node ? res[r].node[1] : res[r].node[0]; }
  [[nodiscard]] bool protected_node(Id node) const { return (nodes[node].why & (kNodeOrigin | kNodeSink)) != 0; }
  void message(std::string text) { messages.push_back(std::move(text)); }

  // --- ResSimple.c helpers ---
  void add_resistor_to_list(Id r, Id& list);  // ResAddResistorToList
  void unlink_resistor(Id r,
                       Id& list);  // unlink from a resistor list without freeing

  // --- conversion ---
  [[nodiscard]] Network to_network() const;
  /// One node of to_network, its resistor and device lists remapped.
  [[nodiscard]] NetNode net_node(Id n, const std::unordered_map<Id, std::size_t>& res_index,
                                 const std::unordered_map<Id, std::size_t>& dev_index) const;
  static Graph from_network(const Network& network);

 private:
  void merge_values(Id keep, Id gone);
  void merge_name(Id keep, Id gone);
  void fix_contact(Id cp, Id keep, Id gone);
  void fix_junction(Id j, Id keep, Id gone);
  int series_check(Id node);                                     // ResSeriesCheck
  int parallel_check(Id node);                                   // ResParallelCheck
  int triangle_check(Id node);                                   // ResTriangleCheck
  void fix_series(Id node, Id far, Id other, Id elim, Id keep);  // ResFixRes
  void fix_parallel(Id elim, Id keep);                           // ResFixParallel
  void star(Id node, Id r1, Id r2, Id r3, Id n1,
            Id n2);  // the delta-to-star replacement
  bool loop_check(Id node);
};

/// The status values ResDoneWithNode and its checks return.
enum Reduction : int { kUntouched = 0, kSeries = 1, kParallel = 2, kLoop = 4, kSingle = 8, kTriangle = 32 };

}  // namespace hefesto::extresist::detail
