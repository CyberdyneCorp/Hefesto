// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Native sky130 MOSFET and MOS varactor generators (design D10; D-29): a
// port of the procedures every `sky130::<device>_draw` and `_check` of
// sky130A-fet.tcl delegates to, issuing the same engine commands with the
// same words. PDK ref (open_pdks c6df28c1, ciel release 5740c4a8):
//   sky130A.tcl:48-73           sky130::ruleset
//   sky130A-util.tcl:25-131     getbox, unionbox, draw_contact
//   sky130A-util.tcl:133-426    guard_ring
//   sky130A-via.tcl:7-36        mcon_draw
//   sky130A-fet.tcl:29-212      the _defaults dictionaries
//   sky130A-fet.tcl:452-618     draw_ldnmos_drain, draw_ldpmos_drain
//   sky130A-fet.tcl:620-1049    mos_device
//   sky130A-fet.tcl:1055-1372   mos_draw
//   sky130A-fet.tcl:1378-1702   the per-device draw dictionaries
//   sky130A-fet.tcl:1708-1877   mos_check
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

// ---- sky130A-util.tcl / sky130A-via.tcl ------------------------------------

class Sky130 {
 public:
  explicit Sky130(Engine& e) : e_(e) {}

  // sky130::getbox
  List getbox() { return native::getbox(e_); }

  List draw_contact(const Val& w, const Val& h, const Val& s, const Val& o, const Val& x, const Val& atype,
                    const Val& ctype, const Val& mtype, std::string_view orient = "vert");
  void mcon_draw(std::string_view dir = "default");
  List guard_ring(const Val& gw, const Val& gh, const Dict& parameters);

  Engine& e() { return e_; }

 private:
  void guard_side(std::string_view move_dir, const Val& offset, const Val& half_long, bool horizontal,
                  const Locals& v);
  void guard_via(const Val& coverage, std::string_view side, const Locals& v);

  Engine& e_;
};

bool is_orient(std::string_view orient, std::string_view want) {
  return orient == want || orient == "full";
}

List Sky130::draw_contact(const Val& w0, const Val& h0, const Val& s, const Val& o, const Val& x, const Val& atype,
                          const Val& ctype, const Val& mtype, std::string_view orient) {
  // Substrate diffusions (tap) need not overlap the contact in all directions.
  const bool lv_sub = atype.str() == "psd" || atype.str() == "nsd";
  const std::string_view aorient = lv_sub ? orient : std::string_view{"full"};
  e_.pushbox();
  e_.run({"box", "size", "0", "0"});
  const Val w = lt(w0, x) ? x : w0;
  const Val h = lt(h0, x) ? x : h0;
  const Val hw = div(w, "2.0");
  const Val hh = div(h, "2.0");
  e_.grow("n", hh);
  e_.grow("s", hh);
  e_.grow("e", hw);
  e_.grow("w", hw);
  e_.paint(ctype);
  e_.pushbox();
  if (is_orient(aorient, "vert")) {
    e_.grow("n", s);
    e_.grow("s", s);
  }
  if (is_orient(aorient, "horz")) {
    e_.grow("e", s);
    e_.grow("w", s);
  }
  e_.paint(atype);
  List extents = getbox();
  e_.popbox();
  if (is_orient(orient, "vert")) {
    e_.grow("n", o);
    e_.grow("s", o);
  }
  if (is_orient(orient, "horz")) {
    e_.grow("e", o);
    e_.grow("w", o);
  }
  e_.paint(mtype);
  e_.popbox();
  return extents;
}

void Sky130::mcon_draw(std::string_view dir) {
  const Val w = e_.i2u(Val{e_.run({"box", "width"})});
  const Val h = e_.i2u(Val{e_.run({"box", "height"})});
  if (lt(w, "0.17")) {
    e_.puts(true, "Mcon width must be at least 0.17um");
    return;
  }
  if (lt(h, "0.17")) {
    e_.puts(true, "Mcon height must be at least 0.17um");
    return;
  }
  e_.run({"suspendall"});
  e_.run({"paint", "mcon"});
  e_.pushbox();
  const bool vertical = lt(w, h) || dir == "vert";
  e_.run({"box", "grow", vertical ? "e" : "n", "0.03um"});
  e_.run({"box", "grow", vertical ? "w" : "s", "0.03um"});
  e_.run({"box", "grow", vertical ? "n" : "e", "0.06um"});
  e_.run({"box", "grow", vertical ? "s" : "w", "0.06um"});
  e_.run({"paint", "m1"});
  e_.popbox();
  e_.run({"resumeall"});
}

// One side of the guard ring's diffusion (and the guard substrate around it).
void Sky130::guard_side(std::string_view move_dir, const Val& offset, const Val& half_long, bool horizontal,
                        const Locals& v) {
  const Val& hdifft = v["hdifft"];
  e_.move(move_dir, offset);
  e_.grow(horizontal ? "n" : "e", hdifft);
  e_.grow(horizontal ? "s" : "w", hdifft);
  e_.grow(horizontal ? "e" : "n", half_long);
  e_.grow(horizontal ? "w" : "s", half_long);
  e_.paint(v["plus_diff_type"]);
  if (gt(v["guard_sub_surround"], "0")) {
    e_.grow("c", v["guard_sub_surround"]);
    e_.paint(v["guard_sub_type"]);
  }
}

// The guard ring vias of one side (viagb, viagt, viagr, viagl).
void Sky130::guard_via(const Val& coverage, std::string_view side, const Locals& v) {
  const bool horizontal = side == "s" || side == "n";
  const Val& via_size = v["via_size"];
  const Val& full = horizontal ? v["gw"] : v["gh"];
  e_.pushbox();
  Val ch;
  Val cw;
  if (horizontal) {
    ch = via_size;
    cw = mul(sub(full, via_size), div(abs(coverage), "100.0"));
    if (lt(cw, via_size)) {
      cw = via_size;
    }
    e_.move(side, horizontal ? v["hh"] : v["hw"]);
    e_.grow("n", div(ch, "2"));
    e_.grow("s", div(ch, "2"));
  } else {
    ch = mul(sub(full, via_size), div(abs(coverage), "100.0"));
    if (lt(ch, via_size)) {
      ch = via_size;
    }
    cw = via_size;
    e_.move(side, v["hw"]);
    e_.grow("e", div(cw, "2"));
    e_.grow("w", div(cw, "2"));
  }
  const std::string anchor = first_char(coverage);
  const Val& along = horizontal ? cw : ch;
  const std::string_view low = horizontal ? "w" : "s";
  const std::string_view high = horizontal ? "e" : "n";
  if (anchor == "+") {
    e_.move(low, div(sub(full, via_size), "2"));
    e_.grow(high, along);
  } else if (anchor == "-") {
    e_.move(high, div(sub(full, via_size), "2"));
    e_.grow(low, along);
  } else {
    e_.grow(high, div(along, "2"));
    e_.grow(low, div(along, "2"));
  }
  mcon_draw(horizontal ? "horz" : "vert");
  e_.popbox();
}

Locals guard_ring_locals(const Val& gw, const Val& gh, const Dict& parameters) {
  Locals v;
  for (const auto& [name, value] :
       std::vector<std::pair<const char*, const char*>>{{"rlcov", "100"},     {"tbcov", "100"},
                                                        {"grc", "1"},         {"glc", "1"},
                                                        {"gtc", "1"},         {"gbc", "1"},
                                                        {"viagb", "0"},       {"viagt", "0"},
                                                        {"viagr", "0"},       {"viagl", "0"},
                                                        {"full_metal", "1"},  {"guard_sub_type", "pwell"},
                                                        {"guard_sub_surround", "0"}, {"plus_diff_type", "nsd"},
                                                        {"plus_contact_type", "nsc"}, {"sub_type", "pwell"},
                                                        {"bulk", ""}}) {
    v.set(name, Val{value});
  }
  v.set("gw", gw);
  v.set("gh", gh);
  v.load(parameters);  // as in Tcl, a key named gw or gh replaces the argument
  if (!parameters.has("guard_sub_type")) {
    v.set("guard_sub_type", v["sub_type"]);
  }
  v.set("hx", div(v["contact_size"], "2.0"));
  v.set("hw", div(gw, "2.0"));
  v.set("hh", div(gh, "2.0"));
  const std::string& plus = v["plus_diff_type"].str();
  if (plus == "psd" || plus == "nsd") {
    v.set("diff_surround", "0");
  }
  v.set("difft", add(v["contact_size"], v["diff_surround"], v["diff_surround"]));
  v.set("hdifft", div(v["difft"], "2.0"));
  v.set("hdiffw", div(add(gw, v["difft"]), "2.0"));
  v.set("hdiffh", div(add(gh, v["difft"]), "2.0"));
  return v;
}

List Sky130::guard_ring(const Val& gw, const Val& gh, const Dict& parameters) {
  Locals v = guard_ring_locals(gw, gh, parameters);
  e_.pushbox();
  e_.run({"box", "size", "0", "0"});

  e_.pushbox();
  guard_side("n", v["hh"], v["hdiffw"], true, v);
  e_.popbox();
  e_.pushbox();
  e_.move("s", v["hh"]);
  e_.pushbox();
  e_.grow("n", v["hdifft"]);
  e_.grow("s", v["hdifft"]);
  e_.grow("e", v["hdiffw"]);
  e_.grow("w", v["hdiffw"]);
  e_.paint(v["plus_diff_type"]);
  if (gt(v["guard_sub_surround"], "0")) {
    e_.grow("c", v["guard_sub_surround"]);
    e_.paint(v["guard_sub_type"]);
  }
  e_.popbox();
  // At guard ring bottom center, place a port if requested
  if (ne(v["bulk"], "")) {
    e_.run({"label", v["bulk"].str(), "c", v["plus_diff_type"].str()});
    e_.run({"select", "area", "label"});
    e_.run({"port", "make"});
  }
  e_.popbox();
  e_.pushbox();
  guard_side("e", v["hw"], v["hdiffh"], false, v);
  e_.popbox();
  e_.pushbox();
  guard_side("w", v["hw"], v["hdiffh"], false, v);
  e_.popbox();

  if (v["full_metal"].truth()) {
    const Val hmetw = div(add(gw, v["contact_size"]), "2.0");
    const Val hmeth = div(add(gh, v["contact_size"]), "2.0");
    for (const auto& [dir, horizontal] : std::vector<std::pair<const char*, bool>>{
             {"n", true}, {"s", true}, {"e", false}, {"w", false}}) {
      e_.pushbox();
      e_.move(dir, horizontal ? v["hh"] : v["hw"]);
      e_.grow(horizontal ? "n" : "e", v["hx"]);
      e_.grow(horizontal ? "s" : "w", v["hx"]);
      e_.grow(horizontal ? "e" : "n", horizontal ? hmetw : hmeth);
      e_.grow(horizontal ? "w" : "s", horizontal ? hmetw : hmeth);
      e_.run({"paint", "li"});
      e_.popbox();
    }
  }

  // Contact metal reaches to the ends, scaled by the coverage.
  const Val inset = mul(add(v["metal_surround"], v["metal_spacing"]), "2.0");
  Val ch = mul(sub(gh, v["contact_size"], inset), div(v["rlcov"], "100.0"));
  if (lt(ch, v["contact_size"])) {
    ch = v["contact_size"];
  }
  Val cw = mul(sub(gw, v["contact_size"], inset), div(v["tbcov"], "100.0"));
  if (lt(cw, v["contact_size"])) {
    cw = v["contact_size"];
  }
  const auto side_contact = [&](const char* dir, const Val& half, const Val& cwv, const Val& chv, const char* o) {
    e_.pushbox();
    e_.move(dir, half);
    static_cast<void>(draw_contact(cwv, chv, v["diff_surround"], v["metal_surround"], v["contact_size"],
                                   v["plus_diff_type"], v["plus_contact_type"], "li", o));
    e_.popbox();
  };
  if (gt(v["tbcov"], "0.0")) {
    if (eq(v["gtc"], "1")) {
      side_contact("n", v["hh"], cw, "0", "horz");
    }
    if (eq(v["gbc"], "1")) {
      side_contact("s", v["hh"], cw, "0", "horz");
    }
  }
  if (gt(v["rlcov"], "0.0")) {
    if (eq(v["grc"], "1")) {
      side_contact("e", v["hw"], "0", ch, "vert");
    }
    if (eq(v["glc"], "1")) {
      side_contact("w", v["hw"], "0", ch, "vert");
    }
  }

  // Vias
  for (const auto& [key, side] : std::vector<std::pair<const char*, const char*>>{
           {"viagb", "s"}, {"viagt", "n"}, {"viagr", "e"}, {"viagl", "w"}}) {
    if (ne(v[key], "0")) {
      guard_via(v[key], side, v);
    }
  }

  e_.pushbox();
  e_.grow("e", v["hw"]);
  e_.grow("w", v["hw"]);
  e_.grow("n", v["hh"]);
  e_.grow("s", v["hh"]);
  // Create boundary using properties
  const std::string bbox = e_.run({"box", "values"});
  e_.run({"property", "FIXED_BBOX", bbox});
  e_.grow("c", v["hx"]);
  e_.grow("c", v["diff_surround"]);
  e_.grow("c", v["sub_surround"]);
  e_.paint(v["sub_type"]);
  List cext = getbox();
  e_.popbox();
  e_.popbox();
  return cext;
}

// ---- sky130A-fet.tcl: mos_device ---------------------------------------------

struct Device {
  Sky130& pdk;
  Engine& e;
  Dict parameters;
  Locals v;
};

// A via over a diffusion contact (drain or source), sky130A-fet.tcl:802-824.
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
  d.pdk.mcon_draw("vert");
  e.popbox();
}

// A via over a poly contact, and the conn_gates metal (sky130A-fet.tcl:906-936).
void gate_via(Device& d) {
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
  d.pdk.mcon_draw("horz");
  e.popbox();
  e.pushbox();
  if (eq(v["conn_gates"], "1")) {
    e.grow("n", div(ch, "2"));
    e.grow("s", div(ch, "2"));
    e.grow("w", add(v["he"], v["gate_to_diffcont"]));
    e.grow("e", add(v["he"], v["gate_to_diffcont"]));
    e.run({"paint", "m1"});
  }
  e.popbox();
}

void make_port(Engine& e, const Val& name, const Val& type) {
  e.run({"label", name.str(), "c", type.str()});
  e.run({"select", "area", "label"});
  e.run({"port", "make"});
}

// The top (`n`) or bottom (`s`) poly contact (sky130A-fet.tcl:899-1015).
void poly_contact(Device& d, List& cext, std::string_view dir) {
  Engine& e = d.e;
  const Locals& v = d.v;
  e.pushbox();
  e.move(dir, v["hw"]);
  e.move(dir, v["gate_to_polycont"]);
  if (ne(v["viagate"], "0")) {
    gate_via(d);
  }
  cext = unionbox(cext, d.pdk.draw_contact(v["cpl"], "0", v["poly_surround"], v["metal_surround"],
                                           v["contact_size"], v["poly_type"], v["poly_contact_type"], "li", "horz"));
  if (eq(v["conn_gates"], "1")) {
    e.pushbox();
    e.grow("n", div(v["contact_size"], "2"));
    e.grow("s", div(v["contact_size"], "2"));
    e.grow("w", add(v["he"], v["gate_to_diffcont"]));
    e.grow("e", add(v["he"], v["gate_to_diffcont"]));
    e.run({"paint", "li"});
    if (eq(v["poly_merge"], "1")) {
      e.grow("n", v["poly_surround"]);
      e.grow("s", v["poly_surround"]);
      e.run({"paint", "poly"});
    }
    e.popbox();
  }
  const bool label = dir == "n" ? ne(v["gate"], "") : (ne(v["gate"], "") && eq(v["topc"], "0"));
  if (label) {
    make_port(e, v["gate"], v["poly_contact_type"]);
  }
  e.popbox();
}

// The gate shield of the varactors (sky130A-fet.tcl:878-897).
void gate_shield(Device& d) {
  Engine& e = d.e;
  const Locals& v = d.v;
  e.pushbox();
  e.move("w", v["he"]);
  e.move("w", v["gate_to_diffcont"]);
  e.run({"box", "width", um(mul("2", add(v["he"], v["gate_to_diffcont"])))});
  e.grow("n", div(v["cdwfull"], "2"));
  e.grow("s", div(v["cdwfull"], "2"));
  e.run({"paint", "m1"});
  // Enforce slotting of large metal
  const Val gsh = e.i2u(Val{e.run({"box", "height"})});
  if (gt(gsh, "25")) {
    e.move("n", div(gsh, "2"));
    e.run({"box", "move", "s", "1.15um"});
    e.run({"box", "height", "2.3um"});
    e.run({"box", "grow", "w", "-" + um(v["via_size"])});
    e.run({"box", "grow", "e", "-" + um(v["via_size"])});
    e.run({"erase", "m1"});
  }
  e.popbox();
}

List ldmos_drain(Sky130& pdk, const Dict& parameters, bool nmos);

// Diffusion and poly of the device; the gate rectangle is returned in
// `gaterect` and painted last (sky130A-fet.tcl:657-712).
List draw_diff_and_poly(Device& d, std::vector<std::string>& gaterect) {
  Engine& e = d.e;
  Locals& v = d.v;
  e.pushbox();
  v.set("hw", div(v["w"], "2.0"));
  v.set("hl", div(v["l"], "2.0"));
  v.set("he", div(v["min_effl"], "2.0"));
  if (eq(v["nf"], "1") || lt(v["he"], v["hl"])) {
    v.set("he", v["hl"]);
  }
  e.grow("n", v["hw"]);
  e.grow("s", v["hw"]);
  e.grow("e", v["hl"]);
  e.grow("w", v["hl"]);
  const bool evens0 = eq(v["evens"], "0");
  v.set("dside", evens0 ? "e" : "w");
  v.set("sside", evens0 ? "w" : "e");
  const std::string dside = v["dside"].str();
  const std::string sside = v["sside"].str();

  e.pushbox();
  const Val& grow_diff =
      gt(v["diff_extension"], v["gate_to_diffcont"]) ? v["diff_extension"] : v["gate_to_diffcont"];
  e.grow(dside, grow_diff);
  e.grow(sside, grow_diff);
  e.paint(v["diff_type"]);
  e.popbox();
  e.pushbox();
  if (gt(v["gate_extension"], v["gate_to_polycont"])) {
    e.grow("n", v["gate_extension"]);
    e.grow("s", v["gate_extension"]);
  } else {
    e.grow("n", v["topc"].truth() ? v["gate_to_polycont"] : v["gate_extension"]);
    e.grow("s", v["botc"].truth() ? v["gate_to_polycont"] : v["gate_extension"]);
  }
  e.paint(v["poly_type"]);
  List cext = d.pdk.getbox();
  e.popbox();
  auto split = split_tcl_list(e.run({"box", "values"}));
  gaterect = split.ok() ? std::move(split).value() : std::vector<std::string>{};
  e.popbox();
  return cext;
}

// The dogbone rules 1-4 and the contact sizes (sky130A-fet.tcl:714-786).
void contact_geometry(Device& d) {
  Locals& v = d.v;
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
  const Val tsurround = add(v["diff_surround"], v["diff_overlap_cont"]);
  v.set("cdw", sub(v["w"], mul(tsurround, "2")));
  v.set("cpl", sub(v["l"], mul(v["poly_surround"], "2")));
  v.set("cdwfull", v["cdw"]);
  v.set("cplfull", v["cpl"]);
  v.set("cdw", mul(v["cdw"], div(v["diffcov"], "100.0")));
  if (eq(v["poverlap"], "0") || eq(v["m"], "1")) {
    v.set("cpl", mul(v["cpl"], div(v["polycov"], "100.0")));
  }
}

// The drain or source diffusion contact with its via and port.
void diffusion_contact(Device& d, List& cext, bool drain) {
  Engine& e = d.e;
  const Locals& v = d.v;
  const std::string side = (drain ? v["dside"] : v["sside"]).str();
  const std::string other = (drain ? v["sside"] : v["dside"]).str();
  e.pushbox();
  e.move(side, v["he"]);
  e.move(side, v["gate_to_diffcont"]);
  const Val& viatype = drain ? v["viadrn"] : v["viasrc"];
  if (ne(viatype, "0")) {
    diffusion_via(d, viatype, side, other);
  }
  cext = unionbox(cext, d.pdk.draw_contact("0", v["cdw"], v["diff_surround"], v["metal_surround"],
                                           v["contact_size"], v["diff_type"], v["diff_contact_type"], "li", "vert"));
  const Val& port = drain ? v["drain"] : v["source"];
  if (ne(port, "")) {
    make_port(e, port, v["diff_contact_type"]);
  }
}

void mos_device_defaults(Locals& v) {
  for (const auto& [name, value] : std::vector<std::pair<const char*, const char*>>{
           {"doports", "0"},     {"diffcov", "100"},       {"polycov", "100"},   {"topc", "1"},
           {"botc", "1"},        {"viasrc", "100"},        {"viadrn", "100"},    {"viagate", "100"},
           {"evens", "1"},       {"dev_sub_type", ""},     {"dev_sub_dist", "0"}, {"min_effl", "0"},
           {"diff_overlap_cont", "0"}, {"gshield", "0"},   {"conn_gates", "0"},  {"poly_merge", "0"},
           {"drain_proc", ""},   {"drain", ""},            {"source", ""},       {"gate", ""}}) {
    v.set(name, Val{value});
  }
}

// Paints the gate last, then the device substrate (sky130A-fet.tcl:1018-1046).
void gate_and_substrate(Device& d, List& cext, const std::vector<std::string>& gaterect, const List& diffarea) {
  Engine& e = d.e;
  const Locals& v = d.v;
  e.pushbox();
  std::vector<std::string> words{"box", "values"};
  words.insert(words.end(), gaterect.begin(), gaterect.end());
  e.run(words);
  // gate_type need not be defined if poly over diff paints the right type.
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
    cext = unionbox(cext, d.pdk.getbox());
  }
  e.popbox();
  if (ne(v["dev_sub_type"], "")) {
    e.run({"box", "values", um(diffarea.at(0)), um(diffarea.at(1)), um(diffarea.at(2)), um(diffarea.at(3))});
    for (const char* dir : {"n", "s", "e", "w"}) {
      e.grow(dir, v["sub_surround"]);
    }
    e.paint(v["dev_sub_type"]);
    if (gt(v["dev_sub_dist"], "0")) {
      cext = unionbox(cext, d.pdk.getbox());
    }
  }
}

List mos_device(Sky130& pdk, const Dict& parameters) {
  Device d{pdk, pdk.e(), parameters, {}};
  Engine& e = d.e;
  Locals& v = d.v;
  mos_device_defaults(v);
  v.load(parameters);
  if (!parameters.has("oldtopc")) {
    v.set("oldtopc", v["topc"]);
  }
  if (!parameters.has("oldbotc")) {
    v.set("oldbotc", v["botc"]);
  }

  e.pushbox();
  e.run({"box", "size", "0", "0"});
  std::vector<std::string> gaterect;
  List cext = draw_diff_and_poly(d, gaterect);
  contact_geometry(d);

  if (ne(v["drain_proc"], "")) {
    d.parameters.set("cdw", v["cdw"]);
    d.parameters.set("cdwfull", v["cdwfull"]);
    d.parameters.set("dside", v["dside"]);
    d.parameters.set("sside", v["sside"]);
    d.parameters.set("doports", v["doports"]);
    const bool nmos = v["drain_proc"].str() == "sky130::draw_ldnmos_drain";
    cext = unionbox(cext, ldmos_drain(pdk, d.parameters, nmos));
  } else {
    diffusion_contact(d, cext, true);
    e.popbox();
  }
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
  return cext;
}

// ---- sky130A-fet.tcl: draw_ldnmos_drain, draw_ldpmos_drain --------------------

List ldmos_drain(Sky130& pdk, const Dict& parameters, bool nmos) {
  Engine& e = pdk.e();
  Locals v;
  v.load(parameters);
  const Val hl = div(v["l"], "2.0");
  const Val hw = div(v["w"], "2.0");
  const Val extension = nmos ? "1.455" : "1.06";
  const std::string dside = v["dside"].str();
  const char* sd_type = nmos ? "mvnsd" : "mvpsd";

  e.pushbox();
  e.grow("n", hw);
  e.grow("s", hw);
  e.move(dside, hl);
  e.pushbox();
  e.grow(dside, extension);
  e.run({"paint", "ed"});
  List cext = pdk.getbox();
  e.popbox();
  e.move(dside, extension);
  e.run({"box", "move", dside, "0.13um"});
  e.run({"box", "grow", "e", "0.13um"});
  e.run({"box", "grow", "w", "0.13um"});
  e.run({"paint", sd_type});
  if (nmos) {
    cext = unionbox(cext, pdk.getbox());
    e.pushbox();
    e.run({"box", "grow", "c", "0.66um"});
    e.run({"paint", "nwell"});
    e.popbox();
  } else {
    e.pushbox();
    e.run({"box", "grow", "c", "0.86um"});
    e.run({"box", "grow", "e", "-0.15um"});
    e.run({"box", "grow", "w", "-0.15um"});
    e.run({"paint", "pwell"});
    e.popbox();
  }
  e.popbox();
  e.pushbox();
  e.move(dside, hl);
  e.move(dside, extension);
  e.run({"box", "move", dside, "0.13um"});

  const Val& viatype = v["viadrn"];
  if (ne(viatype, "0")) {
    Device d{pdk, e, parameters, v};
    diffusion_via(d, viatype, dside, v["sside"].str());
  }
  // cdwfull was calculated for the source side and is too long for the
  // drain side by 0.06um on top and bottom. `if {$cdw > $cdwfull} [set cdw
  // $cdwfull]` sets cdw while the command is substituted, so always.
  const Val cdwfull = sub(v["cdwfull"], "0.12");
  const Val cdw = cdwfull;
  cext = unionbox(cext, pdk.draw_contact("0", cdw, v["drain_diff_surround"], v["metal_surround"],
                                         v["contact_size"], sd_type, nmos ? "mvnsc" : "mvpsc", "li", "vert"));
  e.popbox();
  return cext;
}

// ---- sky130A-fet.tcl: mos_draw ------------------------------------------------

struct Tiling {
  Val dx;
  Val dy;
  Val corellx;
  Val corelly;
  Val gx;
  Val gy;
  Val xoffset;
  bool intc = false;
  Val evenodd = "1";
};

void mos_draw_defaults(Locals& v) {
  for (const auto& [name, value] : std::vector<std::pair<const char*, const char*>>{
           {"poverlap", "0"},     {"doverlap", "1"},    {"dev_sub_dist", "0"}, {"dev_sub_space", "0"},
           {"min_allc", "0"},     {"id_type", ""},      {"id_surround", "0"},  {"id2_type", ""},
           {"id2_surround", "0"}, {"conn_gates", "0"},  {"poly_merge", "0"},   {"doports", "0"},
           {"set_x_to_guard", ""}, {"set_y_to_guard", ""}}) {
    v.set(name, Val{value});
  }
}

// Alternating gate contacts or merged poly (sky130A-fet.tcl:1101-1128).
void contact_alternation(Locals& v, Dict& parameters, Tiling& t) {
  if (gt(v["nf"], "1") && lt(v["l"], v["min_allc"])) {
    if (eq(v["conn_gates"], "1")) {
      t.intc = false;
      v.set("poly_merge", "1");
    } else {
      t.intc = true;
      t.evenodd = "1";
      parameters.set("oldtopc", v["topc"]);
      parameters.set("oldbotc", v["botc"]);
      v.set("topc", "1");
      v.set("botc", "1");
      parameters.set("topc", "1");
      parameters.set("botc", "1");
      v.set("poverlap", "0");
      v.set("poly_merge", "0");
    }
  } else {
    t.intc = false;
    v.set("poly_merge", "0");
  }
  parameters.set("poly_merge", v["poly_merge"]);
}

// Tile pitch, core size and guard ring size (sky130A-fet.tcl:1143-1239).
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
    t.dy = eq(v["poverlap"], "0")
               ? add(fh, v["poly_spacing"])
               : sub(fh, add(v["poly_surround"], v["poly_surround"], v["contact_size"]));
    t.dx = eq(v["doverlap"], "0")
               ? add(fw, v["diff_spacing"])
               : sub(fw, add(v["drain_diff_surround"], v["drain_diff_surround"], v["contact_size"]));
  }
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

void guard_geometry(Locals& v, Tiling& t) {
  t.gx = guard_extent(v, "corex", "diff_tap_space");
  t.gy = guard_extent(v, "corey", "diff_gate_space");
  // The guard ring spacing may be limited by diffusion spacing, not poly
  // to diffusion. `-$w` is text: a minus sign before w's string.
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
}

// The port labels of one device of the array (sky130A-fet.tcl:1267-1328).
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
    parameters.set("gate", eq(v["conn_gates"], "0") ? "G" + x : std::string{"G"});
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
  const bool conn = eq(v["conn_gates"], "0");
  if (poverlap && first_row) {
    parameters.set("gate", conn ? "G" + x : std::string{"G"});
  } else if (poverlap) {
    parameters.set("gate", "");
  } else {
    parameters.set("gate", conn ? "G" + x + "_" + y : "G" + y);
  }
}

void alternate_contacts(Dict& parameters, Tiling& t) {
  t.evenodd = sub("1", t.evenodd);
  const bool top = eq(t.evenodd, "1");
  parameters.set("topc", top ? "1" : "0");
  parameters.set("botc", top ? "0" : "1");
}

// One column (finger `xp`) of m devices (sky130A-fet.tcl:1266-1355).
void tile_column(Sky130& pdk, const Locals& v, Dict& parameters, Tiling& t, const Val& xp, const Val& evens) {
  Engine& e = pdk.e();
  const bool shifted = ne(evens, "0");
  for (Val yp = Val::integer(0); lt(yp, v["m"]); yp = add(yp, "1")) {
    if (v["doports"].truth()) {
      port_labels(v, parameters, xp, yp, evens);
    }
    if (shifted) {
      e.move("e", t.xoffset);
    }
    static_cast<void>(mos_device(pdk, parameters));
    if (shifted) {
      e.move("w", t.xoffset);
    }
    e.move("n", t.dy);
    if (t.intc) {
      alternate_contacts(parameters, t);
    }
  }
}

// The nf x m array of devices (sky130A-fet.tcl:1250-1366).
void tile_devices(Sky130& pdk, const Locals& v, Dict& parameters, Tiling& t) {
  Engine& e = pdk.e();
  Val evens = "1";
  for (Val xp = Val::integer(0); lt(xp, v["nf"]); xp = add(xp, "1")) {
    parameters.set("evens", evens);
    evens = sub("1", evens);
    e.pushbox();
    Val saveeo;
    if (t.intc) {
      alternate_contacts(parameters, t);
      saveeo = t.evenodd;
    }
    tile_column(pdk, v, parameters, t, xp, evens);
    if (t.intc) {
      t.evenodd = saveeo;
    }
    e.popbox();
    e.move("e", t.dx);
  }
}

std::string mos_draw(Sky130& pdk, Dict parameters) {
  Engine& e = pdk.e();
  e.run({"tech", "unlock", "*"});
  const std::string savesnap = e.run({"snap"});
  e.run({"snap", "internal"});
  Locals v;
  mos_draw_defaults(v);
  v.load(parameters);
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
  v.set("w", e.spice2float(v["w"]));
  v.set("l", e.spice2float(v["l"]));
  e.pushbox();
  e.run({"box", "values", "0", "0", "0", "0"});
  Tiling t;
  contact_alternation(v, parameters, t);

  // The base device dimensions, from one device drawn with all layers locked.
  e.run({"tech", "lock", "*"});
  const List bbox = mos_device(pdk, parameters);
  e.puts(false, "Diagnostic: Device bounding box e " + list_string(bbox) + " (um)");
  e.run({"tech", "unlock", "*"});

  tile_geometry(v, bbox, t);
  if (ne(v["guard"], "0") || ne(v["id_type"], "")) {
    guard_geometry(v, t);
  }
  if (ne(v["guard"], "0")) {
    if (v["doports"].truth()) {
      parameters.set("bulk", "B");
    }
    static_cast<void>(pdk.guard_ring(t.gx, t.gy, parameters));
  }
  e.pushbox();
  if (ne(v["id_type"], "")) {
    const Val hw = div(t.gx, "2");
    const Val hh = div(t.gy, "2");
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
  tile_devices(pdk, v, parameters, t);
  e.popbox();
  e.popbox();
  e.run({"snap", savesnap});
  return e.run({"tech", "revert"});
}

// ---- sky130A-fet.tcl: mos_check ------------------------------------------------

struct Coverage {
  const char* key;
  const char* name;  // in the messages
};

// `catch {expr abs($x)}`: the expression is the value's text, so a
// non-number may still evaluate (`1+1`); only numbers are reproduced.
void require_abs(const Val& value) {
  if (!value.is_number()) {
    throw Unsupported{"expr abs(" + value.str() + ")"};
  }
}

void check_coverage(Engine& e, ParameterDict& parameters, const Locals& v, const Coverage& c) {
  const Val& value = v[c.key];
  require_abs(value);
  if (gt(abs(value), "100")) {
    e.puts(true, std::string{c.name} + " coverage can't be more than 100%");
    parameters.set(c.key, "100");
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

Val incr(const Val& v, std::int64_t by) {
  const tcl::Number n = v.num();
  if (!n.integer) {
    throw TclError{"expected integer but got \"" + v.str() + "\""};
  }
  return Val::integer(n.i + by);
}

// The tap-spacing estimate loop (sky130A-fet.tcl:1838-1864); a loop that
// never ends in Tcl (both extents over 30 um with m = nf = 1) is refused.
void check_extent(Engine& e, ParameterDict& parameters, Locals& v) {
  const Val clearance = "1.0";
  const Val origm = v["m"];
  const Val orignf = v["nf"];
  std::set<std::string> seen;
  while (true) {
    const std::string state = v["w"].str() + " " + v["l"].str() + " " + v["m"].str() + " " + v["nf"].str();
    if (!seen.insert(state).second) {
      throw Unsupported{"mos_check does not terminate"};
    }
    const Val yext = expr_text("(" + v["w"].str() + " + " + clearance.str() + ") * " + v["m"].str() + " + " +
                               clearance.str());
    const Val xext = expr_text("(" + v["l"].str() + " + " + clearance.str() + ") * " + v["nf"].str() + " + " +
                               clearance.str());
    if (!gt(min(xext, yext), "30.0")) {
      break;
    }
    if (gt(yext, "30.0") && gt(v["m"], "1")) {
      v.set("m", incr(v["m"], -1));
    } else if (gt(xext, "30.0") && gt(v["nf"], "1")) {
      v.set("nf", incr(v["nf"], -1));
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
  if (ne(v["nf"], orignf)) {
    e.puts(true, "Warning: Fingers may need to be reduced to prevent tap distance violation");
  }
}

void check_counts(Engine& e, ParameterDict& parameters, Locals& v) {
  if (!string_is_int(v["nf"])) {
    e.puts(true, "NF must be an integer!");
    parameters.set("nf", "1");
  }
  if (!string_is_int(v["m"])) {
    e.puts(true, "M must be an integer!");
    parameters.set("m", "1");
  }
  // "diffcov must be numeric!" / "polycov must be numeric!" are left to Tcl.
  require_abs(v["diffcov"]);
  require_abs(v["polycov"]);
}

void check_bounds(Engine& e, ParameterDict& parameters, const Locals& v) {
  if (lt(v["l"], v["lmin"])) {
    e.puts(true, "Mos length must be >= " + v["lmin"].str() + " um");
    parameters.set("l", v["lmin"].str());
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

ParameterDict mos_check(Engine& e, const std::string& device, ParameterDict parameters) {
  Locals v;
  v.load(Dict::from(parameters));
  // Normalize distance units to microns
  v.set("l", e.three_digit_past_decimal(e.spice2float(v["l"])));
  v.set("w", e.three_digit_past_decimal(e.spice2float(v["w"])));
  check_counts(e, parameters, v);
  check_bounds(e, parameters, v);
  static const Coverage kCoverages[] = {
      {"viasrc", "Source via"},        {"viadrn", "Drain via"},         {"viagate", "Gate via"},
      {"viagb", "Guard ring bottom via"}, {"viagt", "Guard ring top via"}, {"viagr", "Guard ring right via"},
      {"viagl", "Guard ring left via"}};
  for (const auto& c : kCoverages) {
    check_coverage(e, parameters, v, c);
  }
  check_extent(e, parameters, v);
  // Used by varactor only
  e.set_variable("magic::minfo_val",
                 device == "sky130_fd_pr__cap_var" ? "Warning:  No model exists for this device!" : "");
  return parameters;
}

// ---- the generators ----------------------------------------------------------------

const Dict& ruleset() {
  // sky130A.tcl:51-72
  static const Dict kRules = {
      {"poly_surround", "0.08"},  {"diff_surround", "0.06"},   {"gate_to_diffcont", "0.145"},
      {"gate_to_polycont", "0.275"}, {"gate_extension", "0.13"}, {"diff_extension", "0.29"},
      {"contact_size", "0.17"},   {"via_size", "0.17"},        {"metal_surround", "0.08"},
      {"sub_surround", "0.18"},   {"diff_spacing", "0.28"},    {"poly_spacing", "0.21"},
      {"diff_poly_space", "0.075"}, {"diff_gate_space", "0.20"}, {"metal_spacing", "0.23"},
      {"mmetal_spacing", "0.14"}, {"res_to_cont", "0.20"},     {"res_diff_spacing", "0.20"}};
  return kRules;
}

const std::set<std::string, std::less<>>& text_keys() {
  static const std::set<std::string, std::less<>> kKeys = {
      "class",        "compatible",        "gate_type",        "diff_type",     "diff_contact_type",
      "plus_diff_type", "plus_contact_type", "poly_type",      "poly_contact_type", "sub_type",
      "dev_sub_type", "id_type",           "id2_type",         "guard_sub_type", "drain_proc",
      "drain",        "source",            "gate",             "bulk",          "set_x_to_guard",
      "set_y_to_guard", "gencell"};
  return kKeys;
}

// The parameters the port reproduces: numbers everywhere a number is used,
// a known drain procedure, a bounded array.
void require_drawable(const Dict& drawdict) {
  require_numbers(drawdict, text_keys());
  for (const char* key : {"set_x_to_guard", "set_y_to_guard"}) {
    if (drawdict.has(key) && !drawdict.get(key).empty() && !drawdict.get(key).is_number()) {
      throw Unsupported{std::string{key} + " is not a number"};
    }
  }
  if (drawdict.has("drain_proc")) {
    const std::string& proc = drawdict.get("drain_proc").str();
    if (!proc.empty() && proc != "sky130::draw_ldnmos_drain" && proc != "sky130::draw_ldpmos_drain") {
      throw Unsupported{"drain procedure " + proc};
    }
  }
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
}

struct MosRow {
  const char* type;
  const char* check_device;
  const char* defaults;
  std::vector<std::pair<std::string, Val>> newdict;
};

class Sky130Mos final : public NativeGenerator {
 public:
  explicit Sky130Mos(const MosRow& row) : row_(row) {
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
      Engine e{buffered, HelperFamily::sky130};
      return mos_check(e, row_.check_device, parameters);
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
      Engine e{host, HelperFamily::sky130};
      Sky130 pdk{e};
      return cmd::CommandResult::success(mos_draw(pdk, drawdict));
    });
  }

 private:
  const MosRow& row_;
  Dict newdict_;
};

const std::vector<MosRow>& rows() {
  // clang-format off
  static const std::vector<MosRow> kRows = {
#include "pcell/native/sky130/mos_table.inc"
  };
  // clang-format on
  return kRows;
}

}  // namespace

void add_sky130_mosfets(NativeRegistry& registry, const script::PdkIdentity& pdk) {
  for (const auto& row : rows()) {
    registry.add(pdk, std::string{"sky130::"} + row.type, std::make_unique<Sky130Mos>(row));
  }
}

}  // namespace hefesto::pcell::native
