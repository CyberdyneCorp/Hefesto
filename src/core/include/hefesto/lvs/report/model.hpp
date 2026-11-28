// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The data of the LVS reports (openspec/changes/m9-lvs-simulation design.md
// D16, "As landed (LVSHIER, LVSREP, LVSCMD; M9 R3)"; capability
// lvs-reports): the run's output text with its console-only marks, and per
// compared pair the values of the structured list result ("Structured list
// result", "JSON file") with the layout or schematic references of "Layout
// object references in structured results". lvs/hier fills it, lvs/report
// writes it. Nothing here depends on the comparison state.
//
// Owner: LVSREP.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/db/geometry.hpp"

namespace hefesto::lvs {

/// A piece of the run's output, written as is (it carries its own line
/// ends). `console_only` text never reaches the log ("Log and console
/// routing").
struct OutputText {
  std::string text;
  bool console_only = false;

  friend bool operator==(const OutputText&, const OutputText&) = default;
};

/// The `format` widths of "Two-column layout" (both > 0, Q-LVS-16).
struct ColumnWidths {
  int left = 41;
  int right = 41;
};

/// One `<class> <pin> <count>` entry of a net fragment; `pin` is the pin
/// label (`(a|b)` for a permutable group).
struct FanoutEntry {
  std::string device_class;
  std::string pin;
  std::uint64_t count = 0;
};

struct NetFragment {
  std::string name;
  std::vector<FanoutEntry> entries;
};

/// One `<pin> = <fanout>` row of a device fragment; a permutable group
/// holds several pins and fanouts.
struct PinFanout {
  std::vector<std::string> pins;
  std::vector<std::uint64_t> fanouts;
};

struct InstanceFragment {
  std::string name;  // without the reader's leading '/'
  std::vector<PinFanout> pins;
};

/// One illegal class of "Net mismatch report" or "Device mismatch report":
/// the fragments of circuit 1 and of circuit 2.
template <typename Fragment>
struct MismatchClass {
  std::vector<Fragment> first;
  std::vector<Fragment> second;
};

/// One key and value of a `properties` entry; a missing side is
/// `(no matching parameter)` / `(no value)`.
struct PropertyValue {
  std::string key;
  std::string value;
};

/// The property errors of one matched device pair.
struct PropertyPair {
  std::string device1;
  std::string device2;
  std::vector<PropertyValue> values1;
  std::vector<PropertyValue> values2;
};

/// A row of the pin table ("Pin table").
struct PinRow {
  std::string left;
  std::string right;
};

/// A layout or schematic place of a net, device or pin of the structured
/// result ("Layout object references in structured results"): the use path
/// from the root and a point on the object (layout), or the schematic path
/// and the object's location (schematic).
struct ObjectReference {
  enum class Kind : std::uint8_t { net, device, pin };
  enum class Source : std::uint8_t { layout, schematic };
  Kind kind = Kind::net;
  int circuit = 1;
  std::string name;
  Source source = Source::layout;
  std::vector<std::string> use_path;
  db::Point point;
};

/// The `devices` entry of one side: the summary's class and count.
struct DeviceCount {
  std::string name;
  std::uint64_t count = 0;
};

/// What the structured result holds for one compared pair, in the key order
/// of "Structured list result".
struct PairReport {
  std::string cell1;
  std::string cell2;
  int file1 = 0;
  int file2 = 0;
  /// Both circuits had devices: `name`, `devices` and `nets` are written.
  bool has_devices = false;
  std::vector<DeviceCount> devices1;
  std::vector<DeviceCount> devices2;
  std::uint64_t nets1 = 0;
  std::uint64_t nets2 = 0;
  /// Convergence ran: `badnets` and `badelements` are written (possibly empty).
  bool converged = false;
  std::vector<MismatchClass<NetFragment>> bad_nets;
  std::vector<MismatchClass<InstanceFragment>> bad_elements;
  std::vector<PropertyPair> properties;
  /// The pins after pin matching; nullopt when neither cell had pins.
  std::optional<std::pair<std::vector<std::string>, std::vector<std::string>>> pins;
  /// The pair failed and its cells were flattened into their parents
  /// ("lvs per-pair flow" item "Flattening").
  bool flattened = false;
  /// The pair failed and was kept as black boxes (`-noflatten`).
  bool black_boxed = false;
  /// Layout or schematic places of the entries above (empty for netlist
  /// files). Never written to `comp.out`, the list result or `comp.json`.
  std::vector<ObjectReference> references;
};

}  // namespace hefesto::lvs
