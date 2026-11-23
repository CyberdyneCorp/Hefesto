// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The flat device, coupling-capacitor and resistor records of a flattened
// netlist (capabilities ext-flattening-read "Device records", "Parameters
// records", ext-flattening-hierarchy "Visit orders for output", "Device
// length and width"; openspec/changes/m7-extraction/design.md D5, D13).
// Netlist (read.hpp) holds them in Magic's visit orders: devices as
// EFVisitDevs (children first, each definition's device table in Magic's
// hash order), resistors as EFVisitResists, couplings in the deterministic
// order of Q-NETLIST-1 (by terminal names, the smaller name first).
//
// The fields added in M7 round 2 have default member initializers, so
// positional initializers of the scaffold's fields stay valid.
//
// Owner: INTEGRATION (this header); EXTFLAT (the records' contents).

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "hefesto/db/geometry.hpp"

namespace hefesto::extflat {

/// The index of no node in Netlist::nodes() (a killed or unresolved one).
inline constexpr std::size_t kNoNode = std::numeric_limits<std::size_t>::max();

/// One terminal of a flat device: the flat node name, the terminal length
/// and its attributes.
struct FlatTerminal {
  std::string node;
  std::int64_t length = 0;
  std::vector<std::string> attributes;  // the attribute list split at ','; empty for "0"

  /// The node's index in Netlist::nodes(); kNoNode when the node was
  /// killed or its name did not resolve.
  std::size_t node_index = kNoNode;
  /// The attribute list as the `.ext` file gives it; empty for "0".
  std::string attribute_text{};

  friend bool operator==(const FlatTerminal&, const FlatTerminal&) = default;
};

/// One flat device (EFDev in its flattened form).
struct FlatDevice {
  std::string device_class;  // the `device` record's class keyword ("fet" for a `fet` record)
  std::string model;         // the device type ("None" for an unmodelled R or C)
  db::Rect area;             // the 1x1 rectangle in root coordinates
  std::string substrate;  // flat node name; empty without a substrate
  std::vector<FlatTerminal> terminals;
  std::vector<std::string> params;  // verbatim `name=value` parameters, newest first (Q-NETLIST-9)
  std::string hier_prefix;           // the use path of the cell it came from

  /// dev_cap: a devcap's value (fF) or a `c=` parameter.
  float cap = 0.0F;
  /// dev_res: a devres's value (ohms) or an `r=` parameter.
  float res = 0.0F;
  /// dev_area and dev_perim (internal units, `a=`/`p=` or a `fet` record).
  std::int32_t device_area = 0;
  std::int32_t perimeter = 0;
  /// L and W as EFGetLengthAndWidth derives them (a legacy `fet` from its
  /// area, perimeter, terminals and `ext:l=`/`ext:w=` gate attributes).
  std::int32_t length = 0;
  std::int32_t width = 0;
  /// The substrate node's index in Netlist::nodes(), kNoNode without one.
  std::size_t substrate_index = kNoNode;
  /// The scale EFVisitDevs passes: the definition's scale when the output
  /// scale changed between files and it is not 1, else 1.
  float scale = 1.0F;
  /// The rectangle in the coordinates of the definition's own file.
  db::Rect local_area{};
  /// The definition the device belongs to.
  std::string def{};
  /// One-level visits only: a terminal is on a killed node (EFHierVisitDevs
  /// reports it and emits the device anyway).
  bool on_killed_node = false;
  /// The substrate's local canonical name in its definition (the suffix
  /// spcdevSubstrate compares with a default substrate); `substrate` is
  /// that name joined to the instance path and resolved when it is a flat
  /// name (an implicit global substrate is flat under its bare name only).
  std::string substrate_local{};

  friend bool operator==(const FlatDevice&, const FlatDevice&) = default;
};

/// One flat coupling capacitor (EFCoupleHashTable entry), in attofarads.
struct FlatCap {
  std::string a;
  std::string b;
  double cap = 0.0;

  friend bool operator==(const FlatCap&, const FlatCap&) = default;
};

/// One flat explicit resistor, in milliohms.
struct FlatResist {
  std::string a;
  std::string b;
  double resistance = 0.0;

  friend bool operator==(const FlatResist&, const FlatResist&) = default;
};

/// One subcircuit call of EFVisitSubcircuits: a use of a definition
/// flagged as a subcircuit, or the root when it is one.
struct FlatSubcircuit {
  std::string def;
  std::string prefix;                     // the instance path joined with '/' (empty for the root)
  std::vector<std::string> components;    // the instance path's components
  bool top = false;

  friend bool operator==(const FlatSubcircuit&, const FlatSubcircuit&) = default;
};

/// One declared device parameter of a `parameters` record (DevParam).
struct DeviceParam {
  /// parm_type: the key's first character, and its second when the key
  /// is longer; the two-digit token index ("00", "01", ...) for a cell's
  /// instance parameters (a type name starting with ':').
  std::string type;
  /// parm_name: the text after '=' up to a '*' or a '+'/'-' offset (the
  /// whole token for instance parameters).
  std::string name;
  /// parm_scale: the factor after '*', else 1.
  double scale = 1.0;
  /// parm_offset: the integer after '+'/'-'; 0 with a '*' scale in both
  /// magic-compat settings (Q-NETLIST-8: Magic leaves it uninitialised).
  std::int32_t offset = 0;

  friend bool operator==(const DeviceParam&, const DeviceParam&) = default;
};

}  // namespace hefesto::extflat
