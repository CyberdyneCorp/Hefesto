// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Parallel device merging and distributed junctions of the flat SPICE
// writer (capability netlist-spice-devices "Parallel device merging",
// "Terminal area and perimeter" with -d). Q-NETLIST-6 (fix, both modes):
// each device keeps its own multiplier and merged flag, so a device with
// one terminal takes part in no merge and is written with M = 1.
//
// Ported from Magic 8.3.694 (fc5daa9e) ext2spice/ext2spice.c:4514-4668
// (mkDevMerge, parallelDevs), 4670-4888 (mergeAttr, devMergeVisit) and
// 4890-4962 (update_w, devDistJunctVisit), Copyright (C) 1985, 1990
// Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: SPICE.

#include <vector>

#include "netlist/spice/flat_writer.hpp"

namespace hefesto::netlist::spice::detail {

namespace {

enum class Parallel : std::uint8_t { no, same, swapped };

// devMerge: a kept device and its nodes.
struct Candidate {
  std::size_t index = 0;
  float l = 0.0F;
  float w = 0.0F;
  std::optional<std::size_t> g;
  std::optional<std::size_t> s;
  std::optional<std::size_t> d;
  std::optional<std::size_t> b;
};

bool none_model(const FlatWriter& w, const InputDevice& dev) {
  return w.in.has_none_type && dev.type == "None";
}

Parallel mos_parallel(const FlatWriter& w, const Candidate& f1, const Candidate& f2) {
  const bool aggressive = w.settings.merge == MergeMode::aggressive;
  if (f1.b != f2.b || f1.g != f2.g || f1.l != f2.l || !(aggressive || f1.w == f2.w)) {
    return Parallel::no;
  }
  if (f1.d == f2.d && f1.s == f2.s) {
    return Parallel::same;
  }
  if (f1.s == f2.d && f1.d == f2.s) {
    return Parallel::swapped;
  }
  return Parallel::no;
}

Parallel cap_parallel(const FlatWriter& w, const Candidate& f1, const Candidate& f2) {
  const bool aggressive = w.settings.merge == MergeMode::aggressive;
  if (f1.g != f2.g || f1.s != f2.s) {
    return Parallel::no;
  }
  const InputDevice& d1 = w.devices[f1.index];
  const InputDevice& d2 = w.devices[f2.index];
  if (none_model(w, d1)) {
    return aggressive || d1.capacitance == d2.capacitance ? Parallel::same : Parallel::no;
  }
  return aggressive || (f1.l == f2.l && f1.w == f2.w) ? Parallel::same : Parallel::no;
}

// parallelDevs.
Parallel parallel(const FlatWriter& w, const Candidate& f1, const Candidate& f2) {
  const InputDevice& d1 = w.devices[f1.index];
  const InputDevice& d2 = w.devices[f2.index];
  if (d1.device_class != d2.device_class || d1.type != d2.type) {
    return Parallel::no;
  }
  switch (d1.device_class) {
    case DeviceClass::msubckt:
    case DeviceClass::mosfet:
    case DeviceClass::fet:
      return mos_parallel(w, f1, f2);
    case DeviceClass::asymmetric:
      return mos_parallel(w, f1, f2) == Parallel::same ? Parallel::same : Parallel::no;
    case DeviceClass::cap:
    case DeviceClass::caprev:
      return cap_parallel(w, f1, f2);
    default:
      return Parallel::no;
  }
}

// The kept device's new multiplier.
float merged_mult(const FlatWriter& w, const Candidate& fp, const Candidate& cfp) {
  const float kept = w.mult[cfp.index];
  const InputDevice& nd = w.devices[fp.index];
  const InputDevice& kd = w.devices[cfp.index];
  float m = kept + w.mult[fp.index];
  switch (nd.device_class) {
    case DeviceClass::msubckt:
    case DeviceClass::mosfet:
    case DeviceClass::asymmetric:
    case DeviceClass::fet:
      if (cfp.w > 0) {
        m = kept + (fp.w / cfp.w);
      }
      break;
    case DeviceClass::csubckt:
    case DeviceClass::cap:
    case DeviceClass::caprev:
      if (none_model(w, nd)) {
        if (kd.capacitance > 0) {
          m = kept + (nd.capacitance / kd.capacitance);
        }
      } else if (cfp.l > 0 && cfp.w > 0) {
        m = kept + ((fp.l * fp.w) / (cfp.l * cfp.w));
      }
      break;
    default:
      break;
  }
  return m;
}

void merge_attr(std::optional<std::string>& a1, const std::optional<std::string>& a2) {
  if (!a1) {
    a1 = a2;
  } else if (a2) {
    *a1 += *a2;
  }
}

std::string prefix_text(const InputDevice& dev) {
  std::string text;
  for (const std::string& part : dev.prefix) {
    text += part + "/";
  }
  return text;
}

enum class Outcome : std::uint8_t { merge, keep_looking, stop };

// The source/drain area attribute rules of devMergeVisit for a parallel
// pair: all flat, or hierarchical within one cell.
Outcome attribute_rule(FlatWriter& w, std::size_t index, const Candidate& cfp, Parallel mode) {
  InputDevice& dev = w.devices[index];
  InputDevice& kept = w.devices[cfp.index];
  const std::size_t nt = dev.terminals.size();
  InputTerminal& source = dev.terminals[1];
  InputTerminal& drain = dev.terminals[nt >= 3 ? 2 : 1];
  std::size_t cs = 1;
  std::size_t cd = 1;
  if (kept.terminals.size() >= 3) {
    (mode == Parallel::same ? cd : cs) = 2;
  }
  const bool hs = hier_sd_attr(source, w.settings.hier_ap);
  const bool hd = hier_sd_attr(drain, w.settings.hier_ap);
  const bool chs = hier_sd_attr(kept.terminals[cs], w.settings.hier_ap);
  const bool chd = hier_sd_attr(kept.terminals[cd], w.settings.hier_ap);
  if (!(chs || chd || hs || hd)) {
    return Outcome::merge;
  }
  const bool same_cell = kept.prefix == dev.prefix;
  if (!same_cell && (chs != hs || chd != hd)) {
    w.warn("Warning: conflicting SD attributes of parallel devs in cell: " + prefix_text(kept));
    return Outcome::stop;
  }
  if (!same_cell) {
    return Outcome::stop;
  }
  if (hs && !chs) {
    merge_attr(kept.terminals[cs].attributes, source.attributes);
  }
  if (hd && !chd) {
    merge_attr(kept.terminals[cd].attributes, drain.attributes);
  }
  return Outcome::merge;
}

}  // namespace

void FlatWriter::distribute_junction(const InputDevice& dev) {
  if (dev.terminals.size() < 2) {
    warn("outPremature");
    return;
  }
  const DeviceTypeInfo& info = type_info(dev.type);
  const int width = static_cast<int>(static_cast<float>(dev.width) * dev.scale);
  for (std::size_t i = 1; i < dev.terminals.size(); ++i) {
    const auto node = find_node(dev.terminals[i].node);
    const std::int32_t cls = i == 1 ? info.source_class : info.drain_class;
    if (!node || cls < 0) {
      continue;  // Magic adds to widths[-1] here, out of bounds
    }
    NodeClient& client = clients[*node];
    if (!client.exists) {
      init_client(*node);
    }
    if (client.widths.size() <= used_bit) {
      client.widths.resize(used_bit + 1, 0.0F);
    }
    if (static_cast<std::size_t>(cls) < client.widths.size()) {
      client.widths[static_cast<std::size_t>(cls)] += static_cast<float>(width);
    }
  }
}

void FlatWriter::merge_devices() {
  std::vector<Candidate> kept;  // newest last; searched newest first
  for (std::size_t i = 0; i < devices.size(); ++i) {
    const InputDevice& dev = devices[i];
    if (settings.distributed_junctions) {
      distribute_junction(dev);
    }
    if (dev.terminals.size() < 2) {
      warn("outPremature");
      continue;
    }
    Candidate fp{i,
                 static_cast<float>(static_cast<float>(dev.length) * dev.scale),
                 static_cast<float>(static_cast<float>(dev.width) * dev.scale),
                 find_node(dev.terminals[0].node),
                 find_node(dev.terminals[1].node),
                 find_node(dev.terminals[dev.terminals.size() >= 3 ? 2 : 1].node),
                 dev.substrate ? substrate(dev, false) : std::nullopt};
    bool merged_away = false;
    for (auto it = kept.rbegin(); it != kept.rend() && !merged_away; ++it) {
      const Parallel mode = parallel(*this, fp, *it);
      if (mode == Parallel::no) {
        continue;
      }
      const Outcome rule = attribute_rule(*this, i, *it, mode);
      if (rule == Outcome::stop) {
        break;
      }
      mult[it->index] = merged_mult(*this, fp, *it);
      killed[i] = true;
      ++merged;
      merged_away = true;
    }
    if (!merged_away) {
      kept.push_back(fp);
    }
  }
}

}  // namespace hefesto::netlist::spice::detail
