// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Malformed CIF input stays bounded (design D21 resource caps): integers
// saturate, scaled values are computed in 64 bits, out-of-plane elements
// are ignored with DB-GEOM-001 as the .mag reader does, the symbol count is
// capped, and random bytes never crash the parser. Owner: CIF.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <random>
#include <string>
#include <vector>

#include "recording_target.hpp"

using hefesto::cif::test::body;
using hefesto::cif::test::Reader;
using Events = std::vector<std::string>;

namespace {

bool has_code(const Reader& r, std::string_view code) {
  const auto codes = r.codes();
  return std::find(codes.begin(), codes.end(), code) != codes.end();
}

}  // namespace

TEST_CASE("A box outside the usable plane is ignored with DB-GEOM-001",
          "[cap:cif-format-io][req:Box command]") {
  Reader r;
  CHECK(body(r.read("L CMF; B 4 4 2000000000 0; B 4 4 2 2; E")) == Events{"paint 0 rect (0,0)-(4,4)"});
  CHECK(has_code(r, "DB-GEOM-001"));
}

TEST_CASE("A coordinate overflowing 64 bits rejects its element and never rescales",
          "[cap:cif-format-io][req:Point scaling on input]") {
  Reader r;
  CHECK(body(r.read("DS 1 4611686018427387903 3; L CMF; B 4 4 99999999999999999999 4; P 7 7 9 9 9 7; DF; E")) ==
        Events{"begin 1", "end"});
  CHECK(has_code(r, "DB-GEOM-001"));
  for (const auto& e : r.target.events) {
    CHECK(e.rfind("rescale", 0) != 0);
  }
}

TEST_CASE("A call translated outside the plane is ignored", "[cap:cif-format-io][req:Calls and transformations]") {
  Reader r;
  CHECK(body(r.read("91 a; C 1 T 2000000000 0; C 1 T 1 1; E")) == Events{"place 1 [1 0 1 0 1 1] forward"});
  CHECK(has_code(r, "DB-GEOM-001"));
}

// Values saturate at 2^61, so the sum of a saturated size and a saturated
// center, and a chain of saturated translations, stay within 64 bits (the
// linux-asan leg runs these under UBSan).
TEST_CASE("A box with a saturated size and center is ignored without overflow",
          "[cap:cif-format-io][req:Box command]") {
  Reader r;
  CHECK(body(r.read("L CMF; B 4611686018427387904 4 4611686018427387904 0; B 4 4 2 2; E")) ==
        Events{"paint 0 rect (0,0)-(4,4)"});
  CHECK(has_code(r, "DB-GEOM-001"));
}

TEST_CASE("A round flash with a saturated diameter and center is ignored without overflow",
          "[cap:cif-format-io][req:Round flash command]") {
  Reader r;
  CHECK(body(r.read("L CMF; R 4611686018427387904 4611686018427387904 0; E")).empty());
  CHECK(has_code(r, "DB-GEOM-001"));
}

TEST_CASE("An area label with a saturated size and center is ignored without overflow",
          "[cap:cif-format-io][req:Label user extensions]") {
  Reader r;
  CHECK(body(r.read("L CMF; 95 a 4611686018427387904 4 4611686018427387904 0; E")).empty());
  CHECK(has_code(r, "DB-GEOM-001"));
}

TEST_CASE("Repeated saturated translations of a call are ignored without overflow",
          "[cap:cif-format-io][req:Calls and transformations]") {
  Reader r;
  CHECK(body(r.read("91 a; C 1 T 4611686018427387904 0 T 4611686018427387904 0 T 4611686018427387904 0 "
                    "T 0 -4611686018427387904 T 0 -4611686018427387904; C 1 T 1 1; E")) ==
        Events{"place 1 [1 0 1 0 1 1] forward"});
  CHECK(has_code(r, "DB-GEOM-001"));
}

TEST_CASE("Out-of-plane wires, flashes and labels are ignored", "[cap:cif-format-io][req:Wire command]") {
  Reader r;
  CHECK(body(r.read("L CMF; W 4000000000000 0 0 10 0; R 10 1500000000 0; 94 a 1500000000 0; "
                    "95 b 4 4 1500000000 0; E"))
            .empty());
  CHECK(r.codes() == Events{"DB-GEOM-001", "DB-GEOM-001", "DB-GEOM-001", "DB-GEOM-001"});
}

TEST_CASE("The symbol cap stops new cells and drops their contents",
          "[cap:cif-format-io][req:Symbol-to-cell naming on read]") {
  Reader r;
  r.options.max_symbols = 2;
  CHECK(body(r.read("DS 1; DF; DS 2; DF; DS 3; L CMF; B 2 2 1 1; 94 x 1 1; C 1; DF; C 4; C 1; E")) ==
        Events{"begin 1", "end", "begin 2", "end", "place 1 [1 0 0 0 1 0] defined"});
  CHECK(r.codes() == Events{"CIF-READ-006"});
}

TEST_CASE("Repeated odd area labels stop rescaling at the unit cap", "[cap:cif-format-io][req:Label user extensions]") {
  Reader r;
  // Each symbol starts at N = 1, so its first odd area label rescales and M
  // doubles; without a cap M would overflow after 62 symbols.
  std::string text;
  for (int i = 1; i <= 40; ++i) {
    text += "DS " + std::to_string(i) + "; 95 a 1 1 0 0; DF; ";
  }
  text += "E";
  static_cast<void>(r.read(text));
  const auto rescales = std::count(r.target.events.begin(), r.target.events.end(), std::string{"rescale 2"});
  CHECK(rescales == 30);
  CHECK(r.printed_line("Warning at line 1 of CIF file: CIF units at maximum scale; value is rounded"));
}

TEST_CASE("At the unit cap an odd area label is halved toward zero in both modes",
          "[cap:cif-format-io][req:Label user extensions]") {
  for (const bool compat : {true, false}) {
    Reader r;
    r.options.magic_compat = compat;
    std::string text;
    for (int i = 1; i <= 30; ++i) {
      text += "DS " + std::to_string(i) + "; 95 a 1 1 0 0; DF; ";
    }
    // M is now 2^30: ll = (-3,-3) and ur = (-1,-1) are halved, not rescaled.
    static_cast<void>(r.read(text + "DS 31; 95 b 1 1 -1 -1 CMF; DF; E"));
    REQUIRE(r.target.events.size() >= 4);
    const auto tail = std::vector<std::string>(r.target.events.end() - 4, r.target.events.end());
    CHECK(tail == std::vector<std::string>{"begin 31", "label b (-1,-1)-(0,0) type=3", "end", "end_top"});
    CHECK(r.printed().back() == "Warning at line 1 of CIF file: CIF units at maximum scale; value is rounded");
  }
}

TEST_CASE("Random bytes never crash the parser", "[cap:cif-format-io][req:CIF input grammar]") {
  const std::string alphabet = "DSFCLBRPWE0123456789-;() \n9145TMXYabc";
  std::mt19937 rng(20261113);
  for (int round = 0; round < 300; ++round) {
    std::string text;
    const auto length = std::uniform_int_distribution<int>(0, 400)(rng);
    for (int i = 0; i < length; ++i) {
      if (std::uniform_int_distribution<int>(0, 9)(rng) == 0) {
        text.push_back(static_cast<char>(std::uniform_int_distribution<int>(0, 255)(rng)));
      } else {
        text.push_back(alphabet[std::uniform_int_distribution<std::size_t>(0, alphabet.size() - 1)(rng)]);
      }
    }
    Reader r;
    r.options.magic_compat = (round % 2) == 0;
    r.options.max_symbols = 64;
    r.style.has_copyup = true;
    static_cast<void>(r.read(text));
    CHECK(r.target.events.back() == "end_top");
  }
}
