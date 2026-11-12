// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Native gf180mcu MOSFET, MOS capacitor and varactor generators (design
// D10; D-29): a port of the procedures every `gf180mcu::<device>_draw` and
// `_check` of gf180mcuD-fet.tcl delegates to, issuing the same engine
// commands with the same words. PDK ref (open_pdks c6df28c1, ciel release
// 5740c4a8):
//   gf180mcuD.tcl:51-75           gf180mcu::ruleset
//   gf180mcuD-util.tcl:67-113     draw_contact
//   gf180mcuD-util.tcl:115-400    guard_ring
//   gf180mcuD-via.tcl:14-47       via1_draw
//   gf180mcuD-fet.tcl:29-240      the _defaults dictionaries
//   gf180mcuD-fet.tcl:534-898     mos_device
//   gf180mcuD-fet.tcl:904-1096    mos_draw
//   gf180mcuD-fet.tcl:1102-1563   the per-device draw dictionaries
//   gf180mcuD-fet.tcl:1568-1680   mos_check
// Variable names follow the Tcl so the two can be read side by side.

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/pcell/framework/tcl_list.hpp"
#include "pcell/native/builtin.hpp"
#include "pcell/native/port.hpp"

namespace hefesto::pcell::native {

namespace {

// ---- gf180mcuD-util.tcl / gf180mcuD-via.tcl --------------------------------

List draw_contact(Engine& e, const Val& w0, const Val& h0, const Val& s, const Val& o, const Val& x,
                  const Val& atype, const Val& ctype, const Val& mtype, std::string_view orient = "vert") {
  e.pushbox();
  e.run({"box", "size", "0", "0"});
  const Val w = lt(w0, x) ? x : w0;
  const Val h = lt(h0, x) ? x : h0;
  const Val hw = div(w, "2.0");
  const Val hh = div(h, "2.0");
  // Bottom layer surrounded on all sides
  e.grow("e", hw);
  e.grow("w", hw);
  e.grow("n", hh);
  e.grow("s", hh);
  e.pushbox();
  e.paint(ctype);
  e.popbox();
  e.pushbox();
  e.grow("c", s);
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

void via1_draw(Engine& e, std::string_view orient = "default") {
  const Val w = e.i2u(Val{e.run({"box", "width"})});
  const Val h = e.i2u(Val{e.run({"box", "height"})});
  if (lt(w, "0.26")) {
    e.puts(true, "Via1 width must be at least 0.26um");
    return;
  }
  if (lt(h, "0.26")) {
    e.puts(true, "Via1 height must be at least 0.26um");
    return;
  }
  e.run({"paint", "via1"});
  e.pushbox();
  e.pushbox();
  e.run({"box", "grow", "c", "0.01um"});
  const bool m2_horizontal = orient == "horz" || orient == "rev";
  e.run({"box", "grow", m2_horizontal ? "e" : "n", "0.05um"});
  e.run({"box", "grow", m2_horizontal ? "w" : "s", "0.05um"});
  e.run({"paint", "m2"});
  e.popbox();
  const bool m1_vertical = orient == "vert" || orient == "rev";
  e.run({"box", "grow", m1_vertical ? "n" : "e", "0.06um"});
  e.run({"box", "grow", m1_vertical ? "s" : "w", "0.06um"});
  e.run({"paint", "m1"});
  e.popbox();
}

Locals guard_ring_locals(const Val& gw, const Val& gh, const Dict& parameters) {
  Locals v;
  for (const auto& [name, value] : std::vector<std::pair<const char*, const char*>>{
           {"rlcov", "100"}, {"tbcov", "100"}, {"grc", "1"},   {"glc", "1"},   {"gtc", "0"},
           {"gbc", "0"},     {"full_metal", "0"}, {"viagb", "0"}, {"viagt", "0"}, {"viagr", "0"},
           {"viagl", "0"},   {"bulk", ""}}) {
    v.set(name, Val{value});
  }
  v.set("gw", gw);
  v.set("gh", gh);
  v.load(parameters);
  v.set("hx", div(v["contact_size"], "2.0"));
  v.set("hw", div(v["gw"], "2.0"));
  v.set("hh", div(v["gh"], "2.0"));
  v.set("difft", add(v["contact_size"], v["diff_surround"], v["diff_surround"]));
  v.set("hdifft", div(v["difft"], "2.0"));
  v.set("hdiffw", div(add(v["gw"], v["difft"]), "2.0"));
  v.set("hdiffh", div(add(v["gh"], v["difft"]), "2.0"));
  return v;
}

// One side of the ring: move to the side, grow across by `across` and
// along by `along`, paint `type`.
void ring_side(Engine& e, std::string_view dir, const Val& offset, const Val& across, const Val& along,
               const Val& type) {
  const bool horizontal = dir == "n" || dir == "s";
  e.pushbox();
  e.move(dir, offset);
  if (horizontal) {
    e.grow("n", across);
    e.grow("s", across);
    e.grow("e", along);
    e.grow("w", along);
  } else {
    e.grow("e", across);
    e.grow("w", across);
    e.grow("n", along);
    e.grow("s", along);
  }
  e.paint(type);
  e.popbox();
}

// The guard ring vias of one side (gf180mcuD-util.tcl:280-371).
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
  e.grow(horizontal ? "n" : "e", div(across, "2"));
  e.grow(horizontal ? "s" : "w", div(across, "2"));
  const std::string anchor = first_char(coverage);
  const std::string_view low = horizontal ? "w" : "s";
  const std::string_view high = horizontal ? "e" : "n";
  if (anchor == "+") {
    e.move(low, div(sub(full, via_size), "2"));
    e.grow(high, along);
  } else if (anchor == "-") {
    e.move(high, div(sub(full, via_size), "2"));
    e.grow(low, along);
  } else {
    e.grow(high, div(along, "2"));
    e.grow(low, div(along, "2"));
  }
  via1_draw(e);
  e.popbox();
}

void ring_contacts(Engine& e, const Locals& v) {
  const Val inset = mul(add(v["metal_surround"], v["metal_spacing"]), "2.0");
  Val ch = mul(sub(v["gh"], v["contact_size"], inset), div(v["rlcov"], "100.0"));
  if (lt(ch, v["contact_size"])) {
    ch = v["contact_size"];
  }
  Val cw = mul(sub(v["gw"], v["contact_size"], inset), div(v["tbcov"], "100.0"));
  if (lt(cw, v["contact_size"])) {
    cw = v["contact_size"];
  }
  const auto contact = [&](const char* dir, const Val& half, const Val& w, const Val& h, const char* orient) {
    e.pushbox();
    e.move(dir, half);
    static_cast<void>(draw_contact(e, w, h, v["diff_surround"], v["metal_surround"], v["contact_size"],
                                   v["plus_diff_type"], v["plus_contact_type"], "m1", orient));
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
  const Val& plus = v["plus_diff_type"];
  e.pushbox();
  e.run({"box", "size", "0", "0"});
  ring_side(e, "n", v["hh"], v["hdifft"], v["hdiffw"], plus);
  ring_side(e, "s", v["hh"], v["hdifft"], v["hdiffw"], plus);
  // At guard ring bottom center, place a port if requested
  if (ne(v["bulk"], "")) {
    e.run({"label", v["bulk"].str(), "c", plus.str()});
    e.run({"select", "area", "label"});
    e.run({"port", "make"});
  }
  ring_side(e, "e", v["hw"], v["hdifft"], v["hdiffh"], plus);
  ring_side(e, "w", v["hw"], v["hdifft"], v["hdiffh"], plus);
  if (v["full_metal"].truth()) {
    const Val hmetw = div(add(v["gw"], v["contact_size"]), "2.0");
    const Val hmeth = div(add(v["gh"], v["contact_size"]), "2.0");
    ring_side(e, "n", v["hh"], v["hx"], hmetw, "m1");
    ring_side(e, "s", v["hh"], v["hx"], hmetw, "m1");
    ring_side(e, "e", v["hw"], v["hx"], hmeth, "m1");
    ring_side(e, "w", v["hw"], v["hx"], hmeth, "m1");
  }
  ring_contacts(e, v);
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

// ---- gf180mcuD-fet.tcl: mos_device -------------------------------------------

struct Device {
  Engine& e;
  Locals v;
};

void make_port(Engine& e, const Val& name, const Val& type) {
  e.run({"label", name.str(), "c", type.str()});
  e.run({"select", "area", "label"});
  e.run({"port", "make"});
}

// One side of the diffusion: the drain (`drain_diff_*`) or the source.
void diffusion_side(Device& d, std::string_view side, const Val& extension, const Val& type) {
  Engine& e = d.e;
  const Locals& v = d.v;
  e.pushbox();
  if (gt(extension, v["gate_to_diffcont"])) {
    e.grow(side, extension);
  } else {
    e.grow(side, v["gate_to_diffcont"]);
    // Grow to far side of contact (a dogbone contact moved outward).
    e.grow(side, div(v["contact_size"], "2.0"));
  }
  e.paint(type);
  e.popbox();
}

List draw_diff_and_poly(Device& d, std::vector<std::string>& gaterect) {
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
  diffusion_side(d, v["dside"].str(), v["drain_diff_extension"], v["drain_diff_type"]);
  diffusion_side(d, v["sside"].str(), v["diff_extension"], v["diff_type"]);
  e.pushbox();
  if (gt(v["gate_extension"], v["gate_to_polycont"])) {
    e.grow("n", v["gate_extension"]);
    e.grow("s", v["gate_extension"]);
  } else {
    e.grow("n", v["topc"].truth() ? v["gate_to_polycont"] : v["gate_extension"]);
    e.grow("s", v["botc"].truth() ? v["gate_to_polycont"] : v["gate_extension"]);
  }
  e.paint(v["poly_type"]);
  List cext = getbox(e);
  e.popbox();
  auto split = split_tcl_list(e.run({"box", "values"}));
  gaterect = split.ok() ? std::move(split).value() : std::vector<std::string>{};
  e.popbox();
  return cext;
}

// The dogbone rules 1-4 and the contact sizes (gf180mcuD-fet.tcl:636-708).
void contact_geometry(Locals& v) {
  const Val eps = "0.0005";
  v.set("diffcont_orient", "vert");
  v.set("ddover", "0");
  const Val cdwmin = add(v["contact_size"], mul(v["diff_surround"], "2"));
  Val cstem = sub(v["gate_to_diffcont"], div(cdwmin, "2.0"));
  Val cgrow = sub(v["diff_poly_space"], cstem);
  const bool narrow = lt(add(v["w"], eps), cdwmin);
  if (narrow) {
    if (gt(cgrow, "0")) {
      v.set("gate_to_diffcont", add(v["gate_to_diffcont"], cgrow));
      v.set("diffcont_orient", "horz");
    }
    v.set("ddover", div(sub(cdwmin, v["w"]), "2.0"));
  }
  const Val gporig = v["gate_to_polycont"];
  const Val cplmin = add(v["contact_size"], mul(v["poly_surround"], "2"));
  cstem = sub(v["gate_to_polycont"], div(cplmin, "2.0"));
  cgrow = sub(v["diff_poly_space"], cstem);
  const bool short_gate = lt(add(v["l"], eps), cplmin);
  if (short_gate && gt(cgrow, "0")) {
    v.set("gate_to_polycont", add(v["gate_to_polycont"], cgrow));
  }
  if (narrow && short_gate) {
    cgrow = div(sub(cplmin, v["w"]), "2.0");
    v.set("gate_to_polycont", add(v["gate_to_polycont"], cgrow));
  }
  if (eq(v["poverlap"], "1") && gt(v["m"], "1") && lt(sub(v["gate_to_polycont"], gporig), v["ddover"])) {
    v.set("gate_to_polycont", add(gporig, v["ddover"]));
  }
  v.set("cdw", sub(v["w"], mul(v["diff_surround"], "2")));
  v.set("cpl", sub(v["l"], mul(v["poly_surround"], "2")));
  v.set("cdwfull", v["cdw"]);
  v.set("cplfull", v["cpl"]);
  v.set("cdw", mul(v["cdw"], div(v["diffcov"], "100.0")));
  if (eq(v["poverlap"], "0") || eq(v["m"], "1")) {
    v.set("cpl", mul(v["cpl"], div(v["polycov"], "100.0")));
  }
}

// A via over a diffusion contact (gf180mcuD-fet.tcl:719-739).
void diffusion_via(Device& d, const Val& viatype, std::string_view first, std::string_view second) {
  Engine& e = d.e;
  const Val& via_size = d.v["via_size"];
  const Val& cdwfull = d.v["cdwfull"];
  e.pushbox();
  const Val cw = via_size;
  Val ch = mul(cdwfull, div(abs(viatype), "100.0"));
  if (lt(ch, via_size)) {
    ch = via_size;
  }
  e.grow(first, div(cw, "2"));
  e.grow(second, div(cw, "2"));
  const std::string anchor = first_char(viatype);
  if (anchor == "+") {
    e.move("s", div(sub(cdwfull, via_size), "2"));
    e.grow("n", ch);
  } else if (anchor == "-") {
    e.move("n", div(sub(cdwfull, via_size), "2"));
    e.grow("s", ch);
  } else {
    e.grow("n", div(ch, "2"));
    e.grow("s", div(ch, "2"));
  }
  via1_draw(e, "vert");
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
  cext = unionbox(cext, draw_contact(e, "0", v["cdw"], v["diff_surround"], v["metal_surround"], v["contact_size"],
                                     drain ? v["drain_diff_type"] : v["diff_type"],
                                     drain ? v["drain_diff_contact_type"] : v["diff_contact_type"], "m1",
                                     v["diffcont_orient"].str()));
  const Val& port = drain ? v["drain"] : v["source"];
  if (ne(port, "")) {
    make_port(e, port, v["diff_contact_type"]);
  }
  e.popbox();
}

// A via over a poly contact (gf180mcuD-fet.tcl:788-811 and 830-853).
void gate_via(Device& d, std::string_view nudge) {
  Engine& e = d.e;
  const Locals& v = d.v;
  const Val& via_size = v["via_size"];
  const Val& cplfull = v["cplfull"];
  e.pushbox();
  const Val ch = via_size;
  Val cw = mul(cplfull, div(abs(v["viagate"]), "100.0"));
  if (lt(cw, via_size)) {
    cw = via_size;
  }
  e.grow("n", div(ch, "2"));
  e.grow("s", div(ch, "2"));
  const std::string anchor = first_char(v["viagate"]);
  if (anchor == "+") {
    e.move("w", div(sub(cplfull, via_size), "2"));
    e.grow("e", cw);
  } else if (anchor == "-") {
    e.move("e", div(sub(cplfull, via_size), "2"));
    e.grow("w", cw);
  } else {
    e.grow("e", div(cw, "2"));
    e.grow("w", div(cw, "2"));
  }
  e.run({"box", "move", nudge, "0.005um"});
  if (eq(v["poverlap"], "0") && gt(v["m"], "1")) {
    e.run({"box", "move", nudge, "0.055um"});
  }
  via1_draw(e);
  e.popbox();
}

void poly_contact(Device& d, List& cext, std::string_view dir) {
  Engine& e = d.e;
  const Locals& v = d.v;
  e.pushbox();
  e.move(dir, v["hw"]);
  e.move(dir, v["gate_to_polycont"]);
  if (ne(v["viagate"], "0")) {
    gate_via(d, dir == "n" ? "s" : "n");
  }
  cext = unionbox(cext, draw_contact(e, v["cpl"], "0", v["poly_surround"], v["metal_surround"], v["contact_size"],
                                     v["poly_type"], v["poly_contact_type"], "m1", "horz"));
  const bool label = dir == "n" ? ne(v["gate"], "") : (ne(v["gate"], "") && eq(v["topc"], "0"));
  if (label) {
    make_port(e, v["gate"], v["poly_contact_type"]);
  }
  e.popbox();
}

void mos_device_defaults(Locals& v) {
  for (const auto& [name, value] : std::vector<std::pair<const char*, const char*>>{
           {"diffcov", "100"}, {"polycov", "100"}, {"topc", "1"},    {"botc", "1"},    {"dev_sub_type", ""},
           {"evens", "1"},     {"viasrc", "0"},    {"viadrn", "0"},  {"viagate", "0"}, {"drain", ""},
           {"source", ""},     {"gate", ""}}) {
    v.set(name, Val{value});
  }
}

void drain_defaults(Locals& v, const Dict& parameters) {
  if (!parameters.has("drain_diff_type")) {
    v.set("drain_diff_type", v["diff_type"]);
  }
  if (!parameters.has("drain_diff_contact_type")) {
    v.set("drain_diff_contact_type", v["diff_contact_type"]);
  }
  if (!parameters.has("drain_diff_extension")) {
    v.set("drain_diff_extension", v["diff_extension"]);
  }
}

List mos_device(Engine& e, const Dict& parameters) {
  Device d{e, {}};
  Locals& v = d.v;
  mos_device_defaults(v);
  v.load(parameters);
  drain_defaults(v, parameters);

  e.pushbox();
  e.run({"box", "size", "0", "0"});
  std::vector<std::string> gaterect;
  List cext = draw_diff_and_poly(d, gaterect);
  contact_geometry(v);
  diffusion_contact(d, cext, true);
  diffusion_contact(d, cext, false);
  if (v["topc"].truth()) {
    poly_contact(d, cext, "n");
  }
  if (v["botc"].truth()) {
    poly_contact(d, cext, "s");
  }
  // Now draw the gate, after contacts have been drawn
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
  e.popbox();
  if (ne(v["dev_sub_type"], "")) {
    e.run({"box", "values", um(cext.at(0)), um(cext.at(1)), um(cext.at(2)), um(cext.at(3))});
    for (const char* dir : {"n", "s", "e", "w"}) {
      e.grow(dir, v["sub_surround"]);
    }
    e.paint(v["dev_sub_type"]);
    cext = getbox(e);
  }
  e.popbox();
  return cext;
}

// ---- gf180mcuD-fet.tcl: mos_draw -------------------------------------------------

struct Tiling {
  Val dx;
  Val dy;
  Val corellx;
  Val corelly;
  Val gx;
  Val gy;
};

void tile_geometry(Locals& v, const List& bbox, Tiling& t) {
  const Val fw = sub(bbox.at(2), bbox.at(0));
  const Val fh = sub(bbox.at(3), bbox.at(1));
  const Val lw = add(bbox.at(2), bbox.at(0));
  const Val lh = add(bbox.at(3), bbox.at(1));
  t.dy = eq(v["poverlap"], "0") ? add(fh, v["poly_spacing"])
                                : sub(fh, add(v["poly_surround"], v["poly_surround"], v["contact_size"]));
  t.dx = eq(v["doverlap"], "0") ? add(fw, v["diff_spacing"])
                                : sub(fw, add(v["diff_surround"], v["diff_surround"], v["contact_size"]));
  v.set("corex", add(mul(sub(v["nf"], "1"), t.dx), fw));
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

Val guard_extent(const Locals& v, const char* core, const char* space) {
  const bool sub_dist = gt(v["dev_sub_dist"], "0") && gt(add(v["dev_sub_dist"], v["sub_surround"]), v[space]);
  const Val inner = sub_dist ? add(v["dev_sub_dist"], v["diff_surround"]) : add(v[space], v["diff_surround"]);
  return add(v[core], mul("2.0", inner), v["contact_size"]);
}

void guard_geometry(const Locals& v, Tiling& t) {
  t.gx = guard_extent(v, "corex", "diff_spacing");
  t.gy = guard_extent(v, "corey", "diff_gate_space");
  if (eq(v["guard"], "0")) {
    return;
  }
  // `-$w` is text: a minus sign before w's string.
  const Val inset = div(add(v["contact_size"], mul("2.0", v["diff_surround"]), Val{"-" + v["w"].str()}), "2.0");
  const Val sdiff = sub(add(inset, v["diff_spacing"]), add(v["gate_extension"], v["diff_gate_space"]));
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
}

// Without a guard ring, the substrate type around the core.
void device_substrate(Engine& e, Locals& v) {
  e.pushbox();
  if (eq(v["dev_surround"], "0")) {
    v.set("dev_surround", v["sub_surround"]);
  }
  const Val hgx = add(v["dev_surround"], div(v["corex"], "2"));
  const Val hgy = add(v["dev_surround"], div(v["corey"], "2"));
  e.grow("e", hgx);
  e.grow("w", hgx);
  e.grow("n", hgy);
  e.grow("s", hgy);
  e.paint(v["sub_type"]);
  e.popbox();
}

// The port labels of one device of the array (gf180mcuD-fet.tcl:1032-1083).
void port_labels(const Locals& v, Dict& parameters, const Val& xp, const Val& yp, const Val& evens) {
  const std::string x = xp.str();
  const std::string y = yp.str();
  const bool overlap_drain = v["doverlap"].truth() && evens.truth() && gt(xp, "0");
  const bool overlap_source = v["doverlap"].truth() && eq(evens, "0") && lt(xp, sub(v["nf"], "1"));
  const bool single_finger = eq(v["nf"], "1");
  if (eq(v["m"], "1")) {
    if (single_finger) {
      parameters.set("drain", "D");
      parameters.set("source", "S");
      parameters.set("gate", "G");
      return;
    }
    parameters.set("drain", overlap_drain ? std::string{} : "D" + x);
    parameters.set("source", overlap_source ? std::string{} : "S" + x);
    parameters.set("gate", "G" + x);
    return;
  }
  const bool first_row = eq(yp, "0");
  const bool poverlap = v["poverlap"].truth();
  if (single_finger) {
    parameters.set("drain", "D" + y);
    parameters.set("source", "S" + y);
    parameters.set("gate", poverlap ? (first_row ? std::string{"G"} : std::string{}) : "G" + y);
    return;
  }
  parameters.set("drain", overlap_drain ? std::string{} : "D" + x + "_" + y);
  parameters.set("source", overlap_source ? std::string{} : "S" + x + "_" + y);
  parameters.set("gate", poverlap ? (first_row ? "G" + x : std::string{}) : "G" + x + "_" + y);
}

void tile_devices(Engine& e, const Locals& v, Dict& parameters, const Tiling& t) {
  Val evens = "1";
  for (Val xp = Val::integer(0); lt(xp, v["nf"]); xp = add(xp, "1")) {
    parameters.set("evens", evens);
    evens = sub("1", evens);
    e.pushbox();
    for (Val yp = Val::integer(0); lt(yp, v["m"]); yp = add(yp, "1")) {
      if (v["doports"].truth()) {
        port_labels(v, parameters, xp, yp, evens);
      }
      static_cast<void>(mos_device(e, parameters));
      e.move("n", t.dy);
    }
    e.popbox();
    e.move("e", t.dx);
  }
}

std::string mos_draw(Engine& e, Dict parameters) {
  e.run({"tech", "unlock", "*"});
  Locals v;
  for (const auto& [name, value] : std::vector<std::pair<const char*, const char*>>{
           {"poverlap", "0"}, {"doverlap", "1"}, {"dev_sub_dist", "0"}, {"dev_surround", "0"}}) {
    v.set(name, Val{value});
  }
  v.load(parameters);
  if (eq(v["poverlap"], "1")) {
    v.set("topc", "1");
    v.set("botc", "1");
    parameters.set("topc", "1");
    parameters.set("botc", "1");
  }
  v.set("w", e.spice2float(v["w"]));
  v.set("l", e.spice2float(v["l"]));
  e.pushbox();
  e.run({"box", "values", "0", "0", "0", "0"});
  // The base device dimensions, from one device drawn with all layers locked.
  e.run({"tech", "lock", "*"});
  const List bbox = mos_device(e, parameters);
  e.run({"tech", "unlock", "*"});

  Tiling t;
  tile_geometry(v, bbox, t);
  guard_geometry(v, t);
  if (ne(v["guard"], "0")) {
    static_cast<void>(guard_ring(e, t.gx, t.gy, parameters));
  } else {
    device_substrate(e, v);
  }
  e.pushbox();
  e.move("w", t.corellx);
  e.move("s", t.corelly);
  tile_devices(e, v, parameters, t);
  e.popbox();
  e.popbox();
  return e.run({"tech", "revert"});
}

// ---- gf180mcuD-fet.tcl: mos_check ---------------------------------------------

Val incr(const Val& v, std::int64_t by) {
  const tcl::Number n = v.num();
  if (!n.integer) {
    throw TclError{"expected integer but got \"" + v.str() + "\""};
  }
  return Val::integer(n.i + by);
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

void check_bounds(Engine& e, ParameterDict& parameters, Locals& v) {
  if (lt(v["l"], v["lmin"])) {
    e.puts(true, "Mos length must be >= " + v["lmin"].str() + " um");
    parameters.set("l", v["lmin"].str());
  }
  if (gt(v["lmax"], "0") && gt(v["l"], v["lmax"])) {
    e.puts(true, "MOS length must be <= " + v["lmax"].str());
    parameters.set("l", v["lmax"].str());
    v.set("l", v["lmax"]);
  }
  if (lt(v["w"], v["wmin"])) {
    e.puts(true, "Mos width must be >= " + v["wmin"].str() + " um");
    parameters.set("w", v["wmin"].str());
  }
  if (lt(v["nf"], "1")) {
    e.puts(true, "NF must be >= 1");
    parameters.set("nf", "1");
  }
  if (lt(v["m"], "1")) {
    e.puts(true, "M must be >= 1");
    parameters.set("m", "1");
  }
  check_percent(e, parameters, v["diffcov"], "diffcov", "Diffusion");
  check_percent(e, parameters, v["polycov"], "polycov", "Poly");
}

// The tap-spacing loop (gf180mcuD-fet.tcl:1648-1676); a loop that never
// ends in Tcl is refused.
void check_extent(Engine& e, ParameterDict& parameters, Locals& v) {
  const Val origm = v["m"];
  const Val orignf = v["nf"];
  std::set<std::string> seen;
  while (true) {
    const std::string state = v["w"].str() + " " + v["l"].str() + " " + v["m"].str() + " " + v["nf"].str();
    if (!seen.insert(state).second) {
      throw Unsupported{"mos_check does not terminate"};
    }
    const Val yext = expr_text("(" + v["w"].str() + " + 3.0) * " + v["m"].str());
    const Val xext = expr_text("(" + v["l"].str() + " + 1.0) * " + v["nf"].str() + " + 1.1");
    if (!gt(min(xext, yext), "40.0")) {
      break;
    }
    if (gt(yext, "40.0") && gt(v["m"], "1")) {
      v.set("m", incr(v["m"], -1));
    } else if (gt(xext, "40.0") && gt(v["nf"], "1")) {
      v.set("nf", incr(v["nf"], -1));
    } else if (gt(yext, "40.0")) {
      v.set("w", "37");
      e.puts_nonewline(true, "Transistor width must be < 37 um");
      e.puts(true, " to avoid tap spacing violation.");
      parameters.set("w", "37");
    } else if (gt(xext, "40.0")) {
      v.set("l", "37.9");
      e.puts_nonewline(true, "Transistor length must be < 37.9 um");
      e.puts(true, " to avoid tap spacing violation.");
      parameters.set("l", "37.9");
    }
  }
  if (ne(v["m"], origm)) {
    e.puts(true, "Y repeat reduced to prevent tap distance violation");
    parameters.set("m", v["m"].str());
  }
  if (ne(v["nf"], orignf)) {
    e.puts(true, "X repeat reduced to prevent tap distance violation");
    parameters.set("nf", v["nf"].str());
  }
}

ParameterDict mos_check(Engine& e, ParameterDict parameters) {
  Locals v;
  v.set("lmax", "0");
  v.load(Dict::from(parameters));
  v.set("l", e.three_digit_past_decimal(e.spice2float(v["l"])));
  v.set("w", e.three_digit_past_decimal(e.spice2float(v["w"])));
  if (!string_is_int(v["nf"])) {
    e.puts(true, "NF must be an integer!");
    parameters.set("nf", "1");
  }
  if (!string_is_int(v["m"])) {
    e.puts(true, "M must be an integer!");
    parameters.set("m", "1");
  }
  // "diffcov must be numeric!" / "polycov must be numeric!" are left to Tcl:
  // `expr abs($x)` evaluates the value's text.
  for (const char* key : {"diffcov", "polycov"}) {
    if (!v[key].is_number()) {
      throw Unsupported{std::string{"expr abs($"} + key + ")"};
    }
  }
  check_bounds(e, parameters, v);
  check_extent(e, parameters, v);
  return parameters;
}

// ---- the generators ------------------------------------------------------------

const Dict& ruleset() {
  // gf180mcuD.tcl:51-75 (sblk_diff_space is set twice; the second value wins)
  static const Dict kRules = {
      {"poly_surround", "0.065"},  {"diff_surround", "0.065"}, {"gate_to_diffcont", "0.26"},
      {"gate_to_polycont", "0.28"}, {"gate_extension", "0.22"}, {"diff_extension", "0.23"},
      {"contact_size", "0.23"},    {"via_size", "0.26"},       {"metal_surround", "0.055"},
      {"sub_surround", "0.12"},    {"diff_spacing", "0.33"},   {"poly_spacing", "0.24"},
      {"diffres_spacing", "0.40"}, {"polyres_spacing", "0.40"}, {"diff_poly_space", "0.10"},
      {"diff_gate_space", "0.11"}, {"metal_spacing", "0.23"},  {"mmetal_spacing", "0.38"},
      {"sblk_to_cont", "0.33"},    {"sblk_diff_space", "0.44"}, {"sblk_diff_space", "0.60"}};
  return kRules;
}

const std::set<std::string, std::less<>>& text_keys() {
  static const std::set<std::string, std::less<>> kKeys = {
      "class",          "compatible",       "gate_type",       "diff_type",
      "diff_contact_type", "plus_diff_type", "plus_contact_type", "poly_type",
      "poly_contact_type", "sub_type",       "dev_sub_type",    "drain_diff_type",
      "drain_diff_contact_type", "drain",    "source",          "gate",
      "bulk",           "gencell"};
  return kKeys;
}

void require_drawable(const Dict& drawdict) {
  require_numbers(drawdict, text_keys());
  for (const char* key : {"nf", "m"}) {
    if (drawdict.has(key) && gt(drawdict.get(key), "1000")) {
      throw Unsupported{"array too large"};
    }
  }
}

void require_checkable(const ParameterDict& parameters) {
  Locals v;
  v.load(Dict::from(parameters));
  for (const char* key : {"l", "w", "lmin", "wmin", "nf", "m"}) {
    if (!v[key].is_number()) {
      throw Unsupported{std::string{key} + " is not a number"};
    }
  }
  if (v.has("lmax") && !v["lmax"].is_number()) {
    throw Unsupported{"lmax is not a number"};
  }
}

struct MosRow {
  const char* type;
  const char* check_device;  // empty: gf180mcu::mos_check takes no device name
  const char* defaults;
  std::vector<std::pair<std::string, Val>> newdict;
};

class Gf180Mos final : public NativeGenerator {
 public:
  explicit Gf180Mos(const MosRow& row) : row_(row) {
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
      Engine e{buffered, HelperFamily::gf180mcu};
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
      Engine e{host, HelperFamily::gf180mcu};
      return cmd::CommandResult::success(mos_draw(e, drawdict));
    });
  }

 private:
  const MosRow& row_;
  Dict newdict_;
};

const std::vector<MosRow>& rows() {
  // clang-format off
  static const std::vector<MosRow> kRows = {
#include "pcell/native/gf180mcu/mos_table.inc"
  };
  // clang-format on
  return kRows;
}

}  // namespace

void add_gf180mcu_mosfets(NativeRegistry& registry, const script::PdkIdentity& pdk) {
  for (const auto& row : rows()) {
    registry.add(pdk, std::string{"gf180mcu::"} + row.type, std::make_unique<Gf180Mos>(row));
  }
}

}  // namespace hefesto::pcell::native
