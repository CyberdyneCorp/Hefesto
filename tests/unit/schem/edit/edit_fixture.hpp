// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Helpers of the schem/edit tests: a headless `schematic` session (CMD's
// fixture) with a new document whose search path is the system library and
// the session's temporary folder.
//
// Owner: EDIT.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <vector>

#include "../cmd/cmd_fixture.hpp"
#include "hefesto/schem/cmd/commands.hpp"
#include "hefesto/schem/session.hpp"

namespace hefesto::test {

/// Installs a copy of the system library under the session folder (the
/// project sandbox grants only that folder) as `<CAD_ROOT>/assets/schematic`
/// with CAD_ROOT the session folder, and returns its `devices` folder.
inline std::string devices_folder(SchemSession& s) {
  const std::filesystem::path devices = s.folder().path() / "assets/schematic/xschem_library/devices";
  if (!std::filesystem::exists(devices)) {
    std::filesystem::create_directories(devices.parent_path());
    std::filesystem::copy(std::filesystem::path{HEFESTO_SOURCE_DIR} / "assets/schematic/xschem_library/devices",
                          devices, std::filesystem::copy_options::recursive);
  }
  s.editor().script.sandbox.environment["CAD_ROOT"] = s.folder().str();
  return devices.generic_string();
}

/// Creates `name` in the session folder (not saved) with the system library
/// and the session folder on the search path.
inline void new_schematic(SchemSession& s, const std::string& name = "top.sch") {
  REQUIRE(s.schematic({"new", "schematic", s.folder().str(name)}).ok());
  REQUIRE(s.schematic({"path", "set", devices_folder(s) + ":" + s.folder().str()}).ok());
}

inline schem::Document& active(SchemSession& s) {
  schem::OpenDocument* open = schem::active_document(s.schem());
  REQUIRE(open != nullptr);
  return open->document;
}

/// Runs `schematic <args>` and requires it to succeed; returns its value text.
inline std::string run(SchemSession& s, std::vector<std::string> args) {
  const cmd::CommandResult result = s.schematic(std::move(args));
  INFO(SchemSession::message(result));
  REQUIRE(result.ok());
  return result.value.to_string();
}

/// Saves the active document to `name` and returns the file text.
inline std::string saved(SchemSession& s, const std::string& name = "out.sch") {
  run(s, {"save", s.folder().str(name)});
  return s.folder().read(name);
}

/// The lines of `text` that start with `prefix`.
inline std::vector<std::string> records(const std::string& text, const std::string& prefix) {
  std::vector<std::string> out;
  std::string::size_type start = 0;
  while (start < text.size()) {
    auto end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    const std::string line = text.substr(start, end - start);
    if (line.rfind(prefix, 0) == 0) out.push_back(line);
    start = end + 1;
  }
  return out;
}

/// A `subcircuit` symbol `<cell>.sym` with an input pin `A` at (-20,0) and
/// an output pin `Y` at (20,0), and its schematic `<cell>.sch` with ports
/// `A` and `Y` and, when `child` is given, one instance of `<child>.sym`.
inline void write_cell(SchemSession& s, const std::string& cell, const std::string& child = {}) {
  s.folder().write(cell + ".sym",
                   "v {xschem version=3.4.8RC file_version=1.3}\n"
                   "K {type=subcircuit\nformat=\"@name @pinlist @symname\"\ntemplate=\"name=x1\"}\n"
                   "B 5 -22.5 -2.5 -17.5 2.5 {name=A dir=in}\n"
                   "B 5 17.5 -2.5 22.5 2.5 {name=Y dir=out}\n"
                   "L 4 -20 0 20 0 {}\n");
  std::string sch =
      "v {xschem version=3.4.8RC file_version=1.3}\n"
      "C {ipin.sym} -100 0 0 0 {name=p1 lab=A}\n"
      "C {opin.sym} 100 0 0 0 {name=p2 lab=Y}\n";
  if (!child.empty()) sch += "C {" + child + ".sym} 0 0 0 0 {name=x2}\n";
  s.folder().write(cell + ".sch", sch);
}

}  // namespace hefesto::test
