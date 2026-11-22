// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extract-tech-section, the rules load_styles() parses into ExtStyle
// (openspec/changes/m7-extraction tasks.md 4.2-4.5): resistance classes,
// contact residues, devices and ExtGetDevInfo, substrate, antenna keywords,
// non-electrical types, scale keywords, connectivity and capacitance rules.
// Owner: EXTTECH.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <optional>
#include <string>

#include "ext_style_fixture.hpp"

using namespace hefesto::test::ext;  // NOLINT(google-build-using-namespace): test fixture helpers

namespace {

// A copy: callers pass temporaries.
extract::DevicePattern only_record(const extract::ExtStyle& s, tech::TileType type) {
  const auto records = s.records_for(type);
  REQUIRE(records.size() == 1);
  return *records.front();
}

const extract::AntennaRule* antenna_of(const extract::ExtStyle& s, tech::TileType type) {
  for (const auto& r : s.antenna) {
    if (r.type == type) {
      return &r;
    }
  }
  return nullptr;
}

}  // namespace

// --- Resistance classes and contact resistance ------------------------------

TEST_CASE("Class numbering: each resist line is a new class, a later line overrides a type's class",
          "[cap:extract-tech-section][req:Resistance classes and contact resistance]") {
  const Loaded l = load(" style s\n resist poly 7000\n resist metal1,via 80 2.5\n resist poly 6000\n");
  CHECK(line_messages(l.messages).empty());
  const extract::ExtStyle s = parsed(l);
  const auto& t = *l.tech;
  REQUIRE(s.resist_classes.size() == 3);
  CHECK(s.resist_classes[0].sheet_resistance == 7000);
  CHECK(s.resist_classes[1].sheet_resistance == 80);
  CHECK(s.resist_classes[2].sheet_resistance == 6000);
  CHECK(s.resist_classes[1].types == types_of(t, "metal1,via"));
  CHECK(s.resist_class[type_of(t, "poly")] == 2);
  CHECK(s.resist_class[type_of(t, "via")] == 1);
  CHECK(s.resist_class[type_of(t, "metal2")] == -1);
  CHECK(s.sheet_resistance[type_of(t, "poly")] == 6000);
  CHECK(s.corner_chop[type_of(t, "metal1")] == 2.5F);
  CHECK(s.corner_chop[type_of(t, "poly")] == 1.0F);
}

TEST_CASE("Non-electrical marker via None: resist None removes the types from the active set",
          "[cap:extract-tech-section][req:Resistance classes and contact resistance]") {
  const Loaded l = load(" style s\n resist metal3 None\n");
  const extract::ExtStyle s = parsed(l);
  CHECK_FALSE(s.active.test(type_of(*l.tech, "metal3")));
  CHECK(s.active.test(type_of(*l.tech, "metal2")));
  CHECK(s.resist_classes.empty());
}

TEST_CASE("a decimal resist value makes no class with magic-compat ON (Magic leaves the line)",
          "[cap:extract-tech-section][req:Resistance classes and contact resistance]") {
  const Loaded l = load(" style s\n resist poly 12.5\n resist metal1 12.0\n resist metal2 x\n", true);
  const auto messages = line_messages(l.messages);
  REQUIRE(messages.size() == 2);
  CHECK(messages[0].code == "EXTRACT-STYLE-014");
  CHECK(messages[0].text == "Resist argument truncated to integer value.");
  CHECK(messages[1].text == "Resist argument must be integer or \"None\".");
  CHECK(parsed(l).resist_classes.empty());
}

TEST_CASE("a decimal resist value is truncated into a class with magic-compat OFF",
          "[cap:extract-tech-section][req:Resistance classes and contact resistance]") {
  const Loaded l = load(" style s\n resist poly 12.5\n resist metal1 12.0\n", false);
  const extract::ExtStyle s = parsed(l);
  REQUIRE(s.resist_classes.size() == 2);
  CHECK(s.resist_classes[0].sheet_resistance == 12);
  CHECK(s.resist_classes[1].sheet_resistance == 12);
  CHECK(s.resist_class[type_of(*l.tech, "poly")] == 0);
}

TEST_CASE("contact sets the via resistance; sizes are ignored with a note, a non-integer is an error",
          "[cap:extract-tech-section][req:Resistance classes and contact resistance]") {
  const Loaded l =
      load(" style s\n resist metal1,via 80\n contact via 4 1500\n contact ndc 4 1 1 2000\n contact ndc 1.5\n");
  const auto messages = line_messages(l.messages);
  REQUIRE(messages.size() == 3);
  CHECK(messages[0].code == "EXTRACT-STYLE-017");
  CHECK(messages[0].text == "Contact size value ignored (using GDS generation rules).");
  CHECK(messages[1].text == "Contact size, spacing, and border values ignored (using GDS generation rules).");
  CHECK(messages[2].code == "EXTRACT-STYLE-007");
  CHECK(messages[2].text == "Contact resistivity 1.5 must be an integer value (in milliohms/square).");
  REQUIRE(messages[2].related.has_value());  // a TechError: Magic's file, line and section prefix
  CHECK(messages[2].related->name == "extract");
  const extract::ExtStyle s = parsed(l);
  CHECK(s.via_resistance[type_of(*l.tech, "via")] == 1500);
  CHECK(s.via_resistance[type_of(*l.tech, "ndc")] == 2000);
  CHECK(s.resist_classes[0].contact_resistance == 1500);
}

TEST_CASE("devresist stores (int) value in the records that exist when it is read",
          "[cap:extract-tech-section][req:Resistance classes and contact resistance]") {
  const Loaded l = load(" style s\n device mosfet nfet nfet,pfet ndiff pwell\n devresist nfet linear 12.5\n"
                        " fetresist nfet linear 20\n device mosfet nfet2 nfet pdiff pwell\n devresist pfet area x\n");
  const auto messages = line_messages(l.messages);
  REQUIRE(messages.size() == 1);
  CHECK(messages[0].text == "Device resistivity x must be numeric");
  const extract::ExtStyle s = parsed(l);
  const auto nfet = s.records_for(type_of(*l.tech, "nfet"));
  REQUIRE(nfet.size() == 2);
  CHECK(nfet[0]->model == "nfet2");
  CHECK(nfet[0]->resist.empty());
  REQUIRE(nfet[1]->resist.size() == 1);
  CHECK(nfet[1]->resist[0] == extract::DeviceResist{type_of(*l.tech, "nfet"), "linear", 20});
}

// --- Contact residues and coupling table completion -------------------------

TEST_CASE("Via counted as metal for overlap: a contact with residues on two planes",
          "[cap:extract-tech-section][req:Contact residues and coupling table completion]") {
  const Loaded l = load(" style s\n" + kOrdered +
                        " overlap metal2 metal1 20\n overlap metal3 metal1 10 metal2\n"
                        " sidewall metal1 space space metal1 40\n sideoverlap metal3 space metal1 5\n");
  CHECK(line_messages(l.messages).empty());
  const extract::ExtStyle s = parsed(l);
  const auto& t = *l.tech;
  const extract::CapTables& c = s.caps.tables;
  const tech::TileType via = type_of(t, "via");
  const tech::TileType m1 = type_of(t, "metal1");
  const tech::TileType m2 = type_of(t, "metal2");
  const tech::TileType m3 = type_of(t, "metal3");
  // via is an overlap target wherever its residue metal1 is.
  CHECK(c.overlap_other_types[m2].test(via));
  CHECK(c.overlap_other_types[m3].test(via));
  // via is on both of its planes for overlap and side searches.
  CHECK(c.overlap_types[plane_of(t, "metal1")].test(via));
  CHECK(c.overlap_types[plane_of(t, "metal2")].test(via));
  CHECK(c.side_types[plane_of(t, "metal2")].test(via));
  // It takes its residue's side edges and sideoverlap targets.
  CHECK(c.side_edges[via].test(tech::kSpace));
  CHECK(c.side_overlap_other_types.at({m3, tech::kSpace}).test(via));
  // metal2 shields metal3 from metal1; via has metal1 (t) as a residue, so
  // it is not added as a shield of that pair.
  CHECK(c.overlap_shield_types.at({m3, m1}).test(m2));
  CHECK_FALSE(c.overlap_shield_types.at({m3, m1}).test(via));
  // A shield is an overlap target of the shielded type.
  CHECK(c.overlap_other_types[m3].test(m2));
  CHECK((c.overlap_other_planes[m3] & tech::plane_bit(plane_of(t, "metal2"))) != 0);
  // No parasitic value is stored for the contact itself.
  CHECK(c.overlap.find({m3, via}) == c.overlap.end());
}

TEST_CASE("coupling sets lose the inactive types; resistive connectivity follows the classes",
          "[cap:extract-tech-section][req:Contact residues and coupling table completion]") {
  const Loaded l = load(" style s\n" + kOrdered +
                        " resist metal1 80\n resist via 80\n resist metal3 None\n overlap metal3 metal1 10\n"
                        " overlap metal2 metal1 20\n");
  const extract::ExtStyle s = parsed(l);
  const auto& t = *l.tech;
  CHECK_FALSE(s.caps.tables.overlap_types[plane_of(t, "metal3")].test(type_of(t, "metal3")));
  CHECK(s.caps.tables.overlap_types[plane_of(t, "metal2")].test(type_of(t, "metal2")));
  // metal1 and via connect but sit in different classes.
  CHECK(s.node_conn[type_of(t, "metal1")].test(type_of(t, "via")));
  CHECK_FALSE(s.resist_conn[type_of(t, "metal1")].test(type_of(t, "via")));
  CHECK(s.resist_conn[type_of(t, "metal1")].test(type_of(t, "metal1")));
}

// --- Connectivity overrides --------------------------------------------------

TEST_CASE("Disconnect a well tap: connect and disconnect edit the style's node connectivity",
          "[cap:extract-tech-section][req:Connectivity overrides]") {
  const Loaded l = load(" style s\n disconnect psd pwell\n connect metal3 metal2\n device mosfet nfet nfet ndiff pwell\n");
  const extract::ExtStyle s = parsed(l);
  const auto& t = *l.tech;
  CHECK_FALSE(s.node_conn[type_of(t, "psd")].test(type_of(t, "pwell")));
  CHECK_FALSE(s.node_conn[type_of(t, "pwell")].test(type_of(t, "psd")));
  CHECK(s.node_conn[type_of(t, "metal3")].test(type_of(t, "metal2")));
  CHECK(s.node_conn[type_of(t, "metal2")].test(type_of(t, "metal3")));
  CHECK(s.node_conn[type_of(t, "metal1")].test(type_of(t, "via")));  // from the connect section
  CHECK(s.device_conn[type_of(t, "nfet")] == types_of(t, "nfet"));
  CHECK(s.device_conn[type_of(t, "ndiff")].empty());
}

// --- Device declaration parsing ---------------------------------------------

TEST_CASE("Parameter and range parsing with magic-compat ON: Magic's integer limits",
          "[cap:extract-tech-section][req:Device declaration parsing]") {
  const std::string body = " style s\n" + kOrdered +
                           " device msubcircuit nfet_01v8 nfet ndiff,ndc ndiff,ndc pwell,space VSUBS l>=0.15"
                           " w<20 l=l w=w\n";
  const Loaded l = load(body, true);
  CHECK(line_messages(l.messages).empty());
  const auto& t = *l.tech;
  const extract::DevicePattern& d = only_record(parsed(l), type_of(t, "nfet"));
  CHECK(d.device_class == extract::DeviceClass::msubckt);
  REQUIRE(d.terminals.size() == 1);  // identical masks collapse to symmetric
  CHECK(d.terminals[0] == types_of(t, "ndiff,ndc"));
  CHECK(d.terminal_count == 2);
  CHECK(d.substrate_types == types_of(t, "pwell,space"));
  CHECK(d.substrate_node == "VSUBS");
  REQUIRE(d.params.size() == 2);
  CHECK(d.params[0].code == "l");
  CHECK(d.params[0].name == "l");
  CHECK(d.params[0].minimum == 150);
  CHECK(d.params[0].maximum == -1);
  CHECK(d.params[1].code == "w");
  CHECK(d.params[1].maximum == 19999);
  // After the conversion: `w<20` is 19 (integer division); `l>=0.15` alone
  // has min > max, which Magic treats as no range and never divides.
  const extract::DevicePattern& c = only_record(finished(l), type_of(t, "nfet"));
  CHECK(c.params[1].maximum == 19);
  CHECK(c.params[1].minimum == 0);
  CHECK(c.params[0].minimum == 150);
  CHECK(c.params[0].maximum == -1);
}

TEST_CASE("Parameter and range parsing with magic-compat OFF: limits stay in milli-units",
          "[cap:extract-tech-section][req:Device declaration parsing]") {
  const std::string body = " style s\n" + kOrdered +
                           " device msubcircuit nfet_01v8 nfet ndiff ndiff pwell VSUBS l>=0.15 w<20.5 l=l w=w\n";
  const Loaded l = load(body, false);
  const extract::DevicePattern& c = only_record(finished(l), type_of(*l.tech, "nfet"));
  CHECK(c.params[1].maximum == 20499);
  CHECK(c.params[0].minimum == 150);
}

TEST_CASE("Diode substrate types come from OTHER in both modes (Q-EXTRACT-2)",
          "[cap:extract-tech-section][req:Device declaration parsing]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << on);
    const Loaded l = load(" style s\n device pdiode photodiode pdiff nwell VPB\n", on);
    const auto& t = *l.tech;
    const extract::DevicePattern& d = only_record(parsed(l), type_of(t, "pdiff"));
    CHECK(d.device_class == extract::DeviceClass::pdiode);
    REQUIRE(d.terminals.size() == 1);
    CHECK(d.terminals[0] == types_of(t, "nwell"));
    CHECK(d.substrate_types == types_of(t, "nwell"));
    CHECK(d.substrate_node == "VPB");
    CHECK(d.terminal_count == 1);
  }
}

TEST_CASE("Capacitor values: a numeric last argument is the area cap, the one before the perimeter cap",
          "[cap:extract-tech-section][req:Device declaration parsing]") {
  const Loaded l = load(" style s\n device capacitor None metal3 metal2 10 2\n");
  const auto& t = *l.tech;
  const extract::DevicePattern& d = only_record(parsed(l), type_of(t, "metal3"));
  CHECK(d.device_class == extract::DeviceClass::devcap);
  CHECK(d.model == "None");
  REQUIRE(d.terminals.size() == 1);
  CHECK(d.terminals[0] == types_of(t, "metal2"));
  CHECK(d.substrate_types.empty());
  CHECK(d.substrate_node.empty());
  CHECK(d.perimeter_cap == 10.0);
  CHECK(d.area_cap == 2.0);
}

TEST_CASE("asymmetric mosfets, identifier layers, parameter scale and offset, subcircuit terminals",
          "[cap:extract-tech-section][req:Device declaration parsing]") {
  const Loaded l = load(" style s\n"
                        " device mosfet hv nfet ndiff ndiff,ndc pwell VSUBS +metal3 a=area*2 p=perim+0.5\n"
                        " device mosfet lv pfet pdiff nwell VDD 3 4\n"
                        " device subcircuit sub poly 3 ndiff pdiff\n"
                        " device rsubcircuit rs psd ndiff pwell VSUBS l=l\n");
  const auto& t = *l.tech;
  const auto messages = line_messages(l.messages);
  REQUIRE(messages.size() == 2);
  CHECK(messages[0].text == "Device mosfet hv has overlapping drain and source types!");
  CHECK(messages[1].text == "Not enough terminals for subcircuit, 3 were required, 2 found.");
  const extract::ExtStyle s = parsed(l);
  const extract::DevicePattern& hv = only_record(s, type_of(t, "nfet"));
  CHECK(hv.device_class == extract::DeviceClass::asymmetric);
  REQUIRE(hv.terminals.size() == 2);
  CHECK(hv.identifiers == types_of(t, "metal3"));
  REQUIRE(hv.params.size() == 2);
  CHECK(hv.params[0].code == "a");
  CHECK(hv.params[0].name == "area");
  CHECK(hv.params[0].scale == 2.0);
  CHECK(hv.params[1].name == "perim");
  CHECK(hv.params[1].offset == 500);
  const extract::DevicePattern& lv = only_record(s, type_of(t, "pfet"));
  CHECK(lv.device_class == extract::DeviceClass::mosfet);  // numeric argument 7: symmetric
  CHECK(lv.terminals.size() == 1);
  CHECK(lv.perimeter_cap == 3.0);
  CHECK(lv.area_cap == 4.0);
  const extract::DevicePattern& sub = only_record(s, type_of(t, "poly"));
  CHECK(sub.terminal_count == 2);
  CHECK(sub.terminals.size() == 2);
  const extract::DevicePattern& rs = only_record(s, type_of(t, "psd"));
  CHECK(rs.terminal_count == 2);
  CHECK(rs.substrate_types == types_of(t, "pwell"));
  CHECK(rs.substrate_node == "VSUBS");
}

TEST_CASE("the legacy fet line: substrate types when argument 6 is not a number; global substrate names",
          "[cap:extract-tech-section][req:Device declaration parsing]") {
  const Loaded l = load(" style s\n fet nfet ndiff 2 nfet GND! pwell 100 5\n fet pfet pdiff 2 pfet Vdd 7 0\n"
                        " fet poly ndiff 2 pp ERROR 1 0\n");
  const auto messages = line_messages(l.messages);
  REQUIRE(messages.size() == 1);
  CHECK(messages[0].text == "Fet substrate node Vdd is not a global name");
  const extract::ExtStyle s = parsed(l);
  const auto& t = *l.tech;
  const extract::DevicePattern& n = only_record(s, type_of(t, "nfet"));
  CHECK(n.device_class == extract::DeviceClass::fet);
  CHECK(n.substrate_types == types_of(t, "pwell"));
  CHECK(n.perimeter_cap == 100.0);
  CHECK(n.area_cap == 5.0);
  CHECK(n.terminal_count == 2);
  CHECK(only_record(s, type_of(t, "pfet")).perimeter_cap == 7.0);
  CHECK(s.gate_types() == types_of(t, "nfet,pfet,poly"));
}

TEST_CASE("device_info follows ExtGetDevInfo: type order, last line first, unique names, classes",
          "[cap:extract-tech-section][req:Device declaration parsing]") {
  const Loaded l = load(" style s\n resist ndiff 100\n resist pdiff 200\n resist pwell 300\n"
                        " device mosfet n1 nfet ndiff pwell $SUB\n"
                        " device mosfet n2 nfet,pfet pdiff nwell VDD!\n"
                        " device mosfet asym pfet ndiff pdiff pwell GND!\n"
                        " device mosfet n2 poly metal1 None\n");
  const extract::ExtStyle s = parsed(l);
  const auto& t = *l.tech;
  const auto info = s.device_info();
  // Types in number order (poly < nfet < pfet here), each type's records
  // last line first, the first record of each name: poly's n2, nfet's n1
  // (its n2 is a repeat), pfet's asym (its n2 is a repeat).
  REQUIRE(info.size() == 3);
  CHECK(info[0].model == "n2");
  CHECK(info[0].type == type_of(t, "poly"));
  CHECK(info[0].source_class == -1);
  CHECK(info[0].substrate_node.empty());
  CHECK(info[1].model == "n1");
  CHECK(info[1].type == type_of(t, "nfet"));
  CHECK(info[1].source_class == 0);
  CHECK(info[1].drain_class == 0);  // one terminal mask: the source class
  CHECK(info[1].substrate_class == 2);
  CHECK(info[1].substrate_node == "$SUB");
  CHECK(info[2].model == "asym");
  CHECK(info[2].type == type_of(t, "pfet"));
  CHECK(info[2].source_class == 0);
  CHECK(info[2].drain_class == 1);
  CHECK(info[2].substrate_class == 2);
  CHECK(info[2].substrate_node == "GND!");
}

TEST_CASE("device_info drain class without a source class: Magic's loop index ON, -1 OFF",
          "[cap:extract-tech-section][req:Device declaration parsing]") {
  const std::string body = " style s\n resist ndiff 100\n resist pdiff 200\n device mosfet m nfet metal1 pwell X\n";
  const extract::ExtStyle on_style = parsed(load(body, true));
  const auto on = on_style.device_info();
  REQUIRE(on.size() == 1);
  CHECK(on[0].source_class == -1);
  CHECK(on[0].drain_class == 2);  // the class count
  const extract::ExtStyle off_style = parsed(load(body, false));
  const auto off = off_style.device_info();
  CHECK(off[0].drain_class == -1);
}

TEST_CASE("substitute_substrate_node replaces a known $ variable and leaves the rest",
          "[cap:extract-tech-section][req:Device declaration parsing]") {
  const auto lookup = [](std::string_view name) -> std::optional<std::string> {
    if (name == "SUB") {
      return std::string{"VSUBS"};
    }
    return std::nullopt;
  };
  CHECK(extract::substitute_substrate_node("$SUB", lookup) == "VSUBS");
  CHECK(extract::substitute_substrate_node("$NONE", lookup) == "$NONE");
  CHECK(extract::substitute_substrate_node("GND!", lookup) == "GND!");
}

// --- Substrate declaration ---------------------------------------------------

TEST_CASE("Default substrate type: the lowest substrate type on the substrate plane",
          "[cap:extract-tech-section][req:Substrate declaration]") {
  const Loaded l = load(" style s\n substrate *psd,space,pwell well $SUB -nwell\n");
  const extract::ExtStyle s = parsed(l);
  const auto& t = *l.tech;
  CHECK(s.substrate.types == types_of(t, "psd,space,pwell"));
  REQUIRE(s.substrate.plane.has_value());
  CHECK(*s.substrate.plane == plane_of(t, "well"));
  REQUIRE(s.substrate.default_type.has_value());
  CHECK(*s.substrate.default_type == type_of(t, "pwell"));
  CHECK(s.substrate.node_name == "$SUB");
  CHECK(s.substrate.shield_types == types_of(t, "nwell"));
  // psd is off the substrate plane and not a shield: a substrate device type.
  CHECK(s.substrate_device_types.test(type_of(t, "psd")));
}

TEST_CASE("an unknown substrate plane is reported and leaves no substrate plane",
          "[cap:extract-tech-section][req:Substrate declaration]") {
  const Loaded l = load(" style s\n substrate pwell nosuch\n");
  const auto messages = line_messages(l.messages);
  REQUIRE(messages.size() == 1);
  CHECK(messages[0].code == "EXTRACT-STYLE-019");
  CHECK(messages[0].text == "Unrecognized plane name \"nosuch\"");
  CHECK_FALSE(parsed(l).substrate.plane.has_value());
}

// --- Antenna keywords parse-level reference ---------------------------------

TEST_CASE("Antenna line accepted: model flags, gate ratio and an infinite diffusion ratio",
          "[cap:extract-tech-section][req:Antenna keywords parse-level reference]") {
  const Loaded l = load(" style s\n model partial surface\n antenna metal1 surface 400 none\n"
                        " antenna metal2 sidewall 50 2.5 3\n tiedown ndiff,psd\n");
  CHECK(line_messages(l.messages).empty());
  const extract::ExtStyle s = parsed(l);
  const auto& t = *l.tech;
  CHECK(s.antenna_model == (extract::kAntennaPartial | extract::kAntennaSurface));
  const extract::AntennaRule* m1 = antenna_of(s, type_of(t, "metal1"));
  REQUIRE(m1 != nullptr);
  CHECK(m1->model == extract::AntennaModel::surface);
  CHECK(m1->ratio_gate == 400.0);
  CHECK(std::isinf(m1->ratio_diff[0]));
  CHECK(m1->ratio_diff[1] == 0.0);
  const extract::AntennaRule* m2 = antenna_of(s, type_of(t, "metal2"));
  REQUIRE(m2 != nullptr);
  CHECK(m2->model == extract::AntennaModel::sidewall);
  CHECK(m2->ratio_diff[0] == 2.5);
  CHECK(m2->ratio_diff[1] == 3.0);
  CHECK(s.tiedown == types_of(t, "ndiff,psd"));
  CHECK(s.diff_types() == s.tiedown);
}

TEST_CASE("Q-MISC-2: the antenna model flags start empty for every style and on reload",
          "[cap:extract-tech-section][req:Antenna keywords parse-level reference]") {
  // Style a sets the surface default; style b has no model line, so its
  // antenna line has no calculation type (Magic's uninitialised flags
  // could carry a's).
  const Loaded l = load(" style a\n model partial surface\n style b\n antenna metal1 400 none\n");
  const auto b = l.table.select("b");
  REQUIRE(b != nullptr);
  CHECK(b->antenna_model == 0);
  const extract::AntennaRule* m1 = antenna_of(*b, type_of(*l.tech, "metal1"));
  REQUIRE(m1 != nullptr);
  CHECK(m1->model == extract::AntennaModel::none);
  // Reloading a after b gives a's flags only.
  CHECK(l.table.select("a")->antenna_model == (extract::kAntennaPartial | extract::kAntennaSurface));
  CHECK(l.table.select("b")->antenna_model == 0);
}

TEST_CASE("Q-MISC-3: an antenna line without a calculation type and no default stores none",
          "[cap:extract-tech-section][req:Antenna keywords parse-level reference]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << on);
    const Loaded l = load(" style s\n antenna metal1 400 none\n", on);
    const auto messages = line_messages(l.messages);
    REQUIRE(messages.size() == 1);
    CHECK(messages[0].code == "EXTRACT-STYLE-012");
    CHECK(messages[0].text ==
          "No antenna calculation type given for layer(s) metal1  and no default calculation type found.");
    const extract::ExtStyle style = parsed(l);
    const extract::AntennaRule* m1 = antenna_of(style, type_of(*l.tech, "metal1"));
    REQUIRE(m1 != nullptr);
    CHECK(m1->model == extract::AntennaModel::none);
    CHECK(m1->ratio_gate == 400.0);  // the ratios are still parsed
  }
}

TEST_CASE("Q-MISC-5: an antenna line with a calculation type and one ratio is malformed and ignored",
          "[cap:extract-tech-section][req:Antenna keywords parse-level reference]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << on);
    const Loaded l = load(" style s\n antenna metal1 surface 400\n", on);
    const auto messages = line_messages(l.messages);
    REQUIRE(messages.size() == 1);
    CHECK(messages[0].code == "EXTRACT-STYLE-001");
    CHECK(messages[0].text.starts_with("Malformed line for keyword antenna.  Correct usage:"));
    const extract::ExtStyle style = parsed(l);
    CHECK(antenna_of(style, type_of(*l.tech, "metal1")) == nullptr);
  }
}

TEST_CASE("antenna and model errors: unknown calculation type, non-numeric ratios, unknown model",
          "[cap:extract-tech-section][req:Antenna keywords parse-level reference]") {
  const Loaded l = load(" style s\n model full\n model partial edge\n antenna metal1 volume 1 2\n"
                        " antenna metal2 surface x 2\n antenna metal3 surface 1 y\n");
  const auto messages = line_messages(l.messages);
  REQUIRE(messages.size() == 5);
  CHECK(messages[0].text == "Unknown antenna model \"full\":  Use \"partial\" or \"cumulative\"");
  CHECK(messages[1].text == "Unknown antenna model \"edge\":  Use \"surface\" or \"sidewall\"");
  CHECK(messages[2].text == "Error in layer antenna calculation type \"volume\";  must be \"surface\" or \"sidewall\"");
  CHECK(messages[3].text == "Gate layer antenna ratio x must be numeric");
  CHECK(messages[4].text == "Diff layer antenna ratio y must be numeric");
}

// --- Non-electrical types ----------------------------------------------------

TEST_CASE("Unmentioned marker layer: listed once as non-electrical and not active",
          "[cap:extract-tech-section][req:Non-electrical types]") {
  const Loaded l = load(" style s\n" + kOrdered + " areacap metal1,metal2,metal3 1\n");
  REQUIRE(l.messages.size() == 1);
  CHECK(l.messages[0].code == "EXTRACT-STYLE-018");
  CHECK(l.messages[0].text ==
        "The following types are not handled by extraction and will be treated as non-electrical types:\n"
        "    nwell pdiff comment ");
  const extract::ExtStyle s = parsed(l);
  CHECK_FALSE(s.active.test(type_of(*l.tech, "comment")));
  CHECK(s.non_electrical == types_of(*l.tech, "nwell,pdiff,comment"));
  CHECK(s.active.test(type_of(*l.tech, "via")));  // connected
}

TEST_CASE("with magic-compat ON the mentions of earlier selections accumulate, as allExtractTypes does",
          "[cap:extract-tech-section][req:Non-electrical types]") {
  const std::string body = " style a\n" + kOrdered + " areacap comment 1\n style b\n" + kOrdered + " areacap metal1 1\n";
  const Loaded on = load(body, true);
  const auto b_on = on.table.select("b");
  CHECK(b_on->active.test(type_of(*on.tech, "comment")));  // mentioned by a, read at tech load
  const Loaded off = load(body, false);
  CHECK_FALSE(off.table.select("b")->active.test(type_of(*off.tech, "comment")));
  diag::CollectingSink sink;
  (void)off.table.select("b", sink);
  const auto replayed = sink.take();
  REQUIRE(replayed.size() == 1);
  INFO(replayed[0].text);
  CHECK(replayed[0].text.ends_with("nwell pdiff metal3 comment "));
}

// --- Units, lambda, cscale and rscale; Style defaults ------------------------

TEST_CASE("Unparseable cscale: reported and the capacitance scale is 1; rscale is atoi",
          "[cap:extract-tech-section][req:Units, lambda, cscale and rscale]") {
  const Loaded l = load(" style s\n cscale abc\n rscale 7x\n");
  const auto messages = line_messages(l.messages);
  REQUIRE(messages.size() == 1);
  CHECK(messages[0].text == "Cannot parse cap scale value \"abc\"");
  const extract::ExtStyle s = parsed(l);
  CHECK(s.cscale == 1.0);
  CHECK(s.rscale == 7.0);
  CHECK(parsed(load(" style s\n cscale 12abc\n lambda 2.5\n")).cscale == 12.0);
  CHECK(parsed(load(" style s\n lambda 2.5\n")).units_per_lambda == 2.5F);
}

TEST_CASE("Empty style: Magic's defaults", "[cap:extract-tech-section][req:Style defaults]") {
  const extract::ExtStyle s = finished(load(" style plain\n" + kOrdered));
  CHECK(s.name == "plain");
  CHECK(s.step_size == 100);
  CHECK(s.side_halo == 0);
  CHECK(s.units_per_lambda == 100.0F);
  CHECK(s.cscale == 1000.0);
  CHECK(s.rscale == 1000.0);
  CHECK(s.resist_classes.empty());
  CHECK(s.devices.empty());
  CHECK_FALSE(s.substrate.plane.has_value());
  CHECK(s.antenna_model == 0);
  CHECK_FALSE(s.units_microns);
}

// --- Step size and side halo parsing ------------------------------------------

TEST_CASE("Fractional halo in lambda: magic-compat ON truncates the milli-units, OFF rounds",
          "[cap:extract-tech-section][req:Step size and side halo parsing]") {
  const std::string body = " style s\n" + kOrdered + " sidehalo 2.5\n step 7.9\n";
  const extract::ExtStyle on = finished(load(body, true));
  CHECK(on.side_halo == 2);
  CHECK(on.step_size == 7);
  const extract::ExtStyle off = finished(load(body, false));
  CHECK(off.side_halo == 3);
  CHECK(off.step_size == 8);
}

TEST_CASE("a step that is not positive is reported", "[cap:extract-tech-section][req:Step size and side halo parsing]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << on);
    const Loaded l = load(" style s\n step 0.5\n step -2\n", on);
    const auto messages = line_messages(l.messages);
    const std::size_t expected = on ? 2 : 1;  // OFF: 0.5 is positive
    REQUIRE(messages.size() == expected);
    CHECK(messages.back().code == "EXTRACT-STYLE-015");
    CHECK(messages.back().text == "Hierarchical interaction step size must be > 0");
  }
}

// --- Plane ordering (the full read's part) ------------------------------------

TEST_CASE("the default* shorthands need a planeorder line first",
          "[cap:extract-tech-section][req:Plane ordering]") {
  const Loaded l = load(" style s\n defaultareacap metal1 metal1 10\n defaultsidewall metal1 metal1 10\n" + kOrdered);
  const auto messages = line_messages(l.messages);
  REQUIRE(messages.size() == 1);
  CHECK(messages[0].code == "EXTRACT-STYLE-009");
  CHECK(messages[0].text == "Cannot parse area cap line without plane ordering!");
  const extract::ExtStyle s = parsed(l);
  CHECK(s.caps.tables.area[type_of(*l.tech, "metal1")] == 0.0);
  CHECK_FALSE(s.caps.tables.side_couple.empty());  // defaultsidewall needs no plane order
}

// --- Extract section line grammar (reported by the full read) -----------------

TEST_CASE("Prefix keyword and too many arguments in the full read",
          "[cap:extract-tech-section][req:Extract section line grammar and keyword matching]") {
  const Loaded l = load(" style s\n are metal1 30\n perimc metal1 space 20 extra\n bogus 1\n");
  const auto messages = line_messages(l.messages);
  REQUIRE(messages.size() == 2);
  CHECK(messages[0].text == "Malformed line for keyword perimc.  Correct usage:\n\tperimc intypes outtypes capacitance");
  CHECK(messages[1].code == "EXTRACT-STYLE-020");
  CHECK(messages[1].text.starts_with("Illegal keyword.  Legal keywords are:\n\t areacap connect contact"));
  const extract::ExtStyle s = parsed(l);
  CHECK(s.caps.tables.area[type_of(*l.tech, "metal1")] == 30.0);
  CHECK(s.caps.tables.perimeter.empty());
}

// --- Area and perimeter capacitance rules -------------------------------------

TEST_CASE("Fringe multiplier from areacap; perimc with a zero value records nothing",
          "[cap:extract-tech-section][req:Area and perimeter capacitance rules]") {
  const Loaded l = load(" style s\n areacap metal1 30\n perimc metal1 space,metal2 20\n perimc metal2 space 0\n");
  const extract::ExtStyle s = parsed(l);
  const auto& t = *l.tech;
  const tech::TileType m1 = type_of(t, "metal1");
  const extract::CapTables& c = s.caps.tables;
  CHECK(c.area[m1] == 30.0);
  CHECK(c.fringe_mult.at({m1, tech::kSpace}) == static_cast<float>(static_cast<double>(30.0F) * 0.02));
  CHECK(c.fringe_mult.at({tech::kSpace, m1}) == c.fringe_mult.at({m1, tech::kSpace}));
  CHECK(c.perimeter.at({m1, tech::kSpace}) == 20.0);
  CHECK(c.perimeter.at({m1, type_of(t, "metal2")}) == 20.0);
  // Magic's final pass masks exts_perimCapMask with the active types, which
  // never include space; the value itself stays.
  CHECK_FALSE(c.perimeter_mask[m1].test(tech::kSpace));
  CHECK(c.perimeter_mask[m1].test(type_of(t, "metal2")));
  CHECK(c.perimeter.find({type_of(t, "metal2"), tech::kSpace}) == c.perimeter.end());
  REQUIRE(s.caps.area.size() == 1);
  CHECK(s.caps.area[0] == extract::TypeCap{m1, {}, 30.0});
  CHECK(s.caps.perimeter.size() == 2);
}

// --- Overlap capacitance rules -------------------------------------------------

TEST_CASE("Duplicate overlap pair: reported and the first value stays",
          "[cap:extract-tech-section][req:Overlap capacitance rules]") {
  const Loaded l = load(" style s\n overlap metal2 metal1 20\n overlap metal2 metal1 25\n overlap metal1 metal1 5\n"
                        " overlap metal1 via 5\n overlap metal3 metal1 3 metal1\n");
  const auto messages = line_messages(l.messages);
  REQUIRE(messages.size() == 3);
  CHECK(messages[0].text == "Only one of \"overlap metal2 metal1\" or \"overlap metal1 metal2\" allowed");
  CHECK(messages[1].text == "Can't have overlap capacitance between tiles of the same type (metal1)");
  CHECK(messages[2].text == "Shielding type (metal1) must be on a different plane from shielded types.");
  const extract::ExtStyle s = parsed(l);
  const auto& t = *l.tech;
  CHECK(s.caps.tables.overlap.at({type_of(t, "metal2"), type_of(t, "metal1")}) == 20.0);
  // The contact (via) is skipped silently; metal3/metal1 keeps its value.
  CHECK(s.caps.tables.overlap.at({type_of(t, "metal3"), type_of(t, "metal1")}) == 3.0);
  CHECK(s.caps.tables.fringe_mult.at({type_of(t, "metal1"), type_of(t, "metal2")}) ==
        static_cast<float>(static_cast<double>(20.0F) * 0.02));
}

// --- Sidewall coupling rules and halving ---------------------------------------

TEST_CASE("Halved coefficient: sidewall rules are halved, single-precision offsets, newest first",
          "[cap:extract-tech-section][req:Sidewall coupling rules and halving]") {
  const Loaded l = load(" style s\n sidewall metal1 space space metal1 40 0.5\n"
                        " sidewall metal1 space space metal1 10\n sidewall space metal1 metal1 space 1\n");
  const auto messages = line_messages(l.messages);
  REQUIRE(messages.size() == 1);
  CHECK(messages[0].text == "Can't have space on inside of edge [ignored]");
  const extract::ExtStyle s = parsed(l);
  const auto& t = *l.tech;
  const tech::TileType m1 = type_of(t, "metal1");
  const auto& rules = s.caps.tables.side_couple.at({m1, tech::kSpace});
  REQUIRE(rules.size() == 2);
  CHECK(rules[0].cap == 5.0);  // the later line first
  CHECK(rules[1].cap == 20.0);
  CHECK(rules[1].offset == 0.5F);
  CHECK(rules[1].near_types == tech::TypeMask::of(tech::kSpace));
  CHECK(rules[1].far_types == types_of(t, "metal1"));
  CHECK(s.caps.tables.side_couple_other_edges.at({m1, tech::kSpace}) == types_of(t, "metal1"));
  CHECK(s.caps.sidewall.size() == 2);
}

// --- Sideoverlap rules -------------------------------------------------------

TEST_CASE("Sideoverlap to substrate space: the /plane suffix names the target plane",
          "[cap:extract-tech-section][req:Sideoverlap rules]") {
  const Loaded l = load(" style s\n sideoverlap metal1 space space/well 15\n sideoverlap metal2 space space 1\n"
                        " sideoverlap metal1 space metal1 2\n");
  const auto messages = line_messages(l.messages);
  REQUIRE(messages.size() == 2);
  CHECK(messages[0].text == "Must specify plane for sideoverlap to space");
  CHECK(messages[1].text ==
        "Overlapped types in \"sideoverlap\" rule must be on a\n\tdifferent plane from intypes and outtypes.");
  const extract::ExtStyle s = parsed(l);
  const auto& t = *l.tech;
  const auto& rules = s.caps.tables.side_overlap.at({type_of(t, "metal1"), tech::kSpace});
  REQUIRE(rules.size() == 1);
  CHECK(rules[0].cap == 15.0);
  CHECK(rules[0].planes == tech::plane_bit(plane_of(t, "well")));
  CHECK(rules[0].near_types.test(tech::kSpace));
}

// --- Default capacitance shorthands -------------------------------------------

TEST_CASE("Default perimeter creates fringe-to-substrate rule",
          "[cap:extract-tech-section][req:Default capacitance shorthands]") {
  const Loaded l = load(" style s\n" + kOrdered + " substrate *psd,space well\n defaultperimeter metal1 metal1 12\n");
  CHECK(line_messages(l.messages).empty());
  const extract::ExtStyle s = parsed(l);
  const auto& t = *l.tech;
  const tech::TileType m1 = type_of(t, "metal1");
  const extract::CapTables& c = s.caps.tables;
  CHECK(c.perimeter.at({m1, tech::kSpace}) == 12.0);
  const auto& rules = c.side_overlap.at({m1, tech::kSpace});
  REQUIRE(rules.size() == 1);
  CHECK(rules[0].cap == 12.0);
  CHECK(rules[0].near_types == types_of(t, "psd"));
  CHECK(rules[0].planes == tech::plane_bit(plane_of(t, "well")));
  tech::TypeMask active_plane = t.plane_types(plane_of(t, "active"));
  active_plane.clear_type(tech::kSpace);
  CHECK(rules[0].far_types == active_plane);
  // The contact on metal1's plane picks the rule up through its residue.
  CHECK(c.side_overlap_other_types.at({m1, tech::kSpace}).test(type_of(t, "psd")));
}

TEST_CASE("defaultareacap with the substrate's default type: the substrate plane shields",
          "[cap:extract-tech-section][req:Default capacitance shorthands]") {
  const Loaded l = load(" style s\n" + kOrdered + " substrate *psd,space,pwell well\n defaultareacap metal1 metal1 7\n"
                        " defaultoverlap metal2 metal2 metal1 metal1 4\n defaultsidewall metal1 metal1 6 0.25\n");
  CHECK(line_messages(l.messages).empty());
  const extract::ExtStyle s = parsed(l);
  const auto& t = *l.tech;
  const tech::TileType m1 = type_of(t, "metal1");
  const tech::TileType psd = type_of(t, "psd");
  const extract::CapTables& c = s.caps.tables;
  CHECK(c.area[m1] == 7.0);
  CHECK(c.overlap.at({m1, psd}) == 7.0);
  CHECK(c.overlap.find({m1, type_of(t, "pwell")}) == c.overlap.end());  // the default type is not substrate
  CHECK((c.overlap_shield_planes.at({m1, psd}) & tech::plane_bit(plane_of(t, "well"))) != 0);
  CHECK(c.overlap_shield_types.at({m1, psd}).test(type_of(t, "nwell")));
  CHECK(c.overlap.at({type_of(t, "metal2"), m1}) == 4.0);
  const auto& side = c.side_couple.at({m1, tech::kSpace});
  REQUIRE(side.size() == 1);
  CHECK(side[0].cap == 3.0);
  CHECK(side[0].offset == 0.25F);
}

TEST_CASE("Sideoverlap shields when declaration order differs from plane order, magic-compat ON",
          "[cap:extract-tech-section][req:Default capacitance shorthands]") {
  // metal1 is declared before metal3 but ordered above it.
  const std::string body = " style s\n planeorder well 0\n planeorder active 1\n planeorder metal3 2\n"
                           " planeorder metal2 3\n planeorder metal1 4\n defaultsideoverlap metal1 metal1 metal3 metal3 5\n";
  const Loaded l = load(body, true);
  const auto& t = *l.tech;
  const extract::ExtStyle s = parsed(l);
  const auto& rules = s.caps.tables.side_overlap.at({type_of(t, "metal1"), tech::kSpace});
  REQUIRE(rules.size() == 1);
  CHECK(rules[0].far_types.empty());  // direction from the plane indices: no plane between
}

TEST_CASE("Sideoverlap shields when declaration order differs from plane order, magic-compat OFF",
          "[cap:extract-tech-section][req:Default capacitance shorthands]") {
  const std::string body = " style s\n planeorder well 0\n planeorder active 1\n planeorder metal3 2\n"
                           " planeorder metal2 3\n planeorder metal1 4\n defaultsideoverlap metal1 metal1 metal3 metal3 5\n";
  const Loaded l = load(body, false);
  const auto& t = *l.tech;
  const extract::ExtStyle s = parsed(l);
  const auto& rules = s.caps.tables.side_overlap.at({type_of(t, "metal1"), tech::kSpace});
  REQUIRE(rules.size() == 1);
  CHECK(rules[0].far_types.test(type_of(t, "metal2")));
  CHECK((s.caps.tables.side_overlap_shield_planes.at({type_of(t, "metal1"), type_of(t, "metal3")}) &
         tech::plane_bit(plane_of(t, "metal2"))) != 0);
}

TEST_CASE("defaultsideoverlap rejects one plane and space", "[cap:extract-tech-section][req:Default capacitance shorthands]") {
  const Loaded l = load(" style s\n" + kOrdered + " defaultsideoverlap metal1 metal1 via metal1 5\n"
                        " defaultsideoverlap metal1 metal1 space metal3 5\n");
  const auto messages = line_messages(l.messages);
  REQUIRE(messages.size() == 2);
  CHECK(messages[0].text == "Cannot have fringing capacitance between types on the same plane");
  CHECK(messages[1].text == "Overlap types can't contain space [ignored]");
}

// --- Fringe model default chosen from tech format ------------------------------

TEST_CASE("Old format: fringe halo off; format 34 and later: on, for every style",
          "[cap:extract-tech-section][req:Fringe model default chosen from tech format]") {
  const std::string body = " style a\n style b\n";
  const Loaded old_format = load_full(load_tech(tech_text(body, "", "")));
  CHECK(parsed(old_format).fringe_model == extract::FringeModel::legacy);
  CHECK(old_format.table.select("b")->fringe_model == extract::FringeModel::legacy);
  const Loaded v34 = load_full(load_tech(tech_text(body, "", " format 34\n")));
  CHECK(parsed(v34).fringe_model == extract::FringeModel::fringe_halo);
  CHECK(v34.table.select("b")->fringe_model == extract::FringeModel::fringe_halo);
}
