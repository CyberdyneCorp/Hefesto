// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Native IHP sg13g2 MOSFET generators (design D10; D-29), the four
// `sg13_{lv,hv}_{n,p}mos` devices and their RF variants: a port of the
// procedures every `sg13g2::<device>_draw` and `_check` of
// ihp-sg13g2-fet.tcl delegates to, issuing the same engine commands with the
// same words. PDK ref (IHP-Open-PDK 5e6d592e):
//   ihp-sg13g2.tcl:134-155         sg13g2::ruleset
//   ihp-sg13g2-util.tcl:156-297    getbox, minareabox, unionbox, draw_contact
//   ihp-sg13g2-util.tcl:299-351    metal_ring (its first pushbox is never popped)
//   ihp-sg13g2-util.tcl:353-660    guard_ring
//   ihp-sg13g2-util.tcl:691-787    i2u (%.3f), u2i (round), spice2float, 3digitpastdecimal
//   ihp-sg13g2-via.tcl:415-469     via1_draw
//   ihp-sg13g2-fet.tcl:44-133      the _defaults dictionaries
//   ihp-sg13g2-fet.tcl:344-871     mos_device
//   ihp-sg13g2-fet.tcl:877-1233    mos_draw
//   ihp-sg13g2-fet.tcl:1239-1388   the per-device draw dictionaries
//   ihp-sg13g2-fet.tcl:1394-1555   mos_check
// Variable names follow the Tcl so the two can be read side by side.

#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/pcell/framework/tcl_list.hpp"
#include "pcell/native/builtin.hpp"
#include "pcell/native/port.hpp"

namespace hefesto::pcell::native {

namespace {

// ---- ihp-sg13g2-util.tcl / ihp-sg13g2-via.tcl ------------------------------

// ::tcl::mathfunc::ceil (a double).
Val ceil(const Val& v) {
  return Val::real(std::ceil(v.num().as_double()));
}

void minareabox(Engine& e, const Val& minarea, std::string_view dir) {
  const List curbox = e.box_values();
  const Val oscale{e.run({"cif", "scale", "out"})};
  const Val iarea = div(minarea, mul(oscale, oscale));
  const Val curw = sub(curbox.at(2), curbox.at(0));
  const Val curh = sub(curbox.at(3), curbox.at(1));
  const Val cura = mul(curw, curh);
  if (!lt(cura, iarea)) {
    return;
  }
  if (dir == "horz") {
    const Val neww = sub(div(iarea, curh), curw);
    const Val hneww = ceil(div(neww, "2.0"));
    e.run({"box", "grow", "e", hneww.str() + "i"});
    e.run({"box", "grow", "w", hneww.str() + "i"});
  } else {
    const Val newh = sub(div(iarea, curw), curh);
    const Val hnewh = ceil(div(newh, "2.0"));
    e.run({"box", "grow", "h", hnewh.str() + "i"});  // sic: "h", as in the PDK
    e.run({"box", "grow", "s", hnewh.str() + "i"});
  }
}

void via1_draw(Engine& e, std::string_view dir0 = "default", const Val& layers = "2", const Val& minarea = "0") {
  const Val w = e.i2u(Val{e.run({"box", "width"})});
  const Val h = e.i2u(Val{e.run({"box", "height"})});
  if (lt(w, "0.2")) {
    e.puts(true, "Via1 width must be at least 0.2um");
    return;
  }
  if (lt(h, "0.2")) {
    e.puts(true, "Via1 height must be at least 0.2um");
    return;
  }
  std::string_view dir = dir0;
  if (dir == "default") {
    dir = lt(w, h) ? "vert" : "horz";
  }
  e.run({"suspendall"});
  e.run({"paint", "via1"});
  const bool vertical = dir == "vert";
  e.pushbox();
  e.run({"box", "grow", vertical ? "e" : "n", "0.045um"});
  e.run({"box", "grow", vertical ? "w" : "s", "0.045um"});
  e.run({"paint", "m2"});
  e.popbox();
  if (gt(layers, "1")) {
    e.pushbox();
    e.run({"box", "grow", "c", "0.005um"});
    e.run({"box", "grow", vertical ? "n" : "e", "0.045um"});
    e.run({"box", "grow", vertical ? "s" : "w", "0.045um"});
    minareabox(e, minarea, dir);
    e.run({"paint", "m1"});
    e.popbox();
  }
  e.run({"resumeall"});
}

// draw_contact with aorient "full" (every diffusion overlaps on all sides).
List draw_contact(Engine& e, const Val& w0, const Val& h0, const Val& s, const Val& o, const Val& x,
                  const Val& atype, const Val& ctype, const Val& mtype, std::string_view orient = "vert") {
  e.pushbox();
  e.run({"box", "size", "0", "0"});
  const Val w = lt(w0, x) ? x : w0;
  const Val h = lt(h0, x) ? x : h0;
  const Val hw = div(w, "2.0");
  const Val hh = div(h, "2.0");
  e.grow("n", hh);
  e.grow("s", hh);
  e.grow("e", hw);
  e.grow("w", hw);
  e.paint(ctype);
  e.pushbox();
  e.grow("n", s);
  e.grow("s", s);
  e.grow("e", s);
  e.grow("w", s);
  e.paint(atype);
  List extents = getbox(e);
  e.popbox();
  if (orient == "vert" || orient == "full") {
    e.grow("n", o);
    e.grow("s", o);
  }
  if (orient == "horz" || orient == "full") {
    e.grow("e", o);
    e.grow("w", o);
  }
  e.paint(mtype);
  e.popbox();
  return extents;
}

// One side of a ring: move to the side, grow across and along, paint.
void ring_side(Engine& e, std::string_view dir, const Val& offset, const Val& across, const Val& along,
               const Val& type) {
  const bool horizontal = dir == "n" || dir == "s";
  e.pushbox();
  e.move(dir, offset);
  e.grow(horizontal ? "n" : "e", across);
  e.grow(horizontal ? "s" : "w", across);
  e.grow(horizontal ? "e" : "n", along);
  e.grow(horizontal ? "w" : "s", along);
  e.paint(type);
  e.popbox();
}

// sg13g2::metal_ring: its opening pushbox is never popped (the PDK's code).
void metal_ring(Engine& e, const Val& gw, const Val& gh, const Dict& parameters) {
  Locals v;
  v.set("gw", gw);
  v.set("gh", gh);
  v.load(parameters);
  const Val hx = div(v["contact_size"], "2.0");
  const Val hw = div(v["gw"], "2.0");
  const Val hh = div(v["gh"], "2.0");
  e.pushbox();
  e.run({"box", "size", "0", "0"});
  const Val hmetw = div(add(v["gw"], v["contact_size"]), "2.0");
  const Val hmeth = div(add(v["gh"], v["contact_size"]), "2.0");
  ring_side(e, "n", hh, hx, hmetw, "m1");
  ring_side(e, "s", hh, hx, hmetw, "m1");
  ring_side(e, "e", hw, hx, hmeth, "m1");
  ring_side(e, "w", hw, hx, hmeth, "m1");
}

Locals guard_ring_locals(const Val& gw, const Val& gh, const Dict& parameters) {
  Locals v;
  for (const auto& [name, value] : std::vector<std::pair<const char*, const char*>>{
           {"rlcov", "100"},          {"tbcov", "100"},         {"grc", "1"},
           {"glc", "1"},              {"gtc", "1"},             {"gbc", "1"},
           {"viagb", "0"},            {"viagt", "0"},           {"viagr", "0"},
           {"viagl", "0"},            {"full_metal", "1"},      {"guard_sub_type", "pwell"},
           {"guard_sub_surround", "0"}, {"plus_diff_type", "nsd"}, {"plus_contact_type", "nsc"},
           {"plus_contact_size", "0"}, {"sub_type", "pwell"},   {"diff_width", "0"},
           {"bulk", ""}}) {
    v.set(name, Val{value});
  }
  v.set("gw", gw);
  v.set("gh", gh);
  v.load(parameters);
  if (!parameters.has("guard_sub_type")) {
    v.set("guard_sub_type", v["sub_type"]);
  }
  if (eq(v["plus_contact_size"], "0")) {
    v.set("plus_contact_size", v["contact_size"]);
  }
  v.set("hx", div(v["plus_contact_size"], "2.0"));
  v.set("hw", div(v["gw"], "2.0"));
  v.set("hh", div(v["gh"], "2.0"));
  v.set("difft", add(v["plus_contact_size"], v["diff_surround"], v["diff_surround"]));
  if (lt(v["difft"], v["diff_width"])) {
    v.set("difft", v["diff_width"]);
  }
  v.set("hdifft", div(v["difft"], "2.0"));
  v.set("hdiffw", div(add(v["gw"], v["difft"]), "2.0"));
  v.set("hdiffh", div(add(v["gh"], v["difft"]), "2.0"));
  return v;
}

// A diffusion side of the guard ring with its guard substrate.
void guard_side(Engine& e, const Locals& v, std::string_view dir, const Val& along) {
  const bool horizontal = dir == "n" || dir == "s";
  e.grow(horizontal ? "n" : "e", v["hdifft"]);
  e.grow(horizontal ? "s" : "w", v["hdifft"]);
  e.grow(horizontal ? "e" : "n", along);
  e.grow(horizontal ? "w" : "s", along);
  e.paint(v["plus_diff_type"]);
  if (gt(v["guard_sub_surround"], "0")) {
    e.grow("c", v["guard_sub_surround"]);
    e.paint(v["guard_sub_type"]);
  }
}

void guard_via(Engine& e, const Locals& v, const Val& coverage, std::string_view side) {
  const bool horizontal = side == "s" || side == "n";
  const Val& via_size = v["via_size"];
  const Val& full = horizontal ? v["gw"] : v["gh"];
  e.pushbox();
  Val along = mul(sub(full, via_size), div(abs(coverage), "100.0"));
  if (lt(along, via_size)) {
    along = via_size;
  }
  const Val across = via_size;
  e.move(side, horizontal ? v["hh"] : v["hw"]);
  e.grow(horizontal ? "n" : "e", div(across, "2.0"));
  e.grow(horizontal ? "s" : "w", div(across, "2.0"));
  const std::string anchor = first_char(coverage);
  const std::string_view low = horizontal ? "w" : "s";
  const std::string_view high = horizontal ? "e" : "n";
  if (anchor == "+") {
    e.move(low, div(sub(full, via_size), "2.0"));
    e.grow(high, along);
  } else if (anchor == "-") {
    e.move(high, div(sub(full, via_size), "2.0"));
    e.grow(low, along);
  } else {
    e.grow(high, div(along, "2.0"));
    e.grow(low, div(along, "2.0"));
  }
  via1_draw(e, horizontal ? "horz" : "vert");
  e.popbox();
}

void guard_contacts(Engine& e, const Locals& v) {
  const Val& pcs = v["plus_contact_size"];
  const Val inset = mul(add(v["metal_surround"], v["metal_spacing"]), "2.0");
  Val ch = mul(sub(v["gh"], pcs, inset), div(v["rlcov"], "100.0"));
  if (lt(ch, pcs)) {
    ch = pcs;
  }
  Val cw = mul(sub(v["gw"], pcs, inset), div(v["tbcov"], "100.0"));
  if (lt(cw, pcs)) {
    cw = pcs;
  }
  const auto contact = [&](const char* dir, const Val& half, const Val& w, const Val& h, const char* orient) {
    e.pushbox();
    e.move(dir, half);
    static_cast<void>(draw_contact(e, w, h, v["diff_surround"], v["metal_surround"], pcs, v["plus_diff_type"],
                                   v["plus_contact_type"], "m1", orient));
    e.popbox();
  };
  if (gt(v["tbcov"], "0.0")) {
    if (eq(v["gtc"], "1")) {
      contact("n", v["hh"], cw, "0", "horz");
    }
    if (eq(v["gbc"], "1")) {
      contact("s", v["hh"], cw, "0", "horz");
    }
  }
  if (gt(v["rlcov"], "0.0")) {
    if (eq(v["grc"], "1")) {
      contact("e", v["hw"], "0", ch, "vert");
    }
    if (eq(v["glc"], "1")) {
      contact("w", v["hw"], "0", ch, "vert");
    }
  }
}

List guard_ring(Engine& e, const Val& gw, const Val& gh, const Dict& parameters) {
  const Locals v = guard_ring_locals(gw, gh, parameters);
  e.pushbox();
  e.run({"box", "size", "0", "0"});
  e.pushbox();
  e.move("n", v["hh"]);
  guard_side(e, v, "n", v["hdiffw"]);
  e.popbox();
  e.pushbox();
  e.move("s", v["hh"]);
  e.pushbox();
  guard_side(e, v, "s", v["hdiffw"]);
  e.popbox();
  if (ne(v["bulk"], "")) {
    e.run({"label", v["bulk"].str(), "c", v["plus_diff_type"].str()});
    e.run({"select", "area", "label"});
    e.run({"port", "make"});
  }
  e.popbox();
  e.pushbox();
  e.move("e", v["hw"]);
  guard_side(e, v, "e", v["hdiffh"]);
  e.popbox();
  e.pushbox();
  e.move("w", v["hw"]);
  guard_side(e, v, "w", v["hdiffh"]);
  e.popbox();
  if (v["full_metal"].truth()) {
    const Val hmetw = div(add(v["gw"], v["plus_contact_size"]), "2.0");
    const Val hmeth = div(add(v["gh"], v["plus_contact_size"]), "2.0");
    ring_side(e, "n", v["hh"], v["hx"], hmetw, "m1");
    ring_side(e, "s", v["hh"], v["hx"], hmetw, "m1");
    ring_side(e, "e", v["hw"], v["hx"], hmeth, "m1");
    ring_side(e, "w", v["hw"], v["hx"], hmeth, "m1");
  }
  guard_contacts(e, v);
  for (const auto& [key, side] : std::vector<std::pair<const char*, const char*>>{
           {"viagb", "s"}, {"viagt", "n"}, {"viagr", "e"}, {"viagl", "w"}}) {
    if (ne(v[key], "0")) {
      guard_via(e, v, v[key], side);
    }
  }
  e.pushbox();
  e.grow("e", v["hw"]);
  e.grow("w", v["hw"]);
  e.grow("n", v["hh"]);
  e.grow("s", v["hh"]);
  const std::string bbox = e.run({"box", "values"});
  e.run({"property", "FIXED_BBOX", bbox});
  e.grow("c", v["hx"]);
  e.grow("c", v["diff_surround"]);
  e.grow("c", v["sub_surround"]);
  e.paint(v["sub_type"]);
  List cext = getbox(e);
  e.popbox();
  e.popbox();
  return cext;
}

// ---- ihp-sg13g2-fet.tcl: mos_device ----------------------------------------------

struct Device {
  Engine& e;
  Dict parameters;
  Locals v;
};

void make_port(Engine& e, const Val& name, const Val& type) {
  e.run({"label", name.str(), "c", type.str()});
  e.run({"select", "area", "label"});
  e.run({"port", "make"});
}

void mos_device_defaults(Locals& v) {
  for (const auto& [name, value] : std::vector<std::pair<const char*, const char*>>{
           {"is_rf", "0"},        {"topc", "1"},          {"botc", "1"},        {"gate_ring", "1"},
           {"guard", "1"},        {"doports", "0"},       {"diffcov", "100"},   {"polycov", "100"},
           {"viasrc", "100"},     {"viadrn", "100"},      {"viagate", "100"},   {"evens", "1"},
           {"dev_sub_type", ""},  {"dev_sub_dist", "0"},  {"diff_overlap_cont", "0"}, {"gshield", "0"},
           {"conn_gates", "1"},   {"poly_merge", "0"},    {"drain_proc", ""},   {"drain", ""},
           {"source", ""},        {"gate", ""}}) {
    v.set(name, Val{value});
  }
}

void contact_flags(Locals& v, const Dict& parameters) {
  if (eq(v["gate_ring"], "1")) {
    v.set("topc", "1");
    v.set("botc", "1");
    v.set("oldtopc", "1");
    v.set("oldbotc", "1");
    return;
  }
  if (!parameters.has("oldtopc")) {
    v.set("oldtopc", v["topc"]);
  }
  if (!parameters.has("oldbotc")) {
    v.set("oldbotc", v["botc"]);
  }
}

// Diffusion and poly (ihp-sg13g2-fet.tcl:394-458): the poly box and the
// device box (`cext`) are returned, the gate rectangle saved for last.
List draw_diff_and_poly(Device& d, std::vector<std::string>& gaterect, List& polybox) {
  Engine& e = d.e;
  Locals& v = d.v;
  e.pushbox();
  v.set("hw", div(v["w"], "2.0"));
  v.set("hl", div(v["l"], "2.0"));
  e.grow("n", v["hw"]);
  e.grow("s", v["hw"]);
  e.grow("e", v["hl"]);
  e.grow("w", v["hl"]);
  const bool evens0 = eq(v["evens"], "0");
  v.set("dside", evens0 ? "e" : "w");
  v.set("sside", evens0 ? "w" : "e");
  e.pushbox();
  const Val& grow_diff =
      gt(v["diff_extension"], v["gate_to_diffcont"]) ? v["diff_extension"] : v["gate_to_diffcont"];
  e.grow(v["dside"].str(), grow_diff);
  e.grow(v["sside"].str(), grow_diff);
  e.grow("e", v["diff_surround"]);
  e.grow("e", div(v["sdcontact_size"], "2.0"));
  e.grow("w", v["diff_surround"]);
  e.grow("w", div(v["sdcontact_size"], "2.0"));
  e.paint(v["diff_type"]);
  e.popbox();
  e.pushbox();
  if (gt(v["gate_extension"], v["gate_to_polycont"])) {
    e.grow("n", v["gate_extension"]);
    e.grow("s", v["gate_extension"]);
  } else {
    for (const auto& [dir, flag] : std::vector<std::pair<const char*, const char*>>{{"n", "topc"}, {"s", "botc"}}) {
      if (v[flag].truth()) {
        e.grow(dir, v["gate_to_polycont"]);
        e.grow(dir, v["poly_surround"]);
        e.grow(dir, div(v["contact_size"], "2.0"));
      } else {
        e.grow(dir, v["gate_extension"]);
      }
    }
  }
  e.paint(v["poly_type"]);
  polybox = getbox(e);
  e.popbox();
  List cext = getbox(e);
  auto split = split_tcl_list(e.run({"box", "values"}));
  gaterect = split.ok() ? std::move(split).value() : std::vector<std::string>{};
  e.popbox();
  return cext;
}

// The contact sizes and the via adjustments (ihp-sg13g2-fet.tcl:515-560).
void contact_sizes(Locals& v) {
  const Val tsurround = add(v["diff_surround"], v["diff_overlap_cont"]);
  v.set("cdw", sub(v["w"], mul(tsurround, "2")));
  v.set("cpl", sub(v["l"], mul(v["poly_surround"], "2")));
  v.set("cvw", sub(v["cdw"], "0.01"));
  v.set("via_layers", "2");
  if (gt(v["viagate"], "0")) {
    if (ge(v["l"], "0.42")) {
      v.set("cpl", sub(v["cpl"], "0.08"));
    } else if (ge(v["w"], "0.58") && (gt(v["viasrc"], "0") || gt(v["viadrn"], "0"))) {
      v.set("via_layers", "1");
      v.set("cvw", sub(v["cdw"], "0.22"));
    }
  }
  v.set("cdwfull", v["cdw"]);
  v.set("cvwfull", v["cvw"]);
  v.set("cplfull", v["cpl"]);
  v.set("cdw", mul(v["cdw"], div(v["diffcov"], "100.0")));
  if (eq(v["poverlap"], "0") || eq(v["m"], "1")) {
    v.set("cpl", mul(v["cpl"], div(v["polycov"], "100.0")));
  }
}

// The dogbone rules 1-4 (ihp-sg13g2-fet.tcl:460-513), then the contact sizes.
void contact_geometry(Locals& v) {
  const Val eps = "0.0005";
  v.set("ddover", "0");
  const Val cdwmin = add(v["contact_size"], mul(v["diff_surround"], "2"));
  Val cstem = sub(v["gate_to_diffcont"], div(cdwmin, "2.0"));
  Val cgrow = sub(v["diff_poly_space"], cstem);
  const bool narrow = lt(add(v["w"], eps), cdwmin);
  if (narrow) {
    if (gt(cgrow, "0")) {
      v.set("gate_to_diffcont", add(v["gate_to_diffcont"], cgrow));
    }
    v.set("ddover", div(sub(cdwmin, v["w"]), "2.0"));
  }
  const Val gporig = v["gate_to_polycont"];
  const Val cplmin = add(v["contact_size"], mul(v["poly_surround"], "2"));
  cstem = sub(v["gate_to_polycont"], div(cplmin, "2.0"));
  cgrow = sub(v["diff_poly_space"], cstem);
  const bool short_gate = lt(add(v["l"], eps), cplmin);
  if (short_gate) {
    if (gt(cgrow, "0")) {
      v.set("gate_to_polycont", add(v["gate_to_polycont"], cgrow));
    }
  } else if (lt(add(v["w"], eps), "0.16") && gt(cgrow, "0")) {
    v.set("gate_to_polycont", add(v["gate_to_polycont"], cgrow, "0.01"));
  }
  if (narrow && short_gate) {
    cgrow = div(sub(cplmin, v["w"]), "2.0");
    v.set("gate_to_polycont", add(v["gate_to_polycont"], cgrow));
  }
  if (eq(v["poverlap"], "1") && gt(v["m"], "1") && lt(sub(v["gate_to_polycont"], gporig), v["ddover"])) {
    v.set("gate_to_polycont", add(gporig, v["ddover"]));
  }
  contact_sizes(v);
}

void diffusion_via(Device& d, const Val& viatype, std::string_view first, std::string_view second) {
  Engine& e = d.e;
  const Locals& v = d.v;
  const Val& via_size = v["via_size"];
  const Val& cvwfull = v["cvwfull"];
  e.pushbox();
  const Val cw = eq(v["ccols"], "1") ? via_size : sub(v["sdcontact_size"], "0.01");
  Val ch = mul(cvwfull, div(abs(viatype), "100.0"));
  if (lt(ch, via_size)) {
    ch = via_size;
  }
  e.grow(first, div(cw, "2.0"));
  e.grow(second, div(cw, "2.0"));
  const std::string anchor = first_char(viatype);
  if (anchor == "+") {
    e.move("s", div(sub(cvwfull, via_size), "2.0"));
    e.grow("n", ch);
  } else if (anchor == "-") {
    e.move("n", div(sub(cvwfull, via_size), "2.0"));
    e.grow("s", ch);
  } else {
    e.grow("n", div(ch, "2.0"));
    e.grow("s", div(ch, "2.0"));
  }
  via1_draw(e, "horz", v["via_layers"]);
  e.popbox();
}

void diffusion_contact(Device& d, List& cext, bool drain) {
  Engine& e = d.e;
  const Locals& v = d.v;
  const std::string side = (drain ? v["dside"] : v["sside"]).str();
  const std::string other = (drain ? v["sside"] : v["dside"]).str();
  e.pushbox();
  e.move(side, v["hl"]);
  e.move(side, v["gate_to_diffcont"]);
  const Val& viatype = drain ? v["viadrn"] : v["viasrc"];
  if (ne(viatype, "0")) {
    diffusion_via(d, viatype, side, other);
  }
  cext = unionbox(cext, draw_contact(e, v["sdcontact_size"], v["cdw"], v["diff_surround"], v["metal_surround"],
                                     v["contact_size"], v["diff_type"], v["diff_contact_type"], "m1", "horz"));
  const Val& port = drain ? v["drain"] : v["source"];
  if (ne(port, "")) {
    make_port(e, port, v["diff_contact_type"]);
  }
}

void gate_shield(Device& d) {
  Engine& e = d.e;
  const Locals& v = d.v;
  e.pushbox();
  e.move("w", v["hl"]);
  e.move("w", v["gate_to_diffcont"]);
  e.run({"box", "width", um(mul("2", add(v["hl"], v["gate_to_diffcont"])))});
  e.grow("n", div(v["cdwfull"], "2.0"));
  e.grow("s", div(v["cdwfull"], "2.0"));
  e.run({"paint", "m2"});
  const Val gsh = e.i2u(Val{e.run({"box", "height"})});
  if (gt(gsh, "25")) {
    e.move("n", div(gsh, "2.0"));
    e.run({"box", "move", "s", "1.15um"});
    e.run({"box", "height", "2.3um"});
    e.run({"box", "grow", "w", "-" + um(v["via_size"])});
    e.run({"box", "grow", "e", "-" + um(v["via_size"])});
    e.run({"erase", "m2"});
  }
  e.popbox();
}

void gate_via(Device& d) {
  Engine& e = d.e;
  const Locals& v = d.v;
  const Val& via_size = v["via_size"];
  const Val& cplfull = v["cplfull"];
  const Val reach = add(v["hl"], v["gate_to_diffcont"]);
  e.pushbox();
  const Val ch = via_size;
  Val cw = mul(cplfull, div(abs(v["viagate"]), "100.0"));
  if (lt(cw, via_size)) {
    cw = via_size;
  }
  e.grow("n", div(ch, "2.0"));
  e.grow("s", div(ch, "2.0"));
  const std::string anchor = first_char(v["viagate"]);
  if (anchor == "+") {
    e.move("w", div(sub(cplfull, via_size), "2.0"));
    e.grow("e", cw);
  } else if (anchor == "-") {
    e.move("e", div(sub(cplfull, via_size), "2.0"));
    e.grow("w", cw);
  } else {
    e.grow("e", div(cw, "2.0"));
    e.grow("w", div(cw, "2.0"));
  }
  if (eq(v["conn_gates"], "0")) {
    via1_draw(e, "horz", "2", "0.09");
  } else {
    via1_draw(e, "horz", "2");
  }
  e.popbox();
  e.pushbox();
  if (eq(v["conn_gates"], "1")) {
    e.grow("n", div(ch, "2.0"));
    e.grow("s", div(ch, "2.0"));
    e.grow("w", reach);
    e.grow("e", reach);
    e.run({"paint", "m2"});
  }
  e.popbox();
}

// The metal1 strap connecting the gates (ihp-sg13g2-fet.tcl:705-731).
void gate_strap(Device& d) {
  Engine& e = d.e;
  const Locals& v = d.v;
  const Val reach = add(v["hl"], v["gate_to_diffcont"]);
  e.pushbox();
  e.grow("n", div(v["contact_size"], "2.0"));
  e.grow("s", div(v["contact_size"], "2.0"));
  e.grow("w", reach);
  e.grow("e", reach);
  if (eq(v["poly_merge"], "1")) {
    e.pushbox();
    e.grow("n", v["poly_surround"]);
    e.grow("s", v["poly_surround"]);
    e.run({"paint", "poly"});
    e.popbox();
  }
  if (v["gate_ring"].truth()) {
    const Val ring = add(div(v["sdcontact_size"], "2.0"), v["diff_surround"], v["diff_spacing"]);
    e.grow("w", ring);
    e.grow("e", ring);
  }
  e.run({"paint", "m1"});
  if (ne(v["viagate"], "0")) {
    e.pushbox();
    e.run({"box", "grow", "n", "0.025um"});
    e.run({"box", "grow", "s", "0.025um"});
    e.run({"paint", "m1"});
    e.popbox();
  }
  e.popbox();
}

void poly_contact(Device& d, List& cext, std::string_view dir) {
  Engine& e = d.e;
  const Locals& v = d.v;
  e.pushbox();
  e.move(dir, v["hw"]);
  e.move(dir, v["gate_to_polycont"]);
  if (ne(v["viagate"], "0")) {
    gate_via(d);
  }
  cext = unionbox(cext, draw_contact(e, v["cpl"], "0", v["poly_surround"], v["metal_surround"], v["contact_size"],
                                     v["poly_type"], v["poly_contact_type"], "m1", "horz"));
  if (eq(v["conn_gates"], "1")) {
    gate_strap(d);
  }
  const bool label = dir == "n" ? ne(v["gate"], "") : (ne(v["gate"], "") && eq(v["topc"], "0"));
  if (label) {
    make_port(e, v["gate"], v["poly_contact_type"]);
  }
  e.popbox();
}

void gate_and_substrate(Device& d, List& cext, const std::vector<std::string>& gaterect, const List& diffarea) {
  Engine& e = d.e;
  const Locals& v = d.v;
  e.pushbox();
  std::vector<std::string> words{"box", "values"};
  words.insert(words.end(), gaterect.begin(), gaterect.end());
  e.run(words);
  if (v.has("gate_type")) {
    try {
      e.paint(v["gate_type"]);
    } catch (const TclError&) {
    }
  }
  if (d.parameters.has("sub_surround_dev")) {
    for (const char* dir : {"n", "s", "e", "w"}) {
      e.grow(dir, v["sub_surround_dev"]);
    }
    e.paint(v["dev_sub_type"]);
    cext = unionbox(cext, getbox(e));
  }
  e.popbox();
  if (ne(v["dev_sub_type"], "")) {
    e.run({"box", "values", um(diffarea.at(0)), um(diffarea.at(1)), um(diffarea.at(2)), um(diffarea.at(3))});
    for (const char* dir : {"n", "s", "e", "w"}) {
      e.grow(dir, v["sub_surround"]);
    }
    e.paint(v["dev_sub_type"]);
    if (gt(v["dev_sub_dist"], "0")) {
      cext = unionbox(cext, getbox(e));
    }
  }
}

List mos_device(Engine& e, const Dict& parameters) {
  Device d{e, parameters, {}};
  Locals& v = d.v;
  mos_device_defaults(v);
  v.load(parameters);
  contact_flags(v, parameters);

  e.pushbox();
  e.run({"box", "size", "0", "0"});
  std::vector<std::string> gaterect;
  List polybox;
  List cext = draw_diff_and_poly(d, gaterect, polybox);
  contact_geometry(v);
  if (ne(v["drain_proc"], "")) {
    throw TclError{"drain procedure " + v["drain_proc"].str()};  // refused by require_drawable
  }
  diffusion_contact(d, cext, true);
  e.popbox();
  diffusion_contact(d, cext, false);
  const List diffarea = cext;
  e.popbox();
  if (eq(v["gshield"], "1")) {
    gate_shield(d);
  }
  if (v["topc"].truth() && v["oldtopc"].truth()) {
    poly_contact(d, cext, "n");
  }
  if (v["botc"].truth() && v["oldbotc"].truth()) {
    poly_contact(d, cext, "s");
  }
  gate_and_substrate(d, cext, gaterect, diffarea);
  e.popbox();
  return unionbox(cext, polybox);
}

// ---- ihp-sg13g2-fet.tcl: mos_draw ---------------------------------------------

struct Tiling {
  Val dx;
  Val dy;
  Val corellx;
  Val corelly;
  Val gx;
  Val gy;
  Val xoffset;
  Val extra_gx = "0";
};

void mos_draw_defaults(Locals& v) {
  for (const auto& [name, value] : std::vector<std::pair<const char*, const char*>>{
           {"is_rf", "0"},        {"topc", "0"},         {"botc", "0"},        {"guard", "1"},
           {"poverlap", "0"},     {"doverlap", "1"},     {"dev_sub_dist", "0"}, {"dev_sub_space", "0"},
           {"id_type", ""},       {"id_surround", "0"},  {"id2_type", ""},     {"id2_surround", "0"},
           {"conn_gates", "1"},   {"gate_ring", "1"},    {"poly_merge", "0"},  {"doports", "0"},
           {"set_x_to_guard", ""}, {"set_y_to_guard", ""}}) {
    v.set(name, Val{value});
  }
}

// Parameter preparation of mos_draw (ihp-sg13g2-fet.tcl:905-951).
void prepare(Engine& e, Locals& v, Dict& parameters) {
  if (!parameters.has("drain_diff_surround")) {
    v.set("drain_diff_surround", v["diff_surround"]);
  }
  if (!parameters.has("diff_tap_space")) {
    v.set("diff_tap_space", v["diff_spacing"]);
  }
  if (eq(v["poverlap"], "1")) {
    v.set("topc", "1");
    v.set("botc", "1");
    parameters.set("topc", "1");
    parameters.set("botc", "1");
  }
  if (eq(v["ng"], "1") && eq(v["is_rf"], "0")) {
    v.set("conn_gates", "0");
  }
  v.set("sdcontact_size", add(v["contact_size"], mul(sub(v["ccols"], "1"), "0.34")));
  parameters.set("sdcontact_size", v["sdcontact_size"]);
  const Val hgddiff = div(sub(v["sdcontact_size"], v["contact_size"]), "2.0");
  v.set("gate_to_diffcont", add(v["gate_to_diffcont"], hgddiff));
  parameters.set("gate_to_diffcont", v["gate_to_diffcont"]);
  v.set("w", e.spice2float(v["w"]));
  v.set("l", e.spice2float(v["l"]));
}

void tile_geometry(Locals& v, const List& bbox, Tiling& t) {
  const Val fw = sub(bbox.at(2), bbox.at(0));
  const Val fh = sub(bbox.at(3), bbox.at(1));
  const Val lw = add(bbox.at(2), bbox.at(0));
  const Val lh = add(bbox.at(3), bbox.at(1));
  t.xoffset = add(bbox.at(0), bbox.at(2));
  if (gt(v["dev_sub_dist"], "0")) {
    v.set("poverlap", "0");
    v.set("doverlap", "0");
    const Val& space = gt(v["dev_sub_space"], v["poly_spacing"]) ? v["dev_sub_space"] : v["poly_spacing"];
    t.dx = add(fw, space);
    t.dy = add(fh, space);
  } else {
    if (eq(v["poverlap"], "0")) {
      t.dy = add(fh, v["poly_spacing"]);
      if (gt(v["viagate"], "0")) {
        t.dy = add(t.dy, "0.02");
      }
    } else {
      t.dy = sub(fh, add(v["poly_surround"], v["poly_surround"], v["contact_size"]));
    }
    t.dx = eq(v["doverlap"], "0")
               ? add(fw, v["diff_spacing"])
               : sub(fw, add(v["drain_diff_surround"], v["drain_diff_surround"], v["sdcontact_size"]));
  }
  v.set("corex", add(mul(sub(v["ng"], "1"), t.dx), fw));
  v.set("corey", add(mul(sub(v["m"], "1"), t.dy), fh));
  t.corellx = div(add(sub(v["corex"], fw), lw), "2.0");
  t.corelly = div(add(sub(v["corey"], fh), lh), "2.0");
  if (eq(v["topc"], "0")) {
    const Val cdwmin = add(v["contact_size"], mul(v["diff_surround"], "2"));
    if (lt(v["w"], cdwmin)) {
      v.set("corey", add(v["corey"], div(sub(cdwmin, v["w"]), "2.0")));
    }
  }
}

void gate_ring_geometry(Engine& e, const Locals& v, const Dict& parameters, Tiling& t) {
  if (eq(v["gate_ring"], "0")) {
    t.extra_gx = "0";
    return;
  }
  // Account for widespacing run-length rule
  const bool wide = gt(v["w"], "1") && gt(v["ccols"], "1");
  const Val gms = wide ? add("0.02", v["metal_spacing"]) : v["metal_spacing"];
  t.extra_gx = add(v["contact_size"], mul("2.0", gms));
  t.gx = add(v["corex"], t.extra_gx);
  t.gy = sub(v["corey"], add(v["contact_size"], mul("2.0", v["poly_surround"])));
  metal_ring(e, t.gx, t.gy, parameters);
}

Val guard_extent(const Locals& v, const char* core, const char* space) {
  const bool sub_dist = gt(v["dev_sub_dist"], "0") && gt(add(v["dev_sub_dist"], v["sub_surround"]), v[space]);
  const Val inner = sub_dist ? add(v["dev_sub_dist"], v["diff_surround"]) : add(v[space], v["diff_surround"]);
  return add(v[core], mul("2.0", inner), v["contact_size"]);
}

void guard_geometry(const Locals& v, Tiling& t) {
  t.gx = guard_extent(v, "corex", "diff_tap_space");
  t.gy = guard_extent(v, "corey", "diff_gate_space");
  const Val inset = div(add(v["contact_size"], mul("2.0", v["diff_surround"]), Val{"-" + v["w"].str()}), "2.0");
  const Val sdiff = sub(add(inset, v["diff_tap_space"]), add(v["gate_extension"], v["diff_gate_space"]));
  if (gt(sdiff, "0")) {
    if (eq(v["topc"], "0")) {
      t.gy = add(t.gy, sdiff);
      t.corelly = add(t.corelly, div(sdiff, "2.0"));
    }
    if (eq(v["botc"], "0")) {
      t.gy = add(t.gy, sdiff);
      t.corelly = sub(t.corelly, div(sdiff, "2.0"));
    }
  }
  if (ne(v["set_x_to_guard"], "")) {
    t.gx = add(v["corex"], mul("2.0", v["set_x_to_guard"]));
  }
  if (ne(v["set_y_to_guard"], "")) {
    t.gy = add(v["corey"], mul("2.0", v["set_y_to_guard"]));
  }
  t.gx = add(t.gx, t.extra_gx);
}

void draw_guard(Engine& e, const Locals& v, Dict& parameters, const Tiling& t, const std::string& lvsname) {
  if (v["doports"].truth()) {
    parameters.set("bulk", "B");
  }
  static_cast<void>(guard_ring(e, t.gx, t.gy, parameters));
  if (ne(v["is_rf"], "0") && lvsname != "none") {
    e.pushbox();
    e.run({"box", "size", "0", "0"});
    const Val hgy = div(t.gy, "2.0");
    e.run({"box", "position", "0", um(hgy)});
    e.run({"label", lvsname, "c", "-comment"});
    e.popbox();
  }
}

void port_labels(const Locals& v, Dict& parameters, const Val& xp, const Val& yp, const Val& evens) {
  const std::string x = xp.str();
  const std::string y = yp.str();
  const bool overlap_drain = v["doverlap"].truth() && evens.truth() && gt(xp, "0");
  const bool overlap_source = v["doverlap"].truth() && eq(evens, "0") && lt(xp, sub(v["ng"], "1"));
  const bool single_gate = eq(v["ng"], "1");
  const bool conn = eq(v["conn_gates"], "0");
  if (eq(v["m"], "1")) {
    if (single_gate) {
      parameters.set("drain", "D");
      parameters.set("source", "S");
      parameters.set("gate", "G");
      return;
    }
    parameters.set("drain", overlap_drain ? std::string{} : "D" + x);
    parameters.set("source", overlap_source ? std::string{} : "S" + x);
    parameters.set("gate", conn ? "G" + x : std::string{"G"});
    return;
  }
  const bool first_row = eq(yp, "0");
  const bool shared = v["poverlap"].truth() || v["gate_ring"].truth();
  if (single_gate) {
    parameters.set("drain", "D" + y);
    parameters.set("source", "S" + y);
    parameters.set("gate", shared ? (first_row ? std::string{"G"} : std::string{}) : "G" + y);
    return;
  }
  parameters.set("drain", overlap_drain ? std::string{} : "D" + x + "_" + y);
  parameters.set("source", overlap_source ? std::string{} : "S" + x + "_" + y);
  if (shared && first_row) {
    parameters.set("gate", conn ? "G" + x : std::string{"G"});
  } else if (shared) {
    parameters.set("gate", "");
  } else {
    parameters.set("gate", conn ? "G" + x + "_" + y : "G" + y);
  }
}

void tile_devices(Engine& e, const Locals& v, Dict& parameters, const Tiling& t) {
  Val evens = "1";
  for (Val xp = Val::integer(0); lt(xp, v["ng"]); xp = add(xp, "1")) {
    parameters.set("evens", evens);
    evens = sub("1", evens);
    e.pushbox();
    for (Val yp = Val::integer(0); lt(yp, v["m"]); yp = add(yp, "1")) {
      if (v["doports"].truth()) {
        port_labels(v, parameters, xp, yp, evens);
      }
      if (ne(evens, "0")) {
        e.move("e", t.xoffset);
      }
      static_cast<void>(mos_device(e, parameters));
      if (ne(evens, "0")) {
        e.move("w", t.xoffset);
      }
      e.move("n", t.dy);
    }
    e.popbox();
    e.move("e", t.dx);
  }
}

std::string mos_draw(Engine& e, const std::string& lvsname, Dict parameters) {
  e.run({"tech", "unlock", "*"});
  const std::string curunits = e.run({"units"});
  e.run({"units", "internal"});
  Locals v;
  mos_draw_defaults(v);
  v.load(parameters);
  prepare(e, v, parameters);
  e.pushbox();
  e.run({"box", "values", "0", "0", "0", "0"});
  v.set("poly_merge", v["is_rf"]);
  parameters.set("poly_merge", v["poly_merge"]);
  e.run({"tech", "lock", "*"});
  const List bbox = mos_device(e, parameters);
  e.run({"tech", "unlock", "*"});

  Tiling t;
  tile_geometry(v, bbox, t);
  gate_ring_geometry(e, v, parameters, t);
  if (ne(v["guard"], "0") || ne(v["id_type"], "")) {
    guard_geometry(v, t);
  }
  if (ne(v["guard"], "0")) {
    draw_guard(e, v, parameters, t, lvsname);
  }
  e.pushbox();
  if (ne(v["id_type"], "")) {
    const Val hw = div(t.gx, "2.0");
    const Val hh = div(t.gy, "2.0");
    e.grow("e", hw);
    e.grow("w", hw);
    e.grow("n", hh);
    e.grow("s", hh);
    e.grow("c", v["id_surround"]);
    e.paint(v["id_type"]);
  }
  e.popbox();
  e.pushbox();
  e.move("w", t.corellx);
  e.move("s", t.corelly);
  tile_devices(e, v, parameters, t);
  e.popbox();
  e.popbox();
  if (eq(v["is_rf"], "1")) {
    e.run({"box", "size", "0", "0"});
    e.run({"label", "rfmode=1^", "c", v["gate_type"].str()});
  }
  auto units = split_tcl_list(curunits);
  std::vector<std::string> words{"units"};
  if (units.ok()) {
    words.insert(words.end(), units.value().begin(), units.value().end());
  }
  e.run(words);
  return e.run({"tech", "revert"});
}

// ---- ihp-sg13g2-fet.tcl: mos_check ---------------------------------------------

Val incr(const Val& v, std::int64_t by) {
  const tcl::Number n = v.num();
  if (!n.integer) {
    throw TclError{"expected integer but got \"" + v.str() + "\""};
  }
  return Val::integer(n.i + by);
}

void require_abs(const Val& value) {
  if (!value.is_number()) {
    throw Unsupported{"expr abs(" + value.str() + ")"};
  }
}

void check_percent(Engine& e, ParameterDict& parameters, const Val& value, const char* key, const char* what) {
  if (lt(value, "20")) {
    e.puts(true, std::string{what} + " contact coverage must be at least 20%");
    parameters.set(key, "20");
  } else if (gt(value, "100")) {
    e.puts(true, std::string{what} + " contact coverage can't be more than 100%");
    parameters.set(key, "100");
  }
}

void check_counts(Engine& e, ParameterDict& parameters, const Locals& v) {
  if (!string_is_int(v["ng"])) {
    e.puts(true, "NG must be an integer!");
    parameters.set("ng", "1");
  }
  if (!string_is_int(v["m"])) {
    e.puts(true, "M must be an integer!");
    parameters.set("m", "1");
  }
  require_abs(v["diffcov"]);
  require_abs(v["polycov"]);
  if (lt(v["l"], v["lmin"])) {
    e.puts(true, "Mos length must be >= " + v["lmin"].str() + " um");
    parameters.set("l", v["lmin"].str());
  }
  if (lt(v["w"], v["wmin"])) {
    e.puts(true, "Mos width must be >= " + v["wmin"].str() + " um");
    parameters.set("w", v["wmin"].str());
  }
  if (lt(v["ng"], "1")) {
    e.puts(true, "NG must be >= 1");
    parameters.set("ng", "1");
  }
  if (lt(v["m"], "1")) {
    e.puts(true, "M must be >= 1");
    parameters.set("m", "1");
  }
  check_percent(e, parameters, v["diffcov"], "diffcov", "Diffusion");
  check_percent(e, parameters, v["polycov"], "polycov", "Poly");
}

void check_coverages(Engine& e, ParameterDict& parameters, const Locals& v) {
  static const std::pair<const char*, const char*> kCoverages[] = {
      {"viasrc", "Source via"},           {"viadrn", "Drain via"},          {"viagate", "Gate via"},
      {"viagb", "Guard ring bottom via"}, {"viagt", "Guard ring top via"},  {"viagr", "Guard ring right via"},
      {"viagl", "Guard ring left via"}};
  for (const auto& [key, name] : kCoverages) {
    require_abs(v[key]);
    if (gt(abs(v[key]), "100")) {
      e.puts(true, std::string{name} + " coverage can't be more than 100%");
      parameters.set(key, "100");
    }
  }
}

void check_extent(Engine& e, ParameterDict& parameters, Locals& v) {
  const Val origm = v["m"];
  const Val origng = v["ng"];
  std::set<std::string> seen;
  while (true) {
    const std::string state = v["w"].str() + " " + v["l"].str() + " " + v["m"].str() + " " + v["ng"].str();
    if (!seen.insert(state).second) {
      throw Unsupported{"mos_check does not terminate"};
    }
    const Val yext = expr_text("(" + v["w"].str() + " + 1.0) * " + v["m"].str() + " + 1.0");
    const Val xext = expr_text("(" + v["l"].str() + " + 1.0) * " + v["ng"].str() + " + 1.0");
    if (!gt(min(xext, yext), "30.0")) {
      break;
    }
    if (gt(yext, "30.0") && gt(v["m"], "1")) {
      v.set("m", incr(v["m"], -1));
    } else if (gt(xext, "30.0") && gt(v["ng"], "1")) {
      v.set("ng", incr(v["ng"], -1));
    } else if (gt(yext, "30.0")) {
      v.set("w", "29");
      e.puts_nonewline(true, "Transistor width must be < 29 um");
      e.puts(true, " to avoid tap spacing violation.");
      parameters.set("w", "29");
    } else if (gt(xext, "30.0")) {
      v.set("l", "29");
      e.puts_nonewline(true, "Transistor length must be < 29 um");
      e.puts(true, " to avoid tap spacing violation.");
      parameters.set("l", "29");
    }
  }
  if (ne(v["m"], origm)) {
    e.puts(true, "Warning: M may need to be reduced to prevent tap distance violation");
  }
  if (ne(v["ng"], origng)) {
    e.puts(true, "Warning: Fingers may need to be reduced to prevent tap distance violation");
  }
}

ParameterDict mos_check(Engine& e, ParameterDict parameters) {
  Locals v;
  v.load(Dict::from(parameters));
  v.set("l", e.three_digit_past_decimal(e.spice2float(v["l"])));
  v.set("w", e.three_digit_past_decimal(e.spice2float(v["w"])));
  check_counts(e, parameters, v);
  check_coverages(e, parameters, v);
  check_extent(e, parameters, v);
  e.set_variable("magic::minfo_val", "");
  return parameters;
}

// ---- the generators ---------------------------------------------------------------

const Dict& ruleset() {
  // ihp-sg13g2.tcl:134-155
  static const Dict kRules = {
      {"poly_surround", "0.07"},   {"diff_surround", "0.07"},  {"gate_to_diffcont", "0.19"},
      {"gate_to_polycont", "0.22"}, {"gate_extension", "0.18"}, {"diff_extension", "0.18"},
      {"contact_size", "0.16"},    {"via_size", "0.20"},       {"metal_surround", "0.05"},
      {"sub_surround", "0.31"},    {"diff_spacing", "0.21"},   {"poly_spacing", "0.18"},
      {"diff_poly_space", "0.07"}, {"diff_gate_space", "0.07"}, {"metal_spacing", "0.18"},
      {"mmetal_spacing", "0.21"},  {"res_to_cont", "0.20"},    {"res_diff_spacing", "0.18"}};
  return kRules;
}

const std::set<std::string, std::less<>>& text_keys() {
  static const std::set<std::string, std::less<>> kKeys = {
      "class",           "compatible",        "gate_type",      "diff_type",      "diff_contact_type",
      "plus_diff_type",  "plus_contact_type", "poly_type",      "poly_contact_type", "sub_type",
      "dev_sub_type",    "id_type",           "id2_type",       "guard_sub_type", "drain",
      "source",          "gate",              "bulk",           "set_x_to_guard", "set_y_to_guard",
      "gencell"};
  return kKeys;
}

void require_drawable(const Dict& drawdict) {
  require_numbers(drawdict, text_keys());
  for (const char* key : {"set_x_to_guard", "set_y_to_guard"}) {
    if (drawdict.has(key) && !drawdict.get(key).empty() && !drawdict.get(key).is_number()) {
      throw Unsupported{std::string{key} + " is not a number"};
    }
  }
  if (drawdict.has("drain_proc") && !drawdict.get("drain_proc").empty()) {
    throw Unsupported{"drain procedure"};
  }
  for (const char* key : {"ng", "m"}) {
    if (drawdict.has(key) && gt(drawdict.get(key), "1000")) {
      throw Unsupported{"array too large"};
    }
  }
}

void require_checkable(const ParameterDict& parameters) {
  Locals v;
  v.load(Dict::from(parameters));
  for (const char* key : {"l", "w", "lmin", "wmin", "ng", "m"}) {
    if (!v[key].is_number()) {
      throw Unsupported{std::string{key} + " is not a number"};
    }
  }
}

struct MosRow {
  const char* type;
  const char* check_device;  // unused by sg13g2::mos_check's body
  const char* lvsname;       // mos_draw's first argument
  const char* defaults;
  std::vector<std::pair<std::string, Val>> newdict;
};

class Sg13g2Mos final : public NativeGenerator {
 public:
  explicit Sg13g2Mos(const MosRow& row) : row_(row) {
    for (const auto& [k, value] : row.newdict) {
      newdict_.set(k, value);
    }
  }

  [[nodiscard]] ParameterDict defaults() const override {
    auto parsed = ParameterDict::from_tcl(row_.defaults);
    return parsed.ok() ? std::move(parsed).value() : ParameterDict{};
  }

  [[nodiscard]] std::optional<diag::Outcome<ParameterDict>> check(NativeHost& host,
                                                                 const ParameterDict& parameters) const override {
    BufferedHost buffered{host};
    auto result = run_check([&] {
      require_checkable(parameters);
      Engine e{buffered, HelperFamily::sg13g2};
      return mos_check(e, parameters);
    });
    if (result) {
      buffered.flush();
    }
    return result;
  }

  [[nodiscard]] std::optional<cmd::CommandResult> draw(NativeHost& host,
                                                       const ParameterDict& parameters) const override {
    return run_port([&] {
      const Dict drawdict = ruleset().merged(newdict_).merged(Dict::from(parameters));
      require_drawable(drawdict);
      Engine e{host, HelperFamily::sg13g2};
      return cmd::CommandResult::success(mos_draw(e, row_.lvsname, drawdict));
    });
  }

 private:
  const MosRow& row_;
  Dict newdict_;
};

const std::vector<MosRow>& rows() {
  // clang-format off
  static const std::vector<MosRow> kRows = {
#include "pcell/native/sg13g2/mos_table.inc"
  };
  // clang-format on
  return kRows;
}

}  // namespace

void add_sg13g2_mosfets(NativeRegistry& registry, const script::PdkIdentity& pdk) {
  for (const auto& row : rows()) {
    registry.add(pdk, std::string{"sg13g2::"} + row.type, std::make_unique<Sg13g2Mos>(row));
  }
}

}  // namespace hefesto::pcell::native
