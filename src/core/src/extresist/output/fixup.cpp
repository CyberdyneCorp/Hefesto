// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ResFixUpConnections (extresist/output/output.hpp fix_up_connections;
// capability extresist-output "Naming of network nodes"): the device
// terminals of an output net are moved to the network nodes they land on,
// named `<net'>.t<k>`, and the input model's device records follow them so
// that the overlay's device records name those nodes.
//
// Ported from Magic 8.3.694 (fc5daa9e) resis/ResRex.c (ResWriteExtFile's
// device loop, ResFixUpConnections, ResFixDevName), Copyright (C) 1985,
// 1990 Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: RESCMD.

#include <cstddef>
#include <string>
#include <vector>

#include "hefesto/extresist/output/output.hpp"

namespace hefesto::extresist {

namespace {

// rd_terminals slots (resis.h RT_GATE, RT_SUBS, RT_SOURCE, RT_DRAIN).
constexpr std::size_t kGate = 0;
constexpr std::size_t kSubs = 1;
constexpr std::size_t kSource = 2;
constexpr std::size_t kDrain = 3;

// net': one trailing `!` or `#` removed.
std::string trimmed(std::string_view net) {
  if (!net.empty() && (net.back() == '!' || net.back() == '#')) {
    net.remove_suffix(1);
  }
  return std::string{net};
}

std::optional<std::size_t>& slot_of(InputDevice& d, Terminal role) {
  switch (role) {
    case Terminal::gate:
      return d.gate;
    case Terminal::source:
      return d.source;
    case Terminal::drain:
      return d.drain;
    case Terminal::subs:
      break;
  }
  return d.subs;
}

class Fixup {
 public:
  Fixup(InputModel& input, std::size_t net, Network& network, const compat::CompatSettings& compat,
        std::vector<std::string>& messages)
      : input_(input), net_(net), network_(network), compat_(compat), messages_(messages),
        base_(trimmed(input.nodes[net].name)) {}

  void device(std::size_t dev, std::size_t layout);
  [[nodiscard]] bool dont_kill() const { return dont_kill_; }

 private:
  [[nodiscard]] std::size_t term(std::size_t layout, std::size_t slot) const {
    const std::vector<std::size_t>& t = network_.devices[layout].terminals;
    return slot < t.size() ? t[slot] : kNoNode;
  }
  void set_term(std::size_t layout, std::size_t slot, std::size_t node) {
    std::vector<std::size_t>& t = network_.devices[layout].terminals;
    if (t.size() <= slot) {
      t.resize(slot + 1, kNoNode);
    }
    t[slot] = node;
  }
  [[nodiscard]] bool named(std::size_t node) const { return network_.nodes[node].named; }
  [[nodiscard]] bool on_net(const std::optional<std::size_t>& t) const { return t && *t == net_; }
  void next_name() { newname_ = base_ + ".t" + std::to_string(num_++); }
  // `if (x->rn_name != NULL && notdecremented) resNodeNum--`
  void undo_if_named(std::size_t node) {
    if (named(node) && notdecremented_) {
      --num_;
      notdecremented_ = false;
    }
  }
  void missing(std::string_view role, std::size_t layout);
  void rename(Terminal role, std::size_t dev, std::size_t node);
  void gate_and_subs(std::size_t dev, std::size_t layout);
  void permuted_source(std::size_t dev, std::size_t layout);
  void permuted_both(std::size_t dev, std::size_t layout);
  void permuted_drain(std::size_t dev, std::size_t layout);
  void single(Terminal role, std::size_t slot, std::size_t dev, std::size_t layout);
  std::size_t merge(std::size_t layout, std::size_t keep_slot, std::size_t gone_slot);

  InputModel& input_;
  std::size_t net_;
  Network& network_;
  const compat::CompatSettings& compat_;
  std::vector<std::string>& messages_;
  std::string base_;
  std::string newname_;
  int num_ = 0;  // resNodeNum
  bool notdecremented_ = true;
  bool dont_kill_ = false;
};

void Fixup::missing(std::string_view role, std::size_t layout) {
  const db::Rect& at = network_.devices[layout].inside;
  messages_.push_back("Missing " + std::string{role} + " connection of device at (" + std::to_string(at.x0) + " " +
                      std::to_string(at.y0) + ") on net " + base_);
  dont_kill_ = true;
}

// ResFixDevName, then `layoutnode->rn_name = <the input net's name>`.
void Fixup::rename(Terminal role, std::size_t dev, std::size_t node) {
  NetNode& layout_node = network_.nodes[node];
  const std::string name = layout_node.named ? layout_node.name : newname_;
  std::optional<std::size_t> index = input_.find(name);
  if (!index) {
    input_.nodes.emplace_back();
    input_.nodes.back().name = name;
    index = input_.nodes.size() - 1;
  }
  std::vector<NodeDevice>& on = input_.nodes[*index].devices;
  on.insert(on.begin(), NodeDevice{dev, role});
  slot_of(input_.devices[dev], role) = *index;
  layout_node.name = input_.nodes[*index].name;
  layout_node.named = true;
}

void Fixup::gate_and_subs(std::size_t dev, std::size_t layout) {
  if (on_net(input_.devices[dev].gate)) {
    const std::size_t gate = term(layout, kGate);
    if (gate == kNoNode) {
      missing("gate", layout);
    } else {
      if (named(gate)) {
        --num_;
        notdecremented_ = false;
      }
      rename(Terminal::gate, dev, gate);
      next_name();
    }
  }
  if (on_net(input_.devices[dev].subs)) {
    const std::size_t subs = term(layout, kSubs);
    if (subs == kNoNode) {
      missing("substrate", layout);
    } else {
      undo_if_named(subs);
      rename(Terminal::subs, dev, subs);
      next_name();
    }
  }
}

// ResMergeNodes of the layout device's two S/D nodes (the one that is the
// origin or a sink is kept) and ResDoneWithNode; returns the kept node.
std::size_t Fixup::merge(std::size_t layout, std::size_t keep_slot, std::size_t gone_slot) {
  std::size_t keep = term(layout, keep_slot);
  std::size_t gone = term(layout, gone_slot);
  if ((network_.nodes[gone].kind & (kNodeOrigin | kNodeSink)) != 0) {
    std::swap(keep, gone);
  }
  merge_nodes(network_, keep, gone, compat_);
  // ResMergeNodes moved the device's terminals onto the kept node.
  return term(layout, keep_slot) != kNoNode ? term(layout, keep_slot) : term(layout, gone_slot);
}

// The source branch of a symmetric device (doPermute).
void Fixup::permuted_source(std::size_t dev, std::size_t layout) {
  if (term(layout, kSource) == kNoNode && term(layout, kDrain) != kNoNode) {
    set_term(layout, kSource, term(layout, kDrain));
    set_term(layout, kDrain, kNoNode);
  }
  if (on_net(input_.devices[dev].drain)) {
    permuted_both(dev, layout);
    return;
  }
  std::size_t source = term(layout, kSource);
  if (source == kNoNode) {
    missing("terminal", layout);
    return;
  }
  if (term(layout, kDrain) != kNoNode) {
    if (term(layout, kDrain) != source) {
      source = merge(layout, kSource, kDrain);
    }
    set_term(layout, kDrain, kNoNode);
    if (named(source)) {
      --num_;
    }
  } else {
    undo_if_named(source);
  }
  rename(Terminal::source, dev, source);
}

// The source branch when the drain is on the net too: each terminal is
// renamed onto its own layout node (a device shorted onto the net has both
// on the source's node).
void Fixup::permuted_both(std::size_t dev, std::size_t layout) {
  const InputDevice& d = input_.devices[dev];
  if (term(layout, kSource) != kNoNode && term(layout, kDrain) == kNoNode && d.drain == d.source) {
    set_term(layout, kDrain, term(layout, kSource));
  }
  const std::size_t source = term(layout, kSource);
  if (source == kNoNode || term(layout, kDrain) == kNoNode) {
    missing("terminal", layout);
    return;
  }
  undo_if_named(source);
  rename(Terminal::source, dev, source);
  next_name();
  if (named(term(layout, kDrain))) {
    --num_;
  }
  rename(Terminal::drain, dev, term(layout, kDrain));
}

// The drain branch of a symmetric device whose source is not on the net.
void Fixup::permuted_drain(std::size_t dev, std::size_t layout) {
  if (term(layout, kDrain) == kNoNode && term(layout, kSource) != kNoNode) {
    set_term(layout, kDrain, term(layout, kSource));
    set_term(layout, kSource, kNoNode);
  }
  std::size_t drain = term(layout, kDrain);
  if (drain == kNoNode) {
    missing("terminal", layout);
    return;
  }
  if (term(layout, kSource) != kNoNode) {
    if (term(layout, kSource) != drain) {
      drain = merge(layout, kDrain, kSource);
    }
    set_term(layout, kSource, kNoNode);
    if (named(drain)) {
      --num_;
      notdecremented_ = false;
    }
  } else {
    undo_if_named(drain);
  }
  rename(Terminal::drain, dev, drain);
}

// The source or drain of a device with distinct S/D classes.
void Fixup::single(Terminal role, std::size_t slot, std::size_t dev, std::size_t layout) {
  const std::size_t node = term(layout, slot);
  if (node == kNoNode) {
    missing(role == Terminal::source ? "source" : "drain", layout);
    return;
  }
  undo_if_named(node);
  rename(role, dev, node);
  next_name();
}

void Fixup::device(std::size_t dev, std::size_t layout) {
  InputDevice& d = input_.devices[dev];
  if (!d.layout) {
    const NetDevice& ld = network_.devices[layout];
    d.layout = LayoutRecord{db::Point{ld.inside.x0, ld.inside.y0}, ld.length, ld.width, ld.perimeter, ld.area};
  }
  d.touched = true;
  notdecremented_ = true;
  next_name();
  gate_and_subs(dev, layout);
  const bool permute = input_.devices[dev].permute_sd;
  if (on_net(input_.devices[dev].source) && permute) {
    permuted_source(dev, layout);
  } else if (on_net(input_.devices[dev].source)) {
    single(Terminal::source, kSource, dev, layout);
  } else if (on_net(input_.devices[dev].drain) && permute) {
    permuted_drain(dev, layout);
  } else if (on_net(input_.devices[dev].drain)) {
    single(Terminal::drain, kDrain, dev, layout);
  } else {
    --num_;
  }
}

}  // namespace

bool fix_up_connections(InputModel& input, std::size_t net, Network& network, const compat::CompatSettings& compat,
                        std::vector<std::string>& messages) {
  Fixup fix(input, net, network, compat, messages);
  // The net's device list may grow while it is walked (a renamed terminal
  // can land on the net itself); Magic walks the list as it was.
  const std::vector<NodeDevice> devices = input.nodes[net].devices;
  for (std::size_t k = 0; k < devices.size(); ++k) {
    const std::optional<std::size_t> layout =
        k < network.input_devices.size() ? network.input_devices[k] : std::nullopt;
    if (layout && *layout < network.devices.size()) {
      fix.device(devices[k].device, *layout);
    }
  }
  return fix.dont_kill();
}

}  // namespace hefesto::extresist
