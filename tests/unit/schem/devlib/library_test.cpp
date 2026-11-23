// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The system symbol library against its interface table (capability
// schematic-symbols-and-libraries "System library"; design D8 step 3, task
// 7.5). Every file of assets/schematic/xschem_library/devices is read with
// Hefesto's reader and SYMLIB's symbol model and checked against
// tests/data/schem/devices_interface.json: pins in order with their names,
// directions, boxes and other attributes, and the symbol property keys with
// their decoded values. Each file is also a write fixpoint carrying the
// Apache-2.0 notice in its header text, and the library holds exactly the
// inventoried symbols plus netlist_options.sym (Q-SCHEM-10, Q-SCHEM-31).
//
// Owner: DEVLIB.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "devlib_support.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/file/read_write.hpp"
#include "hefesto/schem/sym/symbol.hpp"

namespace schem = hefesto::schem;
namespace dl = hefesto::test::devlib;
using nlohmann::json;

namespace {

constexpr const char* kSystemLibrary = "[cap:schematic-symbols-and-libraries][req:System library]";

/// Keys the library holds besides the table's: the `format` of the two
/// simulator_commands symbols, which the interface notes describe instead.
const std::set<std::string> kNoteFormat{"simulator_commands.sym", "simulator_commands_shown.sym"};

std::set<std::string> library_files() {
  std::set<std::string> names;
  for (const auto& entry : std::filesystem::directory_iterator(dl::kLibrary)) names.insert(entry.path().filename().string());
  return names;
}

schem::Document read_symbol(const std::string& name) {
  schem::ReadOptions options;
  options.kind = schem::FileKind::symbol;
  options.source_name = name;
  return schem::read_document(dl::read_text(dl::kLibrary / name), options).value();
}

std::set<std::string> keys_of(const std::string& props) {
  std::set<std::string> keys;
  for (const auto& property : schem::parse_properties(props)) keys.insert(property.key);
  return keys;
}

void check_properties(const std::string& name, const std::string& props, const json& expected,
                      std::set<std::string> extra_keys) {
  INFO(name << " property string: " << props);
  for (const auto& [key, value] : expected.items()) {
    INFO("key " << key);
    CHECK(schem::get_token(props, key) == value.get<std::string>());
    extra_keys.insert(key);
  }
  CHECK(keys_of(props) == extra_keys);  // no other key that could change netlisting or display
}

void check_pin(const std::string& name, const schem::Pin& pin, const json& expected) {
  INFO(name << " pin " << expected.at("name").get<std::string>());
  CHECK(pin.box.layer == schem::kPinLayer);
  CHECK(pin.name == expected.at("name").get<std::string>());
  CHECK(pin.direction == expected.at("dir").get<std::string>());
  CHECK_FALSE(pin.sim_pinnumber.has_value());
  const auto box = expected.at("box").get<std::vector<double>>();
  CHECK(pin.box.p1 == schem::Point{box.at(0), box.at(1)});
  CHECK(pin.box.p2 == schem::Point{box.at(2), box.at(3)});
  check_properties(name, pin.box.props, expected.at("properties"), {"name", "dir"});
}

void check_interface(const std::string& name, const json& entry) {
  const schem::Symbol symbol = schem::make_symbol(name, dl::kLibrary / name, read_symbol(name));
  std::set<std::string> extra;
  if (kNoteFormat.count(name) != 0) extra.insert("format");
  check_properties(name, symbol.property(), entry.at("properties"), extra);
  const json& pins = entry.at("pins");
  REQUIRE(symbol.pins.size() == pins.size());
  for (std::size_t i = 0; i < pins.size(); ++i) check_pin(name, symbol.pins[i], pins[i]);
  std::vector<schem::PinIndex> file_order(pins.size());
  for (std::size_t i = 0; i < file_order.size(); ++i) file_order[i] = i;
  CHECK(schem::pin_order(symbol) == file_order);  // no sim_pinnumber: pin order is file order
}

}  // namespace

TEST_CASE("the library holds exactly the inventoried symbols and netlist_options.sym", kSystemLibrary) {
  std::set<std::string> table;
  for (const auto& [name, entry] : dl::interface_table().items()) table.insert(name);
  CHECK(library_files() == table);

  std::set<std::string> inventoried{"netlist_options.sym"};  // Q-SCHEM-31: placed by SCHEM-NL-17
  const json inventory = dl::read_json(dl::kSource / "tests/data/schem/inventory.json").at("pdks");
  for (const auto& [pdk, data] : inventory.items()) {
    for (const auto& device : data.at("devices")) inventoried.insert(device.get<std::string>());
  }
  CHECK(table == inventoried);
  CHECK(table.size() == 43);
}

TEST_CASE("every library symbol has the interface of the table", kSystemLibrary) {
  for (const auto& [name, entry] : dl::interface_table().items()) {
    DYNAMIC_SECTION(name) { check_interface(name, entry); }
  }
}

TEST_CASE("every library file is a write fixpoint with the Apache-2.0 header", kSystemLibrary) {
  for (const std::string& name : library_files()) {
    INFO(name);
    const std::string bytes = dl::read_text(dl::kLibrary / name);
    const schem::Document document = read_symbol(name);
    CHECK(document.file_version == "1.3");
    CHECK(document.header_text.rfind("* SPDX-License-Identifier: Apache-2.0\n", 0) == 0);
    CHECK(document.header_text.find("Copyright 2026 The Hefesto Authors") != std::string::npos);
    const std::string written = schem::write_document(document);
    CHECK(written == bytes);
    // The header text survives a load/save round trip.
    schem::ReadOptions options;
    options.kind = schem::FileKind::symbol;
    CHECK(schem::read_document(written, options).value().header_text == document.header_text);
  }
}

TEST_CASE("library graphics are drawn on the symbol layers only", kSystemLibrary) {
  // Pins are the only layer-5 boxes, and no symbol carries wires, instances
  // or embedded blocks (a library symbol is a plain .sym).
  for (const std::string& name : library_files()) {
    INFO(name);
    const schem::Document document = read_symbol(name);
    std::size_t pin_boxes = 0;
    for (const auto& box : document.boxes) pin_boxes += box.layer == schem::kPinLayer ? 1 : 0;
    CHECK(pin_boxes == dl::interface_table().at(name).at("pins").size());
    CHECK(document.wires.empty());
    CHECK(document.instances.empty());
    CHECK(document.embedded.empty());
    for (const auto& line : document.lines) CHECK(line.layer != schem::kPinLayer);
  }
}
