// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extresist net selection, driver and cutoffs (extresist/command/cutoff.hpp).
//
// Ported from Magic 8.3.694 (fc5daa9e) resis/ResRex.c (ResProcessNode,
// devSortFunc, ResWriteExtFile's second cutoff, ResWriteLumpFile),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: RESCMD.

#include "hefesto/extresist/command/cutoff.hpp"

#include <algorithm>
#include <limits>
#include <string>

namespace hefesto::extresist {

namespace {

constexpr double kPsToZs = 1.0e9;
constexpr double kMilliohmsToOhms = 0.001;

bool listed(const std::vector<std::string>& list, const std::string& name) {
  return std::find(list.begin(), list.end(), name) != list.end();
}

// The device's source or drain net other than the one the entry is on.
std::size_t other_terminal(const InputDevice& d, Terminal t) {
  const auto& other = t == Terminal::source ? d.drain : d.source;
  return other.value_or(std::numeric_limits<std::size_t>::max());
}

std::size_t gate_of(const InputDevice& d) {
  return d.gate.value_or(std::numeric_limits<std::size_t>::max());
}

bool driver_class(const InputDevice& d) {
  if (!d.style_class) {
    return false;
  }
  switch (*d.style_class) {
    case extract::DeviceClass::fet:
    case extract::DeviceClass::mosfet:
    case extract::DeviceClass::asymmetric:
    case extract::DeviceClass::msubckt:
    case extract::DeviceClass::bjt:
      return true;
    default:
      return false;
  }
}

// Same gate, and the same or swapped source and drain.
bool parallel(const InputDevice& a, const InputDevice& b) {
  if (a.gate != b.gate) {
    return false;
  }
  return (a.source == b.source && a.drain == b.drain) || (a.source == b.drain && a.drain == b.source);
}

// The first drive point, else the first sink point.
std::optional<Driver> point_driver(const InputNode& node) {
  const ConnectPoint* p = !node.drive_points.empty()  ? &node.drive_points.front()
                          : !node.sink_points.empty() ? &node.sink_points.front()
                                                      : nullptr;
  if (p == nullptr) {
    return std::nullopt;
  }
  return Driver{db::Point{p->rect.x0, p->rect.y0}, p->type, true};
}

// The device with the largest summed W/L among the leading non-gate entries.
std::optional<Driver> device_driver(const InputModel& input, const std::vector<NodeDevice>& sorted) {
  std::optional<Driver> best;
  float max_wl = 0.0F;
  for (std::size_t i = 0; i < sorted.size(); ++i) {
    const InputDevice& first = input.devices[sorted[i].device];
    if (!driver_class(first) || sorted[i].terminal == Terminal::subs) {
      continue;
    }
    if (sorted[i].terminal == Terminal::gate) {
      break;
    }
    float total = first.wl;
    const InputDevice* last = &first;
    while (i + 1 < sorted.size() && parallel(*last, input.devices[sorted[i + 1].device])) {
      last = &input.devices[sorted[++i].device];
      total += last->wl;
    }
    if (total > max_wl) {
      max_wl = total;
      best = Driver{last->location, last->tile_type, false};
    }
  }
  return best;
}

}  // namespace

double min_delay_zs(const ResistOptions& options) noexcept {
  return options.min_delay_ps * kPsToZs;
}

NetSkip net_skip(const InputNode& node, const ResistOptions& options, bool all) {
  if (!options.include.empty()) {
    if (!listed(options.include, node.name)) {
      return NetSkip::not_included;
    }
  } else if (listed(options.ignore, node.name)) {
    return NetSkip::ignored;
  }
  if (!node.status.node_record) {
    return NetSkip::no_record;
  }
  if (node.status.skip && !all) {
    return NetSkip::skipped;
  }
  return NetSkip::none;
}

std::vector<NodeDevice> sort_by_gate(const InputModel& input, const std::vector<NodeDevice>& devices) {
  std::vector<NodeDevice> out = devices;
  std::stable_sort(out.begin(), out.end(), [&](const NodeDevice& a, const NodeDevice& b) {
    const bool ga = a.terminal == Terminal::gate;
    const bool gb = b.terminal == Terminal::gate;
    if (ga || gb) {
      return !ga && gb;  // gate entries last
    }
    const InputDevice& da = input.devices[a.device];
    const InputDevice& db = input.devices[b.device];
    if (gate_of(da) != gate_of(db)) {
      return gate_of(da) < gate_of(db);
    }
    return other_terminal(da, a.terminal) < other_terminal(db, b.terminal);
  });
  return out;
}

std::optional<Driver> choose_driver(const InputModel& input, std::size_t net) {
  const InputNode& node = input.nodes.at(net);
  std::optional<Driver> driver = device_driver(input, sort_by_gate(input, node.devices));
  if (node.status.drive_located) {
    if (std::optional<Driver> located = point_driver(node)) {
      driver = located;
    }
  }
  return driver ? driver : point_driver(node);
}

bool first_cutoff(const InputNode& node, const ResistOptions& options, bool all) {
  if (all) {
    return true;
  }
  const double delay = min_delay_zs(options);
  const bool resistance = options.threshold == 0 || node.resistance > options.threshold;
  const bool slow = delay == 0 || node.cap == 0 || node.resistance * node.cap > delay;
  return (resistance && slow) || listed(options.force, node.name);
}

bool withheld(const InputNode& node, const Extracted& measured, const ResistOptions& options) {
  if (listed(options.force, node.name)) {
    return false;
  }
  if (options.threshold > 0 && measured.maxres < options.threshold) {
    return true;
  }
  const double delay = min_delay_zs(options);
  return delay > 0 && node.cap > 0 && measured.tdi && *measured.tdi < delay;
}

std::int32_t lumped_ohms(const InputNode& node, const Extracted& measured, const ResistOptions& options) {
  if (min_delay_zs(options) > 0) {
    if (node.cap == 0 || !measured.tdi) {
      return 0;
    }
    return static_cast<std::int32_t>((*measured.tdi / node.cap) / 1000.0);
  }
  return static_cast<std::int32_t>(measured.maxres * kMilliohmsToOhms);
}

}  // namespace hefesto::extresist
