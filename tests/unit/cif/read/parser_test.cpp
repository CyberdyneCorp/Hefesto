// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The CIF parser against a recording target (capability cif-format-io;
// openspec/changes/m5-mask-gds-cif tasks.md 11.1). Coordinates are in read
// units: the engine converts them (capability mask-input-styles). Owner: CIF.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "recording_target.hpp"

using hefesto::cif::test::body;
using hefesto::cif::test::Reader;
using Events = std::vector<std::string>;

// --- lexical rules and grammar ----------------------------------------------

TEST_CASE("Lower-case text is ignored", "[cap:cif-format-io][req:CIF lexical rules]") {
  Reader r;
  CHECK(body(r.read("DS 1 25 2; L CMF; Box 4 2 2 1; DF; End")) ==
        Events{"begin 1", "paint 0 rect (0,0)-(50,25)", "end"});
  CHECK(r.printed().empty());
  CHECK(r.session.saw_end);
}

TEST_CASE("Nested comment", "[cap:cif-format-io][req:CIF lexical rules]") {
  Reader r;
  CHECK(body(r.read("(outer (inner) still comment);\nE")).empty());
  CHECK(r.printed().empty());
}

TEST_CASE("A comment reaching the end of file is an error", "[cap:cif-format-io][req:CIF lexical rules]") {
  Reader r;
  static_cast<void>(r.read("(never closed"));
  CHECK(r.printed() == Events{"Error at line 1 of CIF file: (comment) extends to end of file.",
                              "Error at line 1 of CIF file: `;' expected.",
                              "Error at line 1 of CIF file: no \"End\" statement."});
}

TEST_CASE("A missing semicolon is reported and parsing continues", "[cap:cif-format-io][req:CIF lexical rules]") {
  Reader r;
  CHECK(body(r.read("L CMF; B 2 2 1 1 0 1\n(c); B 2 2 3 3; E")) ==
        Events{"paint 0 rect (0,0)-(2,2)", "paint 0 rect (2,2)-(4,4)"});
  CHECK(r.printed() == Events{"Error at line 2 of CIF file: `;' expected."});
}

TEST_CASE("Unknown command is skipped", "[cap:cif-format-io][req:CIF input grammar]") {
  Reader r;
  CHECK(body(r.read("L CMF;\n\nQ 1 2; B 2 2 1 1; E")) == Events{"paint 0 rect (0,0)-(2,2)"});
  CHECK(r.printed() == Events{"Error at line 3 of CIF file: unknown command `Q'; ignored."});
}

TEST_CASE("Unsupported user extension", "[cap:cif-format-io][req:CIF input grammar]") {
  Reader r;
  static_cast<void>(r.read("92 foo; 3 bar; 9X; E"));
  CHECK(r.printed() == Events{"Error at line 1 of CIF file: unimplemented user extension; ignored.",
                              "Error at line 1 of CIF file: unimplemented user extension; ignored.",
                              "Error at line 1 of CIF file: unimplemented user extension; ignored."});
}

TEST_CASE("D followed by another letter is an unknown command", "[cap:cif-format-io][req:CIF input grammar]") {
  Reader r;
  static_cast<void>(r.read("DX 1; E"));
  CHECK(r.printed() == Events{"Error at line 1 of CIF file: unknown command `X'; ignored."});
}

TEST_CASE("The annotated example of the grammar", "[cap:cif-format-io][req:CIF input grammar]") {
  Reader r;
  const char* text =
      "(written by a generator);\n"
      "DS 1 25 2;\n"
      "9 inv;\n"
      "L CMF;\n"
      "    B 40 20 20 10;\n"
      "94 out 40 10 CMF;\n"
      "DF;\n"
      "91 u1;\n"
      "C 1 R 0 1 T 100 0;\n"
      "End\n";
  CHECK(body(r.read(text)) == Events{"begin 1", "rename 1 inv", "paint 0 rect (0,0)-(500,250)",
                                     "label out (500,125)-(500,125) type=3", "end",
                                     "place inv [0 -1 100 1 0 0] id=u1 defined"});
  CHECK(r.printed().empty());
}

// --- symbol definitions -----------------------------------------------------

TEST_CASE("Symbol scale with style multiplier", "[cap:cif-format-io][req:Symbol definitions and scale]") {
  Reader r;
  r.style.multiplier = 10;
  // N = 250, Dn = 2: a CIF coordinate 4 is 500 read units.
  CHECK(body(r.read("DS 3 25 2; L CMF; P 4 0 4 4 0 4; DF; E")) ==
        Events{"begin 3", "paint 0 poly (500,0) (500,500) (0,500)", "end"});
}

TEST_CASE("Malformed symbol starts are reported", "[cap:cif-format-io][req:Symbol definitions and scale]") {
  Reader r;
  static_cast<void>(r.read("DS; DS -2; DS 1 5; DF; DS 2 0 1; DS 3; DF; DF; E"));
  CHECK(r.printed() == Events{
                           "Error at line 1 of CIF file: definition start, but no symbol number; ignored.",
                           "Error at line 1 of CIF file: negative integer not permitted.",
                           "Error at line 1 of CIF file: illegal negative symbol number; definition ignored.",
                           "Error at line 1 of CIF file: only one of two scale factors given; ignored.",
                           "Error at line 1 of CIF file: Illegal scale 0 / 1 changed to 1 / 1",
                           "Error at line 1 of CIF file: definition start inside other definition; ignored.",
                           "Error at line 1 of CIF file: definition finish without definition start; ignored.",
                       });
  CHECK(body(r.target.events) == Events{"begin 1", "end", "begin 2", "end"});
}

TEST_CASE("No layer is current at symbol start, and the top-level layer comes back after DF",
          "[cap:cif-format-io][req:Symbol definitions and scale]") {
  Reader r;
  CHECK(body(r.read("L CPG; DS 1; B 2 2 1 1; L CMF; B 2 2 1 1; DF; B 2 2 1 1; E")) ==
        Events{"begin 1", "paint 0 rect (0,0)-(2,2)", "end", "paint 1 rect (0,0)-(2,2)"});
}

TEST_CASE("DF restores the top-level scale 1/1", "[cap:cif-format-io][req:Symbol definitions and scale]") {
  Reader r;
  CHECK(body(r.read("DS 1 10 1; DF; L CMF; B 2 2 1 1; E")) ==
        Events{"begin 1", "end", "paint 0 rect (0,0)-(2,2)"});
}

TEST_CASE("A pending 91 id is discarded at DS and DF", "[cap:cif-format-io][req:Symbol definitions and scale]") {
  Reader r;
  static_cast<void>(r.read("91 a; DS 1; 91 b; DF; E"));
  CHECK(r.printed() == Events{"Error at line 1 of CIF file: pending call identifier a discarded.",
                              "Error at line 1 of CIF file: pending call identifier b discarded."});
}

// --- symbol-to-cell naming --------------------------------------------------

TEST_CASE("Redefined symbol keeps the old cell", "[cap:cif-format-io][req:Symbol-to-cell naming on read]") {
  Reader r;
  CHECK(body(r.read("DS 5; L CMF; B 2 2 1 1; DF; C 5; DS 5; L CPG; B 2 2 1 1; DF; C 5; E")) ==
        Events{"begin 5", "paint 0 rect (0,0)-(2,2)", "end", "place 5 [1 0 0 0 1 0] defined", "rename 5 5_1",
               "begin 5", "paint 1 rect (0,0)-(2,2)", "end", "place 5 [1 0 0 0 1 0] defined"});
  CHECK(r.printed() == Events{"Error at line 1 of CIF file: Warning: cell definition 5 reused."});
  REQUIRE(r.session.symbols.cells().size() == 2);
  CHECK(r.session.symbols.cells()[0].name == "5_1");
  CHECK(r.session.symbols.cells()[1].name == "5");
}

TEST_CASE("Existing user cell is not cleared", "[cap:cif-format-io][req:Symbol-to-cell naming on read]") {
  Reader r;
  r.target.library.insert("5");
  CHECK(body(r.read("DS 5; L CMF; B 2 2 1 1; DF; C 5; E")) ==
        Events{"begin 5_1", "paint 0 rect (0,0)-(2,2)", "end", "place 5_1 [1 0 0 0 1 0] defined"});
  CHECK(r.printed().empty());
}

TEST_CASE("DD forgets higher symbols with magic-compat OFF", "[cap:cif-format-io][req:Symbol-to-cell naming on read]") {
  const char* text = "DS 3; DF; DS 7; DF; DD 3; C 7; E";
  SECTION("magic-compat OFF") {
    Reader r;
    r.options.magic_compat = false;
    CHECK(body(r.read(text)) ==
          Events{"begin 3", "end", "begin 7", "end", "place 7_1 [1 0 0 0 1 0] forward"});
    CHECK(r.printed() == Events{"Error at line 1 of CIF file: cell 7_1 was used but not defined."});
  }
  SECTION("magic-compat ON") {
    Reader r;
    CHECK(body(r.read(text)) == Events{"begin 3", "end", "begin 7", "end", "place 7 [1 0 0 0 1 0] defined"});
    CHECK(r.printed().empty());
  }
}

TEST_CASE("A forward call and the later DS name the same new cell",
          "[cap:cif-format-io][req:Symbol-to-cell naming on read]") {
  Reader r;
  r.target.library.insert("2");  // a library cell named like the symbol
  CHECK(body(r.read("DS 1; C 2; DF; DS 2; DF; E")) ==
        Events{"begin 1", "place 2_1 [1 0 0 0 1 0] forward", "end", "begin 2_1", "end"});
  CHECK(r.printed().empty());
}

TEST_CASE("DD then redefinition renames the earlier cell", "[cap:cif-format-io][req:Symbol-to-cell naming on read]") {
  Reader r;
  CHECK(body(r.read("DS 4; DF; DD 4; DS 4; DF; E")) == Events{"begin 4", "end", "rename 4 4_1", "begin 4", "end"});
  CHECK(r.printed() == Events{"Error at line 1 of CIF file: Warning: cell definition 4 reused."});
}

TEST_CASE("A redefinition after 9 does not touch the renamed cell",
          "[cap:cif-format-io][req:Symbol-to-cell naming on read]") {
  Reader r;
  CHECK(body(r.read("DS 1; 9 inv; DF; DS 1; DF; E")) ==
        Events{"begin 1", "rename 1 inv", "end", "begin 1", "end"});
  CHECK(r.printed().empty());
}

TEST_CASE("9 to an existing name keeps the cell's name", "[cap:cif-format-io][req:Symbol-to-cell naming on read]") {
  Reader r;
  CHECK(body(r.read("DS 1; 9 top; DF; E")) == Events{"begin 1", "end"});
  CHECK(r.printed() == Events{"Error at line 1 of CIF file: top already exists, so cell from CIF is named 1."});
}

TEST_CASE("Top-level 9 renames the edit cell with magic-compat ON only",
          "[cap:cif-format-io][req:Symbol-to-cell naming on read]") {
  SECTION("magic-compat ON") {
    Reader r;
    CHECK(body(r.read("9 chip; E")) == Events{"rename top chip"});
    CHECK(r.printed().empty());
  }
  SECTION("magic-compat OFF") {
    Reader r;
    r.options.magic_compat = false;
    CHECK(body(r.read("9 chip; E")).empty());
    CHECK(r.printed() == Events{"Warning at line 1 of CIF file: 9 chip outside a symbol definition; ignored."});
  }
}

// --- calls and transformations ----------------------------------------------

TEST_CASE("Rotation then translation", "[cap:cif-format-io][req:Calls and transformations]") {
  Reader r;
  // In read units; with scalefactor 100 the engine's exact conversion maps
  // cell point (x,y) to (-y+2, x+3).
  CHECK(body(r.read("C 1 R 0 1 T 200 300; E")) == Events{"place 1 [0 -1 200 1 0 300] forward"});
}

TEST_CASE("Each transformation applies after the previous ones", "[cap:cif-format-io][req:Calls and transformations]") {
  Reader r;
  CHECK(body(r.read("C 1 T 10 0 R 0 1; C 1 M X T 5 6; C 1 MY; C 1 R -1 0; C 1 R 0 -5; E")) ==
        Events{"place 1 [0 -1 0 1 0 10] forward", "place 1 [-1 0 5 0 1 6] forward",
               "place 1 [1 0 0 0 -1 0] forward", "place 1 [-1 0 0 0 -1 0] forward",
               "place 1 [0 1 0 -1 0 0] forward"});
}

TEST_CASE("Instance id from 91", "[cap:cif-format-io][req:Calls and transformations]") {
  Reader r;
  CHECK(body(r.read("DS 2; DF; 91 xbuf_0; C 2; C 2; 91 a; 91 b; C 2; E")) ==
        Events{"begin 2", "end", "place 2 [1 0 0 0 1 0] id=xbuf_0 defined", "place 2 [1 0 0 0 1 0] defined",
               "place 2 [1 0 0 0 1 0] id=b defined"});
  CHECK(r.printed() == Events{"Error at line 1 of CIF file: 91 command with identifier a pending; a discarded."});
}

TEST_CASE("A non-manhattan rotation is the identity", "[cap:cif-format-io][req:Calls and transformations]") {
  Reader r;
  CHECK(body(r.read("C 1 R 1 1 T 4 4; E")) == Events{"place 1 [1 0 4 0 1 4] forward"});
  CHECK(r.printed().front() == "Error at line 1 of CIF file: non-manhattan direction vector (1, 1); ignored.");
}

TEST_CASE("A malformed call transformation follows magic-compat", "[cap:cif-format-io][req:Calls and transformations]") {
  const char* text = "91 u; C 1 T 10 20 M Z; C 1; E";
  SECTION("magic-compat ON places the call with the partial, unscaled transform") {
    Reader r;
    CHECK(body(r.read(text)) ==
          Events{"place 1 [1 0 10 0 1 20] unscaled id=u forward", "place 1 [1 0 0 0 1 0] forward"});
    CHECK(r.printed().front() == "Error at line 1 of CIF file: mirror, but not in X or Y.");
  }
  SECTION("magic-compat OFF skips it and clears the pending id") {
    Reader r;
    r.options.magic_compat = false;
    CHECK(body(r.read(text)) == Events{"place 1 [1 0 0 0 1 0] forward"});
    CHECK(r.printed() == Events{"Error at line 1 of CIF file: mirror, but not in X or Y.",
                                "Error at line 1 of CIF file: call ignored.",
                                "Error at line 1 of CIF file: cell 1 was used but not defined."});
  }
}

TEST_CASE("Other malformed transformations are reported", "[cap:cif-format-io][req:Calls and transformations]") {
  Reader r;
  static_cast<void>(r.read("C 1 T 5; C 1 R 4; C 1 X; C; DS 1; DF; E"));
  const auto lines = r.printed();
  REQUIRE(lines.size() == 4);
  CHECK(lines[0] == "Error at line 1 of CIF file: translation, but no point.");
  CHECK(lines[1] == "Error at line 1 of CIF file: rotation, but no direction.");
  CHECK(lines[2] == "Error at line 1 of CIF file: transformation expected.");
  CHECK(lines[3] == "Error at line 1 of CIF file: call, but no symbol number; ignored.");
}

TEST_CASE("A cell placed inside itself or its descendants is refused",
          "[cap:cif-format-io][req:Calls and transformations]") {
  Reader r;
  CHECK(body(r.read("DS 1; C 1; C 2; DF; DS 2; C 3; DF; DS 3; C 1; DF; E")) ==
        Events{"begin 1", "place 2 [1 0 0 0 1 0] forward", "end", "begin 2", "place 3 [1 0 0 0 1 0] forward",
               "end", "begin 3", "end"});
  CHECK(r.printed() ==
        Events{"Error at line 1 of CIF file: attempt to place cell use inside its own definition!",
               "Error at line 1 of CIF file: attempt to place cell use inside its own definition!"});
}

TEST_CASE("Copyup on a forward call warns once with magic-compat OFF",
          "[cap:cif-format-io][req:Calls and transformations]") {
  const char* text = "C 1; C 1; DS 1; DF; C 1; E";
  SECTION("magic-compat OFF") {
    Reader r;
    r.style.has_copyup = true;
    r.options.magic_compat = false;
    CHECK(body(r.read(text)) == Events{"place 1 [1 0 0 0 1 0] forward", "place 1 [1 0 0 0 1 0] forward",
                                       "begin 1", "end", "place 1 [1 0 0 0 1 0] defined"});
    CHECK(r.printed() == Events{"Warning at line 1 of CIF file: call to symbol 1 precedes its definition; its "
                                "copyup layers are not copied."});
  }
  SECTION("magic-compat ON") {
    Reader r;
    r.style.has_copyup = true;
    static_cast<void>(r.read(text));
    CHECK(r.printed().empty());
  }
}

// --- point scaling ----------------------------------------------------------

TEST_CASE("Exact division", "[cap:cif-format-io][req:Point scaling on input]") {
  Reader r;
  CHECK(body(r.read("DS 1 25 2; L CMF; P 4 6 8 6 8 10; DF; E")) ==
        Events{"begin 1", "paint 0 poly (50,75) (100,75) (100,125)", "end"});
}

TEST_CASE("A half-unit coordinate subdivides the read units", "[cap:cif-format-io][req:Point scaling on input]") {
  Reader r;
  // N = 1, Dn = 2: the coordinate 3 sets r = 2, N becomes 2 and the value is
  // 3 read units in the new units; the points parsed before are doubled.
  CHECK(body(r.read("DS 1 1 2; L CMF; P 2 0 3 0 3 2; P 2 3 4 3 4 4; DF; E")) ==
        Events{"begin 1", "rescale 2", "paint 0 poly (2,0) (3,0) (3,2)", "paint 0 poly (2,3) (4,3) (4,4)",
               "end"});
  CHECK(r.printed() == Events{"Warning at line 1 of CIF file: CIF style lambda: units rescaled by factor of 2 / 1"});
}

TEST_CASE("A subdivision triggered by y multiplies x", "[cap:cif-format-io][req:Point scaling on input]") {
  Reader r;
  CHECK(body(r.read("DS 1 1 2; L CMF; P 2 3 4 4 4 6; DF; E")) ==
        Events{"begin 1", "rescale 2", "paint 0 poly (2,3) (4,4) (4,6)", "end"});
}

TEST_CASE("Past the rescale limit a value is rounded", "[cap:cif-format-io][req:Point scaling on input]") {
  Reader r;
  r.options.rescale_limit = 1;
  CHECK(body(r.read("DS 1 1 2; L CMF; P 3 -3 5 -3 5 0; DF; E")) ==
        Events{"begin 1", "paint 0 poly (2,-1) (3,-1) (3,0)", "end"});
  CHECK(r.printed().size() == 5);
  CHECK(r.printed().front() == "Warning at line 1 of CIF file: CIF units at maximum scale; value is rounded");
}

// --- layers -----------------------------------------------------------------

TEST_CASE("Geometry before a layer command", "[cap:cif-format-io][req:Layer selection]") {
  Reader r;
  CHECK(body(r.read("DS 1; B 10 10 5 5; L CMF; B 10 10 5 5; DF; E")) ==
        Events{"begin 1", "paint 0 rect (0,0)-(10,10)", "end"});
  CHECK(r.printed().empty());
}

TEST_CASE("An unknown layer drops geometry until the next valid L", "[cap:cif-format-io][req:Layer selection]") {
  Reader r;
  CHECK(body(r.read("L CMF; L XYZ; B 2 2 1 1; L CNW; B 2 2 1 1; L CMFPIN; B 2 2 1 1; L CPG; B 2 2 1 1; E")) ==
        Events{"paint 1 rect (0,0)-(2,2)"});
  CHECK(r.printed() == Events{"Error at line 1 of CIF file: layer XYZ isn't known in the current style.",
                              "Error at line 1 of CIF file: layer CNW isn't known in the current style.",
                              "Error at line 1 of CIF file: layer CMFPI isn't known in the current style."});
}

// --- box, flash, polygon, wire ----------------------------------------------

TEST_CASE("Even box", "[cap:cif-format-io][req:Box command]") {
  Reader r;
  CHECK(body(r.read("L CMF; B 40 20 20 10; E")) == Events{"paint 0 rect (0,0)-(40,20)"});
}

TEST_CASE("Odd length centered on an integer", "[cap:cif-format-io][req:Box command]") {
  SECTION("magic-compat ON truncates") {
    Reader r;
    CHECK(body(r.read("L CMF; B 3 2 10 0; B 3 2 0 0; E")) ==
          Events{"paint 0 rect (8,-1)-(11,1)", "paint 0 rect (-1,-1)-(1,1)"});
  }
  SECTION("magic-compat OFF subdivides the read units") {
    Reader r;
    r.options.magic_compat = false;
    CHECK(body(r.read("DS 1; L CMF; B 3 2 10 0; B 2 2 1 1; DF; E")) ==
          Events{"begin 1", "rescale 2", "paint 0 rect (17,-2)-(23,2)", "paint 0 rect (0,0)-(4,4)", "end"});
  }
}

TEST_CASE("Rotated box", "[cap:cif-format-io][req:Box command]") {
  Reader r;
  CHECK(body(r.read("L CMF; B 40 20 0 0 0 1; B 40 20 0 0 -3 0; E")) ==
        Events{"paint 0 rect (-10,-20)-(10,20)", "paint 0 rect (-20,-10)-(20,10)"});
}

TEST_CASE("Malformed boxes are reported", "[cap:cif-format-io][req:Box command]") {
  Reader r;
  CHECK(body(r.read("L CMF; B 0 5 1 1; B 5 -1 1 1; B 5; B 5 5; B 4 4 2 2 1; E")).empty());
  CHECK(r.printed() == Events{"Error at line 1 of CIF file: box length not strictly positive; ignored.",
                              "Error at line 1 of CIF file: box width not strictly positive; ignored.",
                              "Error at line 1 of CIF file: box, but no length and/or width; ignored.",
                              "Error at line 1 of CIF file: box, but no center; ignored.",
                              "Error at line 1 of CIF file: box, direction botched; box ignored."});
}

TEST_CASE("Flash as a square", "[cap:cif-format-io][req:Round flash command]") {
  Reader r;
  CHECK(body(r.read("L CMF; R 10 50 50; E")) == Events{"paint 0 rect (45,45)-(55,55)"});
}

TEST_CASE("An inexact flash diameter is snapped", "[cap:cif-format-io][req:Round flash command]") {
  Reader r;
  CHECK(body(r.read("DS 1 1 3; L CMF; R 10 30 30; DF; E")) == Events{"begin 1", "paint 0 rect (8,8)-(11,11)", "end"});
  CHECK(r.printed() ==
        Events{"Warning at line 1 of CIF file: Roundflash diameter snapped to nearest integer boundary."});
}

TEST_CASE("Rectangle polygon", "[cap:cif-format-io][req:Polygon command]") {
  Reader r;
  CHECK(body(r.read("L CMF; P 0 0 0 10 20 10 20 0; E")) == Events{"paint 0 poly (0,0) (0,10) (20,10) (20,0)"});
}

TEST_CASE("An improper polygon path is reported", "[cap:cif-format-io][req:Polygon command]") {
  Reader r;
  CHECK(body(r.read("L CMF; P; P 1; E")).empty());
  CHECK(r.printed() == Events{"Error at line 1 of CIF file: polygon, but improper path; ignored.",
                              "Error at line 1 of CIF file: polygon, but improper path; ignored."});
}

TEST_CASE("Straight horizontal wire", "[cap:cif-format-io][req:Wire command]") {
  Reader r;
  CHECK(body(r.read("L CMF; W 10 0 0 100 0; E")) == Events{"paint 0 poly (105,5) (-5,5) (-5,-5) (105,-5)"});
}

TEST_CASE("L-shaped wire (verify against Magic)", "[cap:cif-format-io][req:Wire command]") {
  Reader r;
  // The outline of (-5,-5)-(105,5) united with (95,-5)-(105,105); a repeated
  // point is dropped first.
  CHECK(body(r.read("L CMF; W 10 0 0 100 0 100 100; W 10 0 0 100 0 100 0 100 100; E")) ==
        Events{"paint 0 poly (95,105) (95,5) (-5,5) (-5,-5) (105,-5) (105,105)",
               "paint 0 poly (95,105) (95,5) (-5,5) (-5,-5) (105,-5) (105,105)"});
}

TEST_CASE("An inexact wire width is snapped and a bad wire is reported", "[cap:cif-format-io][req:Wire command]") {
  Reader r;
  static_cast<void>(r.read("DS 1 1 3; L CMF; W 10 0 0 3 0; W; W 3; DF; E"));
  CHECK(r.printed() == Events{"Warning at line 1 of CIF file: Wire width snapped to nearest integer boundary.",
                              "Error at line 1 of CIF file: wire, but no width; ignored.",
                              "Error at line 1 of CIF file: wire, but improper path; ignored."});
}

// --- labels -----------------------------------------------------------------

TEST_CASE("Point label on named layer", "[cap:cif-format-io][req:Label user extensions]") {
  Reader r;
  // 250 and 400 read units; with scalefactor 100 the engine's "any"
  // conversion places the label at (2,4).
  CHECK(body(r.read("94 vdd 250 400 CMF; E")) == Events{"label vdd (250,400)-(250,400) type=3"});
}

TEST_CASE("Area label", "[cap:cif-format-io][req:Label user extensions]") {
  Reader r;
  CHECK(body(r.read("95 bus 20 10 50 50 CMF; E")) == Events{"label bus (40,45)-(60,55) type=3"});
}

TEST_CASE("An odd area label rescales and doubles the symbol numerator",
          "[cap:cif-format-io][req:Label user extensions]") {
  for (const bool compat : {true, false}) {
    Reader r;
    r.options.magic_compat = compat;
    // After the 95 rescale the box keeps its size: (0,0)-(2,2) in the old
    // units is (0,0)-(4,4) in the new ones (Q-MASK-314 fix, both modes).
    CHECK(body(r.read("DS 1; L CMF; 95 a 3 2 0 0 CMF; B 2 2 1 1; DF; E")) ==
          Events{"begin 1", "rescale 2", "label a (-3,-2)-(3,2) type=3", "paint 0 rect (0,0)-(4,4)", "end"});
  }
}

TEST_CASE("Labels on unknown and discarded layers", "[cap:cif-format-io][req:Label user extensions]") {
  Reader r;
  CHECK(body(r.read("94 a 1 1 FOO; 94 b 1 1 CX; 94 c 1 1 CMFP; 94; E")) ==
        Events{"label a (1,1)-(1,1) type=0", "label c (1,1)-(1,1) type=3 sticky"});
  CHECK(r.printed() == Events{"Error at line 1 of CIF file: label attached to unknown layer FOO.",
                              "Error at line 1 of CIF file: 94 command, but no location; ignored."});
}

TEST_CASE("Default label stickiness follows magic-compat", "[cap:cif-format-io][req:Label user extensions]") {
  // CMFP has the label type of CMF and the port kind; CMF has no kind.
  const char* text = "L CMFP; 94 a 1 1; 95 b 2 2 1 1; E";
  SECTION("magic-compat ON: the first layer of the default type decides") {
    Reader r;
    CHECK(body(r.read(text)) == Events{"label a (1,1)-(1,1) type=3", "label b (0,0)-(2,2) type=0"});
  }
  SECTION("magic-compat OFF: the current L layer decides") {
    Reader r;
    r.options.magic_compat = false;
    CHECK(body(r.read(text)) ==
          Events{"label a (1,1)-(1,1) type=3 sticky", "label b (0,0)-(2,2) type=0 sticky"});
  }
  SECTION("magic-compat OFF with no valid layer: not sticky") {
    Reader r;
    r.options.magic_compat = false;
    CHECK(body(r.read("L CMFP; L BAD; 94 a 1 1; E")) == Events{"label a (1,1)-(1,1) type=0"});
  }
}

// --- end of file ------------------------------------------------------------

TEST_CASE("Missing End", "[cap:cif-format-io][req:End of file and top-level geometry]") {
  Reader r;
  CHECK(r.read("DS 1; L CMF; B 2 2 1 1; DF;") == Events{"begin 1", "paint 0 rect (0,0)-(2,2)", "end", "end_top"});
  CHECK(r.printed() == Events{"Error at line 1 of CIF file: no \"End\" statement."});
  CHECK_FALSE(r.session.saw_end);
}

TEST_CASE("Text after End is reported", "[cap:cif-format-io][req:End of file and top-level geometry]") {
  Reader r;
  CHECK(r.read("L CMF; E\nB 2 2 1 1;") == Events{"end_top"});
  CHECK(r.printed() == Events{"Error at line 2 of CIF file: End command isn't at end of file."});
}

TEST_CASE("An open definition is finished at end of file",
          "[cap:cif-format-io][req:End of file and top-level geometry]") {
  Reader r;
  CHECK(r.read("DS 1; C 2; 91 x;") == Events{"begin 1", "place 2 [1 0 0 0 1 0] forward", "end", "end_top"});
  CHECK(r.printed() == Events{"Error at line 1 of CIF file: no \"End\" statement.",
                              "Error at line 1 of CIF file: CIF ended partway through a symbol definition.",
                              "Error at line 1 of CIF file: pending call identifier x discarded.",
                              "Error at line 1 of CIF file: cell 2 was used but not defined."});
}

TEST_CASE("Top-level geometry goes to the edit cell", "[cap:cif-format-io][req:End of file and top-level geometry]") {
  Reader r;
  CHECK(r.read("L CPG; B 4 4 2 2; 94 t 1 1; E") ==
        Events{"paint 1 rect (0,0)-(4,4)", "label t (1,1)-(1,1) type=4", "end_top"});
}

TEST_CASE("A cancelled read stops without the End message",
          "[cap:cif-format-io][req:End of file and top-level geometry]") {
  Reader r;
  const std::atomic<bool> cancel{true};
  r.options.cancel = &cancel;
  CHECK(r.read("L CMF; B 2 2 1 1; E") == Events{"end_top"});
  CHECK(r.printed().empty());
}

// --- read diagnostics -------------------------------------------------------

namespace {

std::string errors(int n) {
  std::string text;
  for (int i = 0; i < n; ++i) {
    text += "Q;\n";
  }
  return text + "E";
}

}  // namespace

TEST_CASE("Limit level", "[cap:cif-format-io][req:Read diagnostics]") {
  Reader r;
  r.level = hefesto::mask::WarningLevel::limit;
  static_cast<void>(r.read(errors(150)));
  const auto lines = r.printed();
  REQUIRE(lines.size() == 100);
  CHECK(lines[98] == "Error at line 99 of CIF file: unknown command `Q'; ignored.");
  CHECK(lines[99] == "Error limit set:  Remaining errors will not be reported.");
  CHECK(r.session.errors == 150);
}

TEST_CASE("Levels none, default and align", "[cap:cif-format-io][req:Read diagnostics]") {
  Reader none;
  none.level = hefesto::mask::WarningLevel::none;
  static_cast<void>(none.read(errors(5)));
  CHECK(none.printed().empty());
  CHECK(none.session.errors == 5);
  for (const auto level : {hefesto::mask::WarningLevel::warn_default, hefesto::mask::WarningLevel::align}) {
    Reader r;
    r.level = level;
    static_cast<void>(r.read(errors(150)));
    CHECK(r.printed().size() == 150);
  }
}

TEST_CASE("Level redirect follows magic-compat", "[cap:cif-format-io][req:Read diagnostics]") {
  SECTION("magic-compat ON prints on the console") {
    Reader r;
    r.level = hefesto::mask::WarningLevel::redirect;
    static_cast<void>(r.read(errors(2)));
    CHECK(r.printed().size() == 2);
    CHECK(r.redirect_file.snapshot().empty());
  }
  SECTION("magic-compat OFF writes to the redirect file") {
    Reader r;
    r.level = hefesto::mask::WarningLevel::redirect;
    r.options.magic_compat = false;
    static_cast<void>(r.read(errors(2)));
    CHECK(r.printed().empty());
    REQUIRE(r.redirect_file.snapshot().size() == 2);
    CHECK(r.redirect_file.snapshot()[0].text == "Error at line 1 of CIF file: unknown command `Q'; ignored.");
  }
  SECTION("magic-compat OFF without a file prints nothing") {
    Reader r;
    r.level = hefesto::mask::WarningLevel::redirect;
    r.options.magic_compat = false;
    r.use_redirect_file = false;
    static_cast<void>(r.read(errors(2)));
    CHECK(r.printed().empty());
    CHECK(r.session.errors == 2);
  }
}

TEST_CASE("Messages at line 0 use the file prefix", "[cap:cif-format-io][req:Read diagnostics]") {
  hefesto::diag::CollectingSink sink;
  hefesto::cif::ReadMessages messages(hefesto::mask::WarningLevel::warn_default, true, sink);
  messages.error(0, "e.");
  messages.warning(0, "w.");
  const auto out = sink.snapshot();
  REQUIRE(out.size() == 2);
  CHECK(out[0].text == "CIF file read error: e.");
  CHECK(out[0].code == "CIF-READ-001");
  CHECK(out[0].severity == hefesto::diag::Severity::error);
  CHECK(out[1].text == "CIF file read warning: w.");
  CHECK(out[1].severity == hefesto::diag::Severity::warning);
}
