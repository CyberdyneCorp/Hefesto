// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `model` and `ignore` (lvs-setup-file "model command", "ignore command").
// Q-LVS-22 (fix, both netgen-compat modes): an `ignore` keyword that is
// neither `class` nor `shorted` is a `bad option` error.
//
// Owner: LVSSETUP.

#include <algorithm>
#include <array>
#include <functional>

#include "runtime.hpp"

namespace hefesto::lvs::setup {

namespace {

constexpr std::array<std::string_view, 17> kClassWords{
    "undefined", "nmos",     "pmos",  "pnp",    "npn",    "resistor", "capacitor",  "diode", "inductor",
    "module",    "blackbox", "xline", "moscap", "mosfet", "bjt",      "subcircuit", "copy"};

enum ClassWord : std::size_t {
  kUndefined, kNmos, kPmos, kPnp, kNpn, kResistor, kCapacitor, kDiode, kInductor,
  kModule, kBlackbox, kXline, kMoscap, kMosfet, kBjt, kSubcircuit, kCopy,
};

/// The class `word` gives a cell with `ports` ports; nullopt for a wrong count.
std::optional<DeviceClass> class_for(std::size_t word, std::size_t ports) {
  const auto pick = [ports](std::size_t n1, DeviceClass c1, std::size_t n2, DeviceClass c2) -> std::optional<DeviceClass> {
    if (ports == n1) {
      return c1;
    }
    if (ports == n2) {
      return c2;
    }
    return std::nullopt;
  };
  const auto exactly = [ports](std::size_t n, DeviceClass c) -> std::optional<DeviceClass> {
    return ports == n ? std::optional{c} : std::nullopt;
  };
  switch (word) {
    case kNmos:
      return pick(3, DeviceClass::nmos, 4, DeviceClass::nmos4);
    case kPmos:
      return pick(3, DeviceClass::pmos, 4, DeviceClass::pmos4);
    case kMosfet:
      return pick(3, DeviceClass::fet, 4, DeviceClass::fet);
    case kPnp:
      return exactly(3, DeviceClass::pnp);
    case kNpn:
      return exactly(3, DeviceClass::npn);
    case kBjt:
      return exactly(3, DeviceClass::bjt);
    case kResistor:
      return pick(2, DeviceClass::resistor, 3, DeviceClass::res3);
    case kCapacitor:
      return pick(2, DeviceClass::capacitor, 3, DeviceClass::cap3);
    case kDiode:
      return exactly(2, DeviceClass::diode);
    case kInductor:
      return exactly(2, DeviceClass::inductor);
    case kXline:
      return exactly(4, DeviceClass::xline);
    case kMoscap:
      return exactly(3, DeviceClass::ecap);
    case kModule:
    case kBlackbox:
      return DeviceClass::module;
    case kSubcircuit:
      return DeviceClass::subcircuit;
    default:
      return DeviceClass::undefined;
  }
}

std::string_view query_name(DeviceClass c, bool auto_blackbox) noexcept {
  switch (c) {
    case DeviceClass::nmos:
    case DeviceClass::nmos4:
      return "nmos";
    case DeviceClass::pmos:
    case DeviceClass::pmos4:
      return "pmos";
    case DeviceClass::fet:
      return "mosfet";
    case DeviceClass::bjt:
      return "bipolar";
    case DeviceClass::npn:
      return "npn";
    case DeviceClass::pnp:
      return "pnp";
    case DeviceClass::resistor:
    case DeviceClass::res3:
      return "resistor";
    case DeviceClass::capacitor:
    case DeviceClass::cap3:
    case DeviceClass::ecap:
      return "capacitor";
    case DeviceClass::subcircuit:
      return "subcircuit";
    case DeviceClass::module:
      return auto_blackbox ? "blackbox" : "module";
    default:
      return "undefined";
  }
}

cmd::CommandResult blackbox(Runtime& runtime, const Words& words) {
  static constexpr std::array<std::string_view, 2> kSwitch{"on", "off"};
  if (words.size() == 1) {
    return cmd::CommandResult::success(runtime.state().auto_blackbox ? 1 : 0);
  }
  const auto word = match_keyword(words[1], kSwitch);
  if (!word) {
    return command_error(bad_option("option", words[1], kSwitch));
  }
  runtime.state().auto_blackbox = *word == 0;
  return cmd::CommandResult::success();
}

/// `model <c> copy`: the class of the same-named cell in the other circuit.
std::string copy_class(Runtime& runtime, const Target& target) {
  Circuit* one = runtime.circuit(1);
  Circuit* two = runtime.circuit(2);
  if (one == nullptr || two == nullptr) {
    return "Circuits have not been queued for comparison.";
  }
  Circuit* own = target.side == 1 ? one : two;
  if (own->db.cells.at(*own->top).file != target.get().file) {
    return "The referenced netlist is not being compared.";
  }
  Circuit* other = target.side == 1 ? two : one;
  const auto id = other->db.find_cell(target.get().name, other->db.cells.at(*other->top).file);
  if (!id) {
    return "No such cell!";
  }
  target.get().device_class = other->db.cells.at(*id).device_class;
  return {};
}

std::string set_class(Runtime& runtime, const Target& target, std::size_t word) {
  if (word == kCopy) {
    return copy_class(runtime, target);
  }
  const auto c = class_for(word, target.get().ports.size());
  if (!c) {
    return "Wrong number of ports for device";
  }
  target.get().device_class = *c;
  return {};
}

void drop_instances(NetlistDb& db, FileNumber file, const std::function<bool(const Instance&)>& drop) {
  for (auto& cell : db.cells) {
    if (cell.file == file) {
      std::erase_if(cell.instances, drop);
    }
  }
}

bool shorted(const Instance& instance) {
  return !instance.connections.empty() && instance.connections.front() != kNoNet &&
         std::all_of(instance.connections.begin(), instance.connections.end(),
                     [&instance](NetId net) { return net == instance.connections.front(); });
}

}  // namespace

cmd::CommandResult model_command(Runtime& runtime, const Words& words) {
  if (words.empty() || words.size() > 2) {
    return wrong_args("model <valid_cellname> [<class>]");
  }
  if (words[0] == "blackbox") {
    return blackbox(runtime, words);
  }
  auto cells = runtime.resolve(words[0]);
  if (!cells) {
    return command_error(cells.error().text);
  }
  if (words.size() == 1) {
    const Cell& cell = cells.value().targets.front().get();
    return cmd::CommandResult::success(std::string{query_name(cell.device_class, runtime.state().auto_blackbox)});
  }
  const auto word = match_keyword(words[1], kClassWords);
  if (!word) {
    return command_error(bad_option("class", words[1], kClassWords));
  }
  for (const auto& target : cells.value().targets) {
    if (std::string error = set_class(runtime, target, *word); !error.empty()) {
      return command_error(std::move(error));
    }
  }
  return cmd::CommandResult::success();
}

cmd::CommandResult ignore_command(Runtime& runtime, const Words& words) {
  static constexpr std::array<std::string_view, 2> kKinds{"class", "shorted"};
  if (words.size() != 2) {
    return wrong_args("ignore class|shorted <valid_cellname>");
  }
  const auto kind = match_keyword(words[0], kKinds);
  if (!kind) {
    return command_error(bad_option("option", words[0], kKinds));  // Q-LVS-22
  }
  auto cells = runtime.resolve(words[1]);
  if (!cells) {
    return command_error(cells.error().text);
  }
  for (const auto& target : cells.value().targets) {
    const CellId klass = target.cell;
    const FileNumber file = target.get().file;
    if (*kind == 0) {
      drop_instances(target.circuit->db, file, [klass](const Instance& i) { return i.cell == klass; });
    } else {
      drop_instances(target.circuit->db, file, [klass](const Instance& i) { return i.cell == klass && shorted(i); });
    }
    runtime.state().ignored.emplace_back(
        target.side, IgnoredClass{target.get().name, *kind == 0 ? IgnoredClass::Kind::class_ : IgnoredClass::Kind::shorted});
  }
  return cmd::CommandResult::success();
}

}  // namespace hefesto::lvs::setup
