// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The version record, header text, writer version string and global
// property records (capability schematic-file-format; Q-SCHEM-1, Q-SCHEM-3;
// openspec/changes/m8-schematic tasks.md 6.2).
//
// Owner: FORMAT.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "file_test_util.hpp"
#include "hefesto/schem/file/read_write.hpp"

namespace schem = hefesto::schem;
using schem::test::kEmptyHeader;
using schem::test::read_text;
using schem::test::round_trip;

TEST_CASE("license header preserved", "[cap:schematic-file-format][req:Version record and header text]") {
  const auto r = read_text("v {xschem version=3.4.7RC file_version=1.2\n* Copyright 2021 X\n}\nK {type=nmos}\n",
                           schem::FileKind::symbol);
  CHECK(r.doc.file_version == "1.2");
  CHECK(r.doc.header_text == "* Copyright 2021 X\n");
  const std::string written = schem::write_document(r.doc);
  CHECK(written.rfind("v {xschem version=3.4.8RC file_version=1.3\n* Copyright 2021 X\n}\nG {}\nK {type=nmos}\n", 0) ==
        0);
}

TEST_CASE("header text is brace-escaped and loses leading blanks and one LF",
          "[cap:schematic-file-format][req:Version record and header text]") {
  const auto r = read_text("v {xschem version=3.4.8RC file_version=1.3 \t\n\nmy \\{header\\}\\\\}\n");
  CHECK(r.doc.header_text == "\nmy {header}\\");
  CHECK(schem::write_document(r.doc).rfind("v {xschem version=3.4.8RC file_version=1.3\n\nmy \\{header\\}\\\\}\nG {}\n", 0) == 0);
}

TEST_CASE("the xschem key is removed only near the start", "[cap:schematic-file-format][req:Version record and header text]") {
  const std::string padding(60, '.');
  const auto r = read_text("v {" + padding + " xschem file_version=1.1}\n");
  CHECK(r.doc.file_version == "1.1");
  CHECK(r.doc.header_text == padding + " xschem ");
}

TEST_CASE("missing version means 1.0", "[cap:schematic-file-format][req:Version record and header text]") {
  for (const bool compat : {true, false}) {
    const auto r = read_text("K {}\nC {res} 0 0 0 0 {name=R1}\n", schem::FileKind::schematic, compat);
    CHECK(r.doc.file_version == "1.0");
    REQUIRE(r.doc.instances.size() == 1);
    CHECK(r.doc.instances[0].symbol == "res.sym");
  }
}

TEST_CASE("file version 1.0 replaces the extension; 1.1 keeps the reference",
          "[cap:schematic-file-format][req:Version record and header text]") {
  const auto old = read_text("v {xschem version=2.0 file_version=1.0}\nC {lib.v2/res.cir} 0 0 0 0 {}\nC {a.b/c} 0 0 0 0 {}\n");
  REQUIRE(old.doc.instances.size() == 2);
  CHECK(old.doc.instances[0].symbol == "lib.v2/res.sym");
  CHECK(old.doc.instances[1].symbol == "a.b/c.sym");
  const auto newer = read_text("v {xschem version=2.9 file_version=1.1}\nC {res} 0 0 0 0 {}\n");
  CHECK(newer.doc.instances[0].symbol == "res");
  const auto no_key = read_text("v {xschem version=2.9}\nC {res} 0 0 0 0 {}\n");
  CHECK(no_key.doc.file_version == "1.0");
  CHECK(no_key.doc.instances[0].symbol == "res.sym");
}

TEST_CASE("leading instance record, xschem-compat ON", "[cap:schematic-file-format][req:Version record and header text]") {
  const auto r = read_text("C {res} 0 0 0 0 {name=R1}\nC {capa} 0 0 0 0 {name=C1}\n", schem::FileKind::schematic, true);
  REQUIRE(r.doc.instances.size() == 2);
  CHECK(r.doc.instances[0].symbol == "res");
  CHECK(r.doc.instances[1].symbol == "capa.sym");
  CHECK(r.doc.file_version == "1.0");
}

TEST_CASE("leading instance record, xschem-compat OFF", "[cap:schematic-file-format][req:Version record and header text]") {
  const auto r = read_text("C {res} 0 0 0 0 {name=R1}\nC {capa} 0 0 0 0 {name=C1}\n", schem::FileKind::schematic, false);
  REQUIRE(r.doc.instances.size() == 2);
  CHECK(r.doc.instances[0].symbol == "res.sym");
  CHECK(r.doc.instances[1].symbol == "capa.sym");
}

TEST_CASE("pinned version", "[cap:schematic-file-format][req:Writer version string]") {
  CHECK(schem::kWriterVersion == "3.4.8RC");
  for (const bool compat : {true, false}) {
    const std::string written =
        round_trip("v {xschem version=3.5.0 file_version=1.2}\nN 0 0 1 1 {}\n", schem::FileKind::schematic, compat);
    CHECK(written.rfind("v {xschem version=3.4.8RC file_version=1.3}\n", 0) == 0);
  }
  CHECK(schem::write_document(schem::Document{}) == kEmptyHeader);
}

TEST_CASE("pre-1.2 symbol upgraded", "[cap:schematic-file-format][req:Global property records]") {
  const auto r = read_text("v {xschem version=2.9.5 file_version=1.1}\nG {type=subcircuit format=\"@name @pinlist @symname\"}\n",
                           schem::FileKind::symbol);
  CHECK(r.doc.globals.k == "type=subcircuit format=\"@name @pinlist @symname\"");
  CHECK(r.doc.globals.g.empty());
  CHECK(schem::write_document(r.doc) ==
        "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {type=subcircuit format=\"@name @pinlist @symname\"}\n"
        "V {}\nS {}\nF {}\nE {}\n");
}

TEST_CASE("G stays VHDL text in schematics, with a K record or without a type",
          "[cap:schematic-file-format][req:Global property records]") {
  const std::string g = "G {type=subcircuit}\n";
  CHECK(read_text(g, schem::FileKind::schematic).doc.globals.g == "type=subcircuit");
  CHECK(read_text(g + "K {}\n", schem::FileKind::symbol).doc.globals.g == "type=subcircuit");
  CHECK(read_text("G {type=\"\" x=1}\n", schem::FileKind::symbol).doc.globals.g == "type=\"\" x=1");
}

TEST_CASE("global records are written G, K, V, S, F, E", "[cap:schematic-file-format][req:Global property records]") {
  CHECK(round_trip("E {e}\nF {f}\nS {s}\nV {v}\nK {k}\nG {g}\n") ==
        "v {xschem version=3.4.8RC file_version=1.3}\nG {g}\nK {k}\nV {v}\nS {s}\nF {f}\nE {e}\n");
  schem::Document symbol;
  symbol.kind = schem::FileKind::symbol;
  symbol.globals.g = "type=label";
  CHECK(schem::write_document(symbol) ==
        "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {type=label}\nV {}\nS {}\nF {}\nE {}\n");
}
