// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The flat device, coupling-capacitor and resistor records of a flattened
// netlist (capabilities ext-flattening-read "Device records",
// ext-flattening-hierarchy "Visit orders for output";
// openspec/changes/m7-extraction/design.md D5, D13). Netlist (read.hpp)
// holds them in Magic's visit orders.
//
// Scaffold state (tasks.md 3.3): the M6 reader fills none of them, so the
// visitors visit nothing until EXTFLAT lands the device, cap and resistor
// read and flattening (tasks.md 10.x).
//
// Owner: INTEGRATION (this header); EXTFLAT (the records' contents).

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hefesto/db/geometry.hpp"

namespace hefesto::extflat {

/// One terminal of a flat device: the flat node name, the terminal length
/// and its attributes.
struct FlatTerminal {
  std::string node;
  std::int64_t length = 0;
  std::vector<std::string> attributes;

  friend bool operator==(const FlatTerminal&, const FlatTerminal&) = default;
};

/// One flat device (EFDev in its flattened form).
struct FlatDevice {
  std::string device_class;  // the `device` record's class keyword
  std::string model;
  db::Rect area;
  std::string substrate;  // flat node name
  std::vector<FlatTerminal> terminals;
  std::vector<std::string> params;  // `name=value`, in written order
  std::string hier_prefix;           // the use path of the cell it came from

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

}  // namespace hefesto::extflat
