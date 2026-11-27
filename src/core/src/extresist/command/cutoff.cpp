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
#include <cmath>
#include <cstdint>
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

// Magic's assignment of a float W/L to its int totWL, saturated: an
// out-of-range float-to-int conversion is undefined behaviour, and a
// crafted `.ext` file can give W/L or a sum of them beyond 2^31 (NaN gives
// 0).
double int_wl(float wl) {
  if (std::isnan(wl)) {
    return 0.0;
  }
  constexpr double kLo = std::numeric_limits<std::int32_t>::min();
  constexpr double kHi = std::numeric_limits<std::int32_t>::max();
  return std::trunc(std::clamp(static_cast<double>(wl), kLo, kHi));
}

// One group of consecutive parallel devices from sorted[i]: its summed W/L
// and the device Magic takes as the driver (t1). The sum is a double so
// that Magic's int totWL is held exactly.
struct Group {
  double wl = 0.0;
  const InputDevice* driver = nullptr;
  std::size_t last = 0;  // index of the group's last entry in `sorted`
};

// ResProcessNode's inner loop. With magic-compat ON the sum is Magic's int
// totWL (each step truncated toward zero), and t1 is the device before the
// last one when the group runs to the end of the list (the loop exits on
// ptr->nextDev == NULL with t1 from the previous step). With it OFF the
// sum is a float and the driver is the group's last device.
Group parallel_group(const InputModel& input, const std::vector<NodeDevice>& sorted, std::size_t i, bool magic_compat) {
  const InputDevice* first = &input.devices[sorted[i].device];
  Group g{magic_compat ? int_wl(first->wl) : static_cast<double>(first->wl), first, i};
  const InputDevice* t1 = first;
  while (g.last + 1 < sorted.size()) {
    t1 = &input.devices[sorted[g.last].device];
    const InputDevice& t2 = input.devices[sorted[g.last + 1].device];
    if (!parallel(*t1, t2)) {
      break;
    }
    // `totWL += rs_wl` adds in float, then converts back to int.
    const float sum = static_cast<float>(g.wl) + t2.wl;
    g.wl = magic_compat ? int_wl(sum) : static_cast<double>(sum);
    ++g.last;
  }
  g.driver = magic_compat ? t1 : &input.devices[sorted[g.last].device];
  return g;
}

// The device with the largest summed W/L among the leading non-gate entries.
std::optional<Driver> device_driver(const InputModel& input, const std::vector<NodeDevice>& sorted, bool magic_compat) {
  std::optional<Driver> best;
  double max_wl = 0.0;
  for (std::size_t i = 0; i < sorted.size(); ++i) {
    if (!driver_class(input.devices[sorted[i].device]) || sorted[i].terminal == Terminal::subs) {
      continue;
    }
    if (sorted[i].terminal == Terminal::gate) {
      break;
    }
    const Group g = parallel_group(input, sorted, i, magic_compat);
    i = g.last;
    if (g.wl > max_wl) {
      max_wl = g.wl;
      best = Driver{g.driver->location, g.driver->tile_type, false};
    }
  }
  return best;
}

// devSortFunc as Magic writes it, with a net's address replaced by its
// reverse first-mention order (a net mentioned later compares lower: the
// order Magic's captures of NL-RES-01, 05 and 06 show; Magic's real order
// follows its heap and is not reproducible, Q-NETLIST-69). It is not a
// strict weak order: a gate entry compares greater than every entry,
// another gate entry included, so the result depends on the sort algorithm.
int dev_sort_func(const InputModel& input, const NodeDevice& a, const NodeDevice& b) {
  if (a.terminal == Terminal::gate) {
    return 1;
  }
  if (b.terminal == Terminal::gate) {
    return -1;
  }
  const InputDevice& da = input.devices[a.device];
  const InputDevice& db = input.devices[b.device];
  if (gate_of(da) < gate_of(db)) {
    return 1;
  }
  const auto sd = [](Terminal t) { return t == Terminal::source || t == Terminal::drain; };
  if (gate_of(da) == gate_of(db) && sd(a.terminal) && sd(b.terminal)) {
    const std::size_t oa = other_terminal(da, a.terminal);
    const std::size_t ob = other_terminal(db, b.terminal);
    if (oa != ob) {
      return oa < ob ? 1 : -1;
    }
    return 0;
  }
  return -1;
}

// glibc's qsort (msort_with_tmp, glibc 2.39 and later, the Debian image
// Magic's captures run in): a top-down merge sort that takes the left entry
// when the comparison is <= 0. With devSortFunc this reverses runs of gate
// entries, so the order of Magic's captures is reproduced on every platform.
void merge_sort(const InputModel& input, std::vector<NodeDevice>& v, std::vector<NodeDevice>& tmp, std::size_t lo,
                std::size_t n) {
  if (n <= 1) {
    return;
  }
  const std::size_t n1 = n / 2;
  merge_sort(input, v, tmp, lo, n1);
  merge_sort(input, v, tmp, lo + n1, n - n1);
  std::size_t i = lo;
  std::size_t j = lo + n1;
  const std::size_t mid = lo + n1;
  const std::size_t end = lo + n;
  std::size_t k = 0;
  while (i < mid && j < end) {
    tmp[k++] = dev_sort_func(input, v[i], v[j]) <= 0 ? v[i++] : v[j++];
  }
  while (i < mid) {
    tmp[k++] = v[i++];
  }
  std::copy(tmp.begin(), tmp.begin() + static_cast<std::ptrdiff_t>(k), v.begin() + static_cast<std::ptrdiff_t>(lo));
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
  if (node.status.redundant) {
    return NetSkip::redundant;
  }
  if (node.status.skip && !all) {
    return NetSkip::skipped;
  }
  return NetSkip::none;
}

std::vector<NodeDevice> sort_by_gate(const InputModel& input, const std::vector<NodeDevice>& devices) {
  std::vector<NodeDevice> out = devices;
  std::vector<NodeDevice> tmp(out.size());
  merge_sort(input, out, tmp, 0, out.size());
  return out;
}

std::optional<Driver> choose_driver(const InputModel& input, std::size_t net, bool magic_compat) {
  const InputNode& node = input.nodes.at(net);
  // The list is already in ResSortByGate order: sorting it again with
  // devSortFunc, which is not a strict weak order, can move entries.
  std::optional<Driver> driver = device_driver(input, node.devices, magic_compat);
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
