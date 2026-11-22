// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// spcdevVisit: one device line of the flat SPICE netlist (capability
// netlist-spice-devices "Device instance names" with Q-NETLIST-34 per mode,
// "Device eligibility and annotations", "MOSFET lines", "Resistor lines"
// with Q-NETLIST-5 and Q-NETLIST-31 per mode, "Capacitor, diode, BJT and
// voltage source lines", "Subcircuit-class device lines").
//
// Ported from Magic 8.3.694 (fc5daa9e) ext2spice/ext2spice.c:2521-2582
// (esOutputResistor), 2657-2665 (swapDrainSource), 2707-3422
// (spcdevVisit) and 3887-3935 (spcdevResPi), Copyright (C) 1985, 1990
// Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: SPICE.

#include <algorithm>
#include <utility>

#include "netlist/spice/flat_writer.hpp"
#include "netlist/spice/numbers.hpp"

namespace hefesto::netlist::spice::detail {

namespace {

bool mos_class(DeviceClass c) {
  return c == DeviceClass::mosfet || c == DeviceClass::asymmetric || c == DeviceClass::fet;
}

bool subckt_class(DeviceClass c) {
  switch (c) {
    case DeviceClass::subckt:
    case DeviceClass::rsubckt:
    case DeviceClass::csubckt:
    case DeviceClass::dsubckt:
    case DeviceClass::msubckt:
    case DeviceClass::veriloga:
      return true;
    default:
      return false;
  }
}

bool diode_class(DeviceClass c) {
  return c == DeviceClass::diode || c == DeviceClass::pdiode || c == DeviceClass::ndiode;
}

char letter(DeviceClass c) {
  if (mos_class(c)) {
    return 'M';
  }
  if (diode_class(c)) {
    return 'D';
  }
  switch (c) {
    case DeviceClass::bjt:
      return 'Q';
    case DeviceClass::res:
      return 'R';
    case DeviceClass::volt:
      return 'V';
    case DeviceClass::cap:
    case DeviceClass::caprev:
      return 'C';
    case DeviceClass::veriloga:
      return 'N';
    default:
      return 'X';
  }
}

// One device being written.
struct Dev {
  FlatWriter& w;
  InputDevice& dev;
  DeviceClass cls;
  Terminals t;
  std::int64_t l = 0;
  std::int64_t width = 0;
  float m = 1.0F;
  bool has_model = true;

  [[nodiscard]] InputTerminal& gate() const { return dev.terminals[t.gate]; }
  [[nodiscard]] InputTerminal& source() const { return dev.terminals[t.source]; }
  [[nodiscard]] InputTerminal& drain() const { return dev.terminals[t.drain]; }
  [[nodiscard]] std::size_t nterm() const { return dev.terminals.size(); }
  void out_term(const InputTerminal& term) const { w.out_node(term.node); }
};

// Terminals 1 and 2 are swapped when terminal 1's attribute is `D` or
// terminal 2's is `S`.
Terminals pick_terminals(InputDevice& dev) {
  Terminals t;
  if (dev.terminals.size() >= 2) {
    t.source = t.drain = 1;
    t.has_source = true;
  }
  if (dev.terminals.size() >= 3) {
    t.drain = 2;
    const auto& a1 = dev.terminals[1].attributes;
    const auto& a2 = dev.terminals[2].attributes;
    if ((a1 && *a1 == "D") || (a2 && *a2 == "S")) {
      std::swap(dev.terminals[1], dev.terminals[2]);
    }
  }
  return t;
}

bool enough_terminals(const Dev& d) {
  if (subckt_class(d.cls)) {
    return true;
  }
  if (diode_class(d.cls)) {
    if (d.nterm() < 2 && !d.dev.substrate) {
      d.w.warn("Diode has only one terminal");
      return false;
    }
    return true;
  }
  if (d.nterm() < 2) {
    d.w.warn("Device other than subcircuit has only one terminal");
    return false;
  }
  return true;
}

// The instance name: the gate attribute (Q-NETLIST-34: prefixed with the
// instance path, joined with `/` only with magic-compat OFF), or a counter.
std::string gate_name(const Dev& d) {
  std::string prefix;
  for (const std::string& part : d.dev.prefix) {
    prefix += (prefix.empty() ? "" : "/") + part;
  }
  if (!prefix.empty() && !d.w.compat.magic_compat) {
    prefix += "/";
  }
  return prefix + *d.gate().attributes;
}

std::string counter_name(const Dev& d) {
  FlatWriter& w = d.w;
  const bool tee = w.settings.resistor_tee;
  switch (d.cls) {
    case DeviceClass::res:
      return std::to_string(w.res_num++) + (tee ? "A" : "");
    case DeviceClass::diode:
    case DeviceClass::ndiode:
    case DeviceClass::pdiode:
      return std::to_string(w.diode_num++);
    case DeviceClass::cap:
    case DeviceClass::caprev:
      return std::to_string(w.cap_num++);
    case DeviceClass::volt:
      return std::to_string(w.volt_num++);
    default:
      break;
  }
  if (subckt_class(d.cls)) {
    return std::to_string(w.sbck_num++) + (d.cls == DeviceClass::rsubckt && tee ? "A" : "");
  }
  return std::to_string(w.dev_num++);
}

void write_name(const Dev& d) {
  d.w.out += letter(d.cls);
  d.w.out += d.gate().attributes ? gate_name(d) : counter_name(d);
}

// spcdevResPi: half of the body node's capacitance moves to each end.
void resistor_pi(const Dev& d) {
  FlatWriter& w = d.w;
  const auto gate = w.find_node(d.gate().node);
  const auto drain = w.find_node(d.drain().node);
  const auto source = w.find_node(d.source().node);
  if (!gate || !drain || !source) {
    return;
  }
  const float rescap = static_cast<float>(dbl(w.node_caps[*gate]) / 2.0);
  w.node_caps[*source] += rescap;
  w.node_caps[*drain] += rescap;
  w.node_caps[*gate] = 0;
}

void type_and_params(const Dev& d, bool subdone) {
  d.w.out += " " + d.dev.type;
  d.w.write_params(d.dev, d.l, d.width, d.m, subdone);
}

void multiplier(const Dev& d) {
  if (d.m != 1.0F) {
    d.w.out += " M=" + format_g(dbl(d.m));
  }
}

// esOutputResistor.
void output_resistor(const Dev& d, const InputTerminal& t1, const InputTerminal& t2, int dscale) {
  d.out_term(t1);
  d.out_term(t2);
  if (!d.has_model) {
    d.w.out += " " + format_f((dbl(d.dev.resistance) / static_cast<double>(dscale)) / dbl(d.m));
    d.w.write_params(d.dev, d.l, d.width, d.m, false);
    return;
  }
  d.w.write_value(d.dev);
  const bool subdone = d.w.settings.format == Dialect::cdl && d.w.write_sub_param(d.dev);
  d.w.out += " " + d.dev.type;
  d.w.geometry(d.l, d.width, d.dev.scale, dscale);
  d.w.write_params(d.dev, d.l, d.width, d.m, subdone);
  multiplier(d);
}

void resistor(const Dev& d) {
  if (!d.w.settings.resistor_tee) {
    output_resistor(d, d.source(), d.drain(), 1);
    resistor_pi(d);
    return;
  }
  output_resistor(d, d.gate(), d.source(), 2);
  d.w.out += "\n";
  d.w.out += letter(d.cls);
  d.w.out += d.gate().attributes ? gate_name(d) + "B" : std::to_string(d.w.res_num - 1) + "B";
  output_resistor(d, d.gate(), d.drain(), 2);
}

bool declares_substrate(const FlatWriter& w, const std::string& type) {
  if (const auto* params = w.declared(type)) {
    for (const ParamDecl& p : *params) {
      if (!p.key.empty() && p.key[0] == 's') {
        return true;
      }
    }
  }
  return false;
}

// The substrate pin, the model and the parameters of a subcircuit device;
// true when a substrate pin was written.
bool subckt_tail(const Dev& d) {
  bool pin = false;
  if (!declares_substrate(d.w, d.dev.type) && d.dev.substrate) {
    d.w.out += " ";
    (void)d.w.substrate(d.dev, true);
    pin = true;
  }
  if (d.w.settings.format == Dialect::cdl) {
    d.w.out += " /";
  }
  type_and_params(d, false);
  multiplier(d);
  return pin;
}

// Q-NETLIST-5: the second half of a flat rsubckt tee, written only with
// magic-compat OFF (Magic's code is unreachable).
void rsubckt_second_half(const Dev& d) {
  d.w.out += "\nX" + std::to_string(d.w.sbck_num - 1) + "B";
  d.out_term(d.gate());
  d.out_term(d.drain());
  if (subckt_tail(d)) {
    if (const auto gate = d.w.find_node(d.gate().node)) {
      d.w.node_caps[*gate] = 0;
    }
  }
}

// The terminals after the gate (ext2spice.c:2972-3028).
void subckt_middle(Dev& d) {
  if (d.cls == DeviceClass::msubckt) {
    if (d.nterm() > 2) {
      d.out_term(d.source());
    }
    return;
  }
  if (d.cls == DeviceClass::dsubckt) {
    return;
  }
  if (d.nterm() > 1) {
    d.out_term(d.source());
  }
  if (d.cls == DeviceClass::rsubckt && d.w.settings.resistor_tee) {
    d.l /= 2;
    d.out_term(d.gate());
  } else if (d.nterm() > 2) {
    d.out_term(d.drain());
    if (d.cls == DeviceClass::rsubckt) {
      resistor_pi(d);
    }
  }
}

void subcircuit(Dev& d) {
  if (d.cls == DeviceClass::msubckt || d.cls == DeviceClass::dsubckt) {
    d.out_term(d.drain());
  }
  if (d.cls != DeviceClass::rsubckt) {
    d.out_term(d.gate());
  }
  subckt_middle(d);
  // Magic writes terminals 3 .. n-2 here (the loop starts one late); with
  // magic-compat OFF every terminal from 3 on is written.
  const std::size_t end = d.w.compat.magic_compat ? d.nterm() - 1 : d.nterm();
  for (std::size_t i = 3; i < end && d.nterm() > 3; ++i) {
    d.out_term(d.dev.terminals[i]);
  }
  (void)subckt_tail(d);
  if (d.cls == DeviceClass::rsubckt && d.w.settings.resistor_tee && !d.w.compat.magic_compat) {
    rsubckt_second_half(d);
  }
}

void bjt(const Dev& d) {
  FlatWriter& w = d.w;
  if (d.dev.substrate) {
    w.out_node(*d.dev.substrate);
  } else {
    w.out += " errGnd!";  // Magic dereferences a null substrate here
  }
  d.out_term(d.gate());
  const auto g = w.find_node(d.gate().node);
  const bool same = g && g == w.find_node(d.source().node);
  d.out_term(same ? d.drain() : d.source());
  const bool subdone = w.settings.format == Dialect::cdl && w.write_sub_param(d.dev);
  type_and_params(d, subdone);
}

void volt(const Dev& d) {
  if (d.nterm() > 1) {
    d.out_term(d.source());
  }
  if (d.nterm() > 2) {
    d.out_term(d.drain());
  }
  d.w.out += " 0.0";
}

void diode_other_end(const Dev& d) {
  if (d.nterm() > 1) {
    d.out_term(d.source());
  } else if (d.dev.substrate) {
    d.w.out_node(*d.dev.substrate);
  }
}

void diode(const Dev& d) {
  if (d.cls == DeviceClass::ndiode) {
    diode_other_end(d);
    d.out_term(d.gate());
  } else {
    d.out_term(d.gate());
    diode_other_end(d);
  }
  const bool subdone = d.w.settings.format == Dialect::cdl && d.w.write_sub_param(d.dev);
  type_and_params(d, subdone);
}

void capacitor(const Dev& d) {
  if (d.cls == DeviceClass::caprev) {
    d.out_term(d.source());
    d.out_term(d.gate());
  } else {
    d.out_term(d.gate());
    d.out_term(d.source());
  }
  if (!d.has_model) {
    d.w.out += " " + si(static_cast<float>(1.0E-15 * dbl(d.m) * dbl(d.dev.capacitance)));
    d.w.write_params(d.dev, d.l, d.width, d.m, false);
    return;
  }
  d.w.write_value(d.dev);
  const bool subdone = d.w.settings.format == Dialect::cdl && d.w.write_sub_param(d.dev);
  d.w.out += " " + d.dev.type;
  d.w.geometry(d.l, d.width, d.dev.scale, 1);
  d.w.write_params(d.dev, d.l, d.width, d.m, subdone);
  multiplier(d);
}

void mos_attributes(const Dev& d) {
  if (d.w.settings.no_attributes) {
    return;
  }
  const auto& g = d.gate().attributes;
  const auto& s = d.source().attributes;
  const auto& dr = d.drain().attributes;
  const bool have_s = s && !s->empty();
  const bool have_d = dr && !dr->empty();
  if (g || have_s || have_d) {
    d.w.out += "\n**devattr";
  }
  d.w.out += g ? " g=" + *g : "";
  d.w.out += have_s ? " s=" + *s : "";
  d.w.out += have_d ? " d=" + *dr : "";
}

void mos_substrate_ap(const Dev& d, std::optional<std::size_t> sub) {
  FlatWriter& w = d.w;
  w.out += " * ";
  const DeviceTypeInfo& info = w.type_info(d.dev.type);
  if (info.substrate_class < 0) {
    const auto& names = w.in.type_names;
    const auto it = std::find(names.begin(), names.end(), d.dev.type);
    const long index = it == names.end() ? -1 : static_cast<long>(it - names.begin());
    w.warn("error: subap for devtype " + std::to_string(index) + " unspecified");
    w.out += "asub=0 psub=0";
  } else if (sub) {
    w.substrate_ap(*sub, info.substrate_class, d.dev.scale, d.m);
  } else {
    w.out += "asub=0 psub=0";
  }
}

void mosfet(const Dev& d) {
  FlatWriter& w = d.w;
  d.out_term(d.drain());
  d.out_term(d.gate());
  d.out_term(d.source());
  std::optional<std::size_t> sub;
  if (d.dev.substrate) {
    w.out += " ";
    sub = w.substrate(d.dev, true);
  }
  w.out += " " + d.dev.type;
  w.geometry(d.l, d.width, d.dev.scale, 1);
  w.write_params(d.dev, d.l, d.width, d.m, false);
  multiplier(d);
  const DeviceTypeInfo& info = w.type_info(d.dev.type);
  const bool hier_s = hier_sd_attr(d.source(), w.settings.hier_ap);
  const bool hier_d = hier_sd_attr(d.drain(), w.settings.hier_ap);
  const bool sub_ap = has_ext_attr(d.gate().attributes, "aps");
  w.out += "\n+ ";
  w.terminal_ap(d.drain(), w.find_node(d.drain().node), info.drain_class, d.dev.scale, "ad", "pd", d.m,
                hier_d ? -1 : d.width);
  w.terminal_ap(d.source(), w.find_node(d.source().node), info.source_class, d.dev.scale, "as", "ps", d.m,
                hier_s ? -1 : d.width);
  if (sub_ap) {
    mos_substrate_ap(d, sub);
  }
  mos_attributes(d);
}

void body(Dev& d) {
  if (mos_class(d.cls)) {
    mosfet(d);
  } else if (subckt_class(d.cls)) {
    subcircuit(d);
  } else if (diode_class(d.cls)) {
    diode(d);
  } else if (d.cls == DeviceClass::bjt) {
    bjt(d);
  } else if (d.cls == DeviceClass::res) {
    resistor(d);
  } else if (d.cls == DeviceClass::volt) {
    volt(d);
  } else {
    capacitor(d);
  }
}

// A resistor or capacitor without a model (esNoModelType or "None").
bool unmodeled(const InputDevice& dev) {
  return dev.type == "None";
}

}  // namespace

void FlatWriter::device(std::size_t index) {
  InputDevice& dev = devices[index];
  if (dev.terminals.empty() || (merging() && killed[index])) {
    return;
  }
  Dev d{*this, dev, dev.device_class, pick_terminals(dev)};
  d.l = dev.length;
  d.width = dev.width;
  d.m = merging() ? mult[index] : 1.0F;
  if (!enough_terminals(d)) {
    return;
  }
  // The deprecated BiCMOS hack: a device of type npn is a BJT.
  if (dev.type == "npn") {
    d.cls = DeviceClass::bjt;
  }
  const bool rc = d.cls == DeviceClass::res || d.cls == DeviceClass::cap || d.cls == DeviceClass::caprev;
  d.has_model = !(rc && unmodeled(dev));
  // Only a MOSFET with two terminals has its source and drain tied; the
  // gate and source of another class are never the same terminal.
  if (mos_class(d.cls) && d.t.source == d.t.drain) {
    out += settings.format == Dialect::ngspice ? "; ** SOURCE/DRAIN TIED\n" : "** SOURCE/DRAIN TIED\n";
  }
  write_name(d);
  body(d);
  out += "\n";
}

}  // namespace hefesto::netlist::spice::detail
