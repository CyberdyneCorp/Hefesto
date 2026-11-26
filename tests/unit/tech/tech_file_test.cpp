// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Capability tech-file-format: lexical rules, sections, registry,
// error recovery, include, repeat, tech/version sections, format gating
// and the annotated example.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/plot/tech/plot_tech.hpp"
#include "hefesto/route/tech/styles.hpp"
#include "hefesto/tech/tech_file.hpp"
#include "tech_fixture.hpp"

namespace tech = hefesto::tech;
namespace platform = hefesto::platform;
using tech::test::load;
using tech::test::load_in;
using tech::test::tiny;
using tech::test::with;
using Tokens = std::vector<std::string>;

namespace {

tech::TechDocument read(const std::string& text, platform::MemoryFileSystem& fs, bool compat = true) {
  hefesto::diag::NullSink sink;
  tech::ReadOptions options;
  options.magic_compat = compat;
  auto doc = tech::read_tech_text(text, "/d/f.tech", fs, "/d", sink, options);
  REQUIRE(doc.ok());
  return doc.value();
}

tech::TechDocument read(const std::string& text) {
  platform::MemoryFileSystem fs;
  return read(text, fs);
}

}  // namespace

TEST_CASE("comment lines are discarded; a mid-line hash is a token; CR is dropped",
          "[cap:tech-file-format][req:Physical line reading and comment lines]") {
  CHECK(tech::tokenize_tech_line("poly # comment") == Tokens{"poly", "#", "comment"});
  const auto doc = read("connect\n    # metal rules follow\r\nm1 m1\r\nend\r\n");
  REQUIRE(doc.sections.size() == 1);
  CHECK(doc.sections[0].name == "connect");
  REQUIRE(doc.sections[0].lines.size() == 1);
  CHECK(doc.sections[0].lines[0].tokens == Tokens{"m1", "m1"});
  CHECK(doc.sections[0].lines[0].where.line == 3);  // comment lines count
  CHECK(doc.sections[0].closed);
  CHECK(read("tech\r\nend\r\n").sections.at(0).name == "tech");
}

TEST_CASE("a backslash at the end joins the next line without a separator",
          "[cap:tech-file-format][req:Line continuation]") {
  auto doc = read("drc\nwidth allDiff 2 \\\n    \"Diffusion width must be at least 2\"\nend\n");
  CHECK(doc.sections.at(0).lines.at(0).tokens ==
        Tokens{"width", "allDiff", "2", "Diffusion width must be at least 2"});
  doc = read("types\nmetal\\\n1\nend\n");
  CHECK(doc.sections.at(0).lines.at(0).tokens == Tokens{"metal1"});
  doc = read("connect\na \\\n# note\nb\nend\n");
  CHECK(doc.sections.at(0).lines.at(0).tokens == Tokens{"a", "b"});
}

TEST_CASE("tokens split on blanks, quotes group, backslash escapes",
          "[cap:tech-file-format][req:Tokenization and quoting]") {
  CHECK(tech::tokenize_tech_line("description \"Minimum technology file structure\"") ==
        Tokens{"description", "Minimum technology file structure"});
  CHECK(tech::tokenize_tech_line("a\\ b \"x\\\"y\" z\"w") == Tokens{"a b", "x\"y", "z\"w"});
  CHECK(tech::tokenize_tech_line("\"a b\"c") == Tokens{"a b", "c"});
  CHECK(tech::tokenize_tech_line("\"\"") == Tokens{""});
  CHECK(tech::tokenize_tech_line("\t x \v y \f") == Tokens{"x", "y"});
}

TEST_CASE("final line without newline, long lines and many tokens",
          "[cap:tech-file-format][req:Line length and token limits]") {
  CHECK(read("tech\nend").sections.at(0).closed);
  platform::MemoryFileSystem fs;
  hefesto::diag::CollectingSink sink;
  const std::string long_line(1500, 'x');
  auto doc = tech::read_tech_text("drc\n" + long_line + "\nend\n", "f", fs, "", sink);
  REQUIRE(doc.ok());
  CHECK(doc.value().sections.at(0).lines.at(0).tokens.at(0).size() == 1023);
  const auto diags = sink.take();
  REQUIRE(!diags.empty());
  CHECK(diags[0].code == "TECH-READ-001");
  CHECK(diags[0].text == "long line truncated");
}

namespace {

// A `drc` section whose one line has `words` words t0, t1, ...
std::string many_words(int words) {
  std::string text = "drc\n";
  for (int i = 0; i < words; ++i) {
    text += "t" + std::to_string(i) + " ";
  }
  return text + "\nend\n";
}

void check_thirty_words(bool compat) {
  platform::MemoryFileSystem fs;
  hefesto::diag::CollectingSink sink;
  tech::ReadOptions options;
  options.magic_compat = compat;
  const auto doc = tech::read_tech_text(many_words(33), "/d/f.tech", fs, "/d", sink, options);
  REQUIRE(doc.ok());
  const auto& tokens = doc.value().sections.at(0).lines.at(0).tokens;
  REQUIRE(tokens.size() == 30);
  CHECK(tokens.back() == "t29");
  const auto diags = sink.take();
  REQUIRE(diags.size() == 1);
  CHECK(diags[0].code == "TECH-READ-002");
  CHECK(diags[0].severity == hefesto::diag::Severity::error);
  CHECK_FALSE(diags[0].native_only);
  CHECK(diags[0].text == "line has more than 30 words; dropped: t30 t31 t32");
}

}  // namespace

// Regression (Q-LEFDEF-139): Magic's techGetTokens overwrites the stack past
// its 30-word vector; both modes keep the first 30 words and report the rest.
TEST_CASE("a line of more than 30 words keeps its first 30 with TECH-READ-002 (magic-compat ON)",
          "[cap:tech-file-format][req:Line length and token limits]") {
  check_thirty_words(true);
}

TEST_CASE("a line of more than 30 words keeps its first 30 with TECH-READ-002 (magic-compat OFF)",
          "[cap:tech-file-format][req:Line length and token limits]") {
  check_thirty_words(false);
}

TEST_CASE("a line of exactly 30 words is read whole without a message",
          "[cap:tech-file-format][req:Line length and token limits]") {
  platform::MemoryFileSystem fs;
  hefesto::diag::CollectingSink sink;
  const auto doc = tech::read_tech_text(many_words(30), "f", fs, "", sink);
  REQUIRE(doc.ok());
  CHECK(doc.value().sections.at(0).lines.at(0).tokens.size() == 30);
  CHECK(sink.take().empty());
}

TEST_CASE("bad headers and stray end skip to the next end", "[cap:tech-file-format][req:Section structure]") {
  auto run = load(with(tiny(), "drc\nend", "drc # rules\nend"));
  CHECK(run.says("Bad section header line"));
  CHECK_FALSE(run.ok());  // drc is then missing
  CHECK(run.says("Section \"drc\" was missing"));

  run = load(with(tiny(), "planes\n", "end\nplanes\n"));
  CHECK(run.says("Unrecognized section name: end"));
  CHECK(run.says("Section \"planes\" was missing"));
  CHECK_FALSE(run.ok());

  run = load(with(tiny(), "planes\n", "end\nplanes\n"), false);  // magic-compat OFF: ignored with a warning
  CHECK(run.has("TECH-SECTION-002"));
  CHECK(run.ok());

  const auto doc = read("drc\nend foo\nend\n");
  CHECK(doc.sections.at(0).lines.at(0).tokens == Tokens{"end", "foo"});
}

TEST_CASE("a repeated section re-runs init with a warning", "[cap:tech-file-format][req:Section structure]") {
  const std::string text = with(tiny(), "aliases\n", "connect\nend\naliases\n");
  auto run = load(with(text, "connect\nend\naliases", "connect\nnwell nwell\nend\naliases"));
  CHECK(run.has("TECH-SECTION-005"));
  CHECK(run.ok());
  run = load(with(tiny(), "drc\nend", "drc\nend\nconnect\nend"), false);
  CHECK(run.has("TECH-SECTION-006"));
  CHECK_FALSE(run.ok());
}

TEST_CASE("prerequisites are checked at the header and required sections at the end",
          "[cap:tech-file-format][req:Section registry and prerequisites]") {
  const std::string text =
      "tech\nt\nend\ntypes\nend\nplanes\nend\nstyles\nend\ncontact\nend\ncompose\nend\nconnect\nend\n"
      "cifoutput\nend\ncifinput\nend\ndrc\nend\nextract\nend\n";
  auto run = load(text);
  CHECK(run.says("Section types appears too early.\n\tMissing prerequisite sections:\n\t\tplanes"));
  CHECK(run.says("Section \"types\" was missing from /t/x.tech."));
  CHECK_FALSE(run.ok());

  // Only the required sections, each empty, in a valid order.
  run = load("tech\nt\nend\nplanes\nend\ntypes\nend\nstyles\nend\ncontact\nend\ncompose\nend\nconnect\nend\n"
             "cifoutput\nend\ncifinput\nend\ndrc\nend\nextract\nend\n");
  CHECK(run.ok());
  CHECK_FALSE(run.has("TECH-SECTION-004"));

  run = load(with(tiny(), "contact\n", "images\n"));
  REQUIRE(run.ok());
  CHECK(run.t().is_contact(tech::test::type_of(run.t(), "pc")));
  CHECK(load(with(tiny(), "planes\n", "Planes\n")).says("Unrecognized section name: Planes"));

  // Every registered section is recognized; optional ones may be omitted.
  for (const char* name : {"version", "aliases", "mzrouter", "lef", "wiring", "router", "plowing", "plot"}) {
    const std::string section = std::string{name} + "\nend\n";
    CHECK_FALSE(load(tiny() + section).says("Unrecognized section name"));
  }
}

TEST_CASE("a rejected line fails the load but reading goes on",
          "[cap:tech-file-format][req:Error reporting and recovery]") {
  auto run = load(with(tiny(), "    m2,m2c       m2,m2c\n", "    m1\n    m1 m2c\n"));
  CHECK(run.says("Line must contain exactly 2 lists of types"));
  CHECK(run.says("The following sections of /t/x.tech contained errors:\n    connect"));
  CHECK_FALSE(run.ok());
  CHECK(run.has("TECH-LOAD-001") == false);  // load_technology returns it; the sink has the details
}

TEST_CASE("include reads files in place, next to the including file, with the suffix",
          "[cap:tech-file-format][req:include directive]") {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/pdk/magic/foo-drc.tech", "drc\nend\n").ok());
  auto run = load_in(fs, with(tiny(), "drc\nend\n", "include foo-drc\n"), true, "/pdk/magic/foo.tech");
  CHECK(run.ok());

  REQUIRE(fs.write_atomic("/pdk/magic/common.inc", "drc\nend\n").ok());
  run = load_in(fs, with(tiny(), "drc\nend\n", "include common.inc\n"), true, "/pdk/magic/bar.tech");
  CHECK(run.says("Couldn't find include file common.inc"));
  CHECK_FALSE(run.ok());  // Q-TECH-2: discarded line, failed load

  run = load_in(fs, with(tiny(), "drc\nend\n", "include common.inc\n"), false, "/pdk/magic/bar.tech");
  CHECK(run.has("TECH-INCLUDE-002"));  // magic-compat OFF: verbatim name found
  CHECK(run.ok());

  REQUIRE(fs.write_atomic("/pdk/magic/common.inc.tech", "drc\nend\n").ok());
  run = load_in(fs, with(tiny(), "drc\nend\n", "include common.inc\n"), true, "/pdk/magic/bar.tech");
  CHECK(run.ok());
}

TEST_CASE("include errors name the included file and its own lines", "[cap:tech-file-format][req:include directive]") {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/p/inc.tech", "connect\n\nm1\nend\n").ok());
  auto run = load_in(fs, with(tiny(), "connect\n", "include inc\nconnect\n"), true, "/p/top.tech");
  bool found = false;
  for (const auto& d : run.diagnostics) {
    if (d.text.find("exactly 2 lists") != std::string::npos) {
      const auto& loc = std::get<hefesto::diag::SourceLocation>(d.location);
      CHECK(loc.file == "/p/inc.tech");
      CHECK(loc.line == 3);
      found = true;
    }
  }
  CHECK(found);
}

TEST_CASE("repeat blocks run N times, once for 0 or a bad count", "[cap:tech-file-format][req:repeat directive]") {
  auto doc = read("connect\nrepeat 3\nm1 m1\nendrepeat\nend\n");
  CHECK(doc.sections.at(0).lines.size() == 3);
  doc = read("connect\nrepeat 0\nX\nendrepeat\nend\n");
  CHECK(doc.sections.at(0).lines.size() == 1);
  platform::MemoryFileSystem fs;
  hefesto::diag::CollectingSink sink;
  auto bad = tech::read_tech_text("connect\nrepeat x\nX\nendrepeat\nend\n", "f", fs, "", sink);
  CHECK(bad.value().sections.at(0).lines.size() == 1);
  CHECK(sink.take().at(0).text == "Error: \"repeat\" with invalid count x");
  // Nested: one counter in magic-compat mode; each block its own with magic-compat OFF.
  const std::string nested = "connect\nrepeat 2\nA\nrepeat 2\nB\nendrepeat\nendrepeat\nend\n";
  CHECK(read(nested).sections.at(0).lines.size() == 3);  // A B B
  platform::MemoryFileSystem fs2;
  CHECK(read(nested, fs2, false).sections.at(0).lines.size() == 6);  // (A B B) x 2
}

// Regression (Q-TECH-33, fuzz_tech_file timeout): `repeat 1111...3`
// saturated to 2^31 - 1 passes over its block.
TEST_CASE("repeat passes share a budget of read lines", "[cap:tech-file-format][req:repeat directive]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    platform::MemoryFileSystem fs;
    hefesto::diag::CollectingSink sink;
    tech::ReadOptions options;
    options.magic_compat = compat;
    const auto doc = tech::read_tech_text("connect\nrepeat 11111111111111111111113\nm1 m1\nendrepeat\nm2 m2\nend\n",
                                          "f", fs, "", sink, options);
    REQUIRE(doc.ok());
    const auto& lines = doc.value().sections.at(0).lines;
    // Each pass reads two lines (m1 m1, endrepeat): the passes up to the
    // budget run, then the rest of the file is read once.
    CHECK(lines.size() == tech::kMaxRepeatLines / 2 + 2);
    CHECK(lines.back().tokens == Tokens{"m2", "m2"});
    CHECK(doc.value().sections.at(0).closed);
    const auto diags = sink.take();
    REQUIRE(diags.size() == 1);
    CHECK(diags.front().code == "TECH-REPEAT-003");
    CHECK(diags.front().text == "\"repeat\" blocks read more than 100000 lines; repeating stops here");
  }
  // Nested blocks multiply with magic-compat OFF; their passes count in the
  // enclosing pass too, so 2000 x 2000 stops early.
  platform::MemoryFileSystem fs;
  hefesto::diag::CollectingSink sink;
  tech::ReadOptions off;
  off.magic_compat = false;
  const auto nested = tech::read_tech_text(
      "connect\nrepeat 2000\nA\nrepeat 2000\nB\nendrepeat\nendrepeat\nC\nend\n", "f", fs, "", sink, off);
  REQUIRE(nested.ok());
  const auto& lines = nested.value().sections.at(0).lines;
  CHECK(lines.size() < tech::kMaxRepeatLines);
  CHECK(lines.back().tokens == Tokens{"C"});
  CHECK(sink.take().size() == 1);
  // A real PDK's use (ihp-sg13g2-cifout.tech: `repeat 32` over two lines) stays far below.
  CHECK(read("connect\nrepeat 32\nA\nB\nendrepeat\nend\n").sections.at(0).lines.size() == 64);
}

TEST_CASE("located load diagnostics name the section they were reported in",
          "[cap:tech-file-format][req:Error reporting and recovery]") {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/p/inc.tech", "connect\n\nm1\nend\n").ok());
  auto run = load_in(fs, with(tiny(), "connect\n", "include inc\nconnect\n"), true, "/p/top.tech");
  bool found = false;
  for (const auto& d : run.diagnostics) {
    if (d.text.find("exactly 2 lists") != std::string::npos) {
      REQUIRE(d.related.has_value());
      CHECK(d.related->kind == hefesto::diag::ObjectKind::section);
      CHECK(d.related->name == "connect");
      found = true;
    }
  }
  CHECK(found);
}

// Regression: line numbers used to restart at the block's first line on every
// repetition; Magic's techLineNumber keeps counting physical reads.
TEST_CASE("line numbers keep increasing across repeat iterations", "[cap:tech-file-format][req:repeat directive]") {
  const auto doc = read("connect\nrepeat 3\nm1 m1\nendrepeat\nend\n");
  const auto& lines = doc.sections.at(0).lines;
  REQUIRE(lines.size() == 3);
  // connect=1 repeat=2 m1=3 endrepeat=4, then m1=5 endrepeat=6, m1=7.
  CHECK(lines[0].where.line == 3);
  CHECK(lines[1].where.line == 5);
  CHECK(lines[2].where.line == 7);
  CHECK(doc.sections.at(0).where.line == 1);
}

// Magic restores the including file's count to the include line when the
// included file ends, and the next read does not count (utils/tech.c:1009-1016),
// so later lines are reported one lower; magic-compat reproduces it.
TEST_CASE("line numbers after an include follow Magic under magic-compat",
          "[cap:tech-file-format][req:include directive]") {
  const std::string top = "connect\ninclude inc\nA B\nC D\nend\n";
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/d/inc.tech", "X Y\n").ok());
  const auto compat = read(top, fs, true);
  const auto& c = compat.sections.at(0).lines;
  REQUIRE(c.size() == 3);
  CHECK(c[0].where.file == "/d/inc.tech");
  CHECK(c[0].where.line == 1);
  CHECK(c[1].where.line == 2);  // physical line 3
  CHECK(c[2].where.line == 3);  // physical line 4
  const auto exact = read(top, fs, false);
  const auto& e = exact.sections.at(0).lines;
  REQUIRE(e.size() == 3);
  CHECK(e[1].where.line == 3);
  CHECK(e[2].where.line == 4);
}

TEST_CASE("the tech section sets the name and the format version", "[cap:tech-file-format][req:tech section]") {
  auto run = load(with(tiny(), "    format 35\n    tiny\n", "    format 35\n    sky130A\n"));
  REQUIRE(run.ok());
  CHECK(run.t().name() == "sky130A");
  CHECK(run.t().format_version() == 35);
  run = load(with(tiny(), "    format 35\n", ""));
  CHECK(run.t().format_version() == 27);
  run = load(with(tiny(), "    tiny\n", "    my tech name\n"));
  CHECK(run.says("Badly formed technology name"));
  CHECK_FALSE(run.ok());
  run = load(with(tiny(), "    format 35\n", "    formatted 30\n"));
  CHECK(run.t().format_version() == 30);
  run = load(with(tiny(), "    format 35\n", "    format abc\n"));
  CHECK(run.says("Bad format version number. . . assuming 27"));
  CHECK(run.ok());
  run = load(with(tiny(), "    format 35\n", "    formatX 40\n"), false);
  CHECK_FALSE(run.ok());  // magic-compat OFF: only exact keywords
  run = load(with(tiny(), "    format 35\n    tiny\n", "    format 35\n"));
  CHECK(run.has("TECH-NAME-001"));  // Q-TECH-8
  CHECK_FALSE(run.ok());
}

TEST_CASE("the version section stores version and description; requires is checked",
          "[cap:tech-file-format][req:version section]") {
  auto run = load(with(tiny(), "    version 1.0\n", "    version 1.0 beta\n    requires magic-8.2.100\n"));
  REQUIRE(run.ok());
  CHECK(run.t().version().version_lines.back() == "1.0\nbeta");
  CHECK(run.t().version().description_lines.back() == "Tiny demo process");
  run = load(with(tiny(), "    version 1.0\n", "    requires 8.3.700\n"));
  CHECK(run.says("Error:  Magic version 8.3.700 is required by this techfile, but this version of magic is "
                 "8.3.694."));
  CHECK_FALSE(run.ok());
  run = load(with(tiny(), "    version 1.0\n", "    requires 8.3\n"));
  CHECK(run.says("Badly formed magic version string, should be major.minor.rev"));
  run = load(with(tiny(), "    version 1.0\n", "    bogus x\n"));
  CHECK(run.says("Badly formed version line"));
  // A technology without a version section has empty strings (Q-TECH-8).
  const auto bare = load(tech::test::skeleton("", ""));
  REQUIRE(bare.ok());
  CHECK(bare.t().version().version_lines.empty());
}

TEST_CASE("the format version selects the fringe-halo default", "[cap:tech-file-format][req:Format version gating]") {
  CHECK(load(with(tiny(), "format 35", "format 34")).t().fringe_halo_default());
  CHECK_FALSE(load(with(tiny(), "format 35", "format 33")).t().fringe_halo_default());
  // A tech section after extract: Magic decides at extract's end (compat), Hefesto OFF at the end of the file.
  std::string late = with(tiny(), "tech\n    format 35\n    tiny\nend\n", "");
  late += "tech\n    format 35\n    tiny\nend\n";
  CHECK_FALSE(load(late).t().fringe_halo_default());
  CHECK(load(late, false).t().fringe_halo_default());
}

TEST_CASE("legacy .tech27 include suffix", "[cap:tech-file-format][req:include directive]") {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/o/old-drc.tech27", "drc\nend\n").ok());
  auto run = load_in(fs, with(tiny(), "drc\nend\n", "include old-drc\n"), true, "/o/old.tech27");
  CHECK(run.ok());
}

TEST_CASE("the annotated example loads with the stated results",
          "[cap:tech-file-format][req:Complete annotated example]") {
  const auto run = load(tiny());
  REQUIRE(run.ok());
  const auto& t = run.t();
  CHECK(t.name() == "tiny");
  CHECK(t.format_version() == 35);
  CHECK(t.version().version_lines.back() == "1.0");
  CHECK(t.version().description_lines.back() == "Tiny demo process");
  const std::vector<std::string> planes = {"router", "designRuleCheck", "designRuleError", "mhint", "fhint",
                                           "rhint",  "well",            "active",          "metal1", "metal2"};
  REQUIRE(t.plane_count() == planes.size());
  for (std::size_t p = 0; p < planes.size(); ++p) {
    CHECK(t.plane_name(static_cast<tech::PlaneId>(p)) == planes[p]);
  }
  const std::vector<std::string> types = {"nwell",  "polysilicon", "ndiffusion", "ntransistor", "metal1",
                                          "metal2", "polycontact", "ndcontact",  "m2contact",   "pwell",
                                          "pc+m2c"};
  REQUIRE(t.type_count() == 9 + types.size());
  for (std::size_t i = 0; i < types.size(); ++i) {
    CHECK(t.type_name(static_cast<tech::TileType>(9 + i)) == types[i]);
  }
  CHECK(t.user_type_end() == 19);
  CHECK(t.locked_by_default(18));
  CHECK(*t.find_type("pm12c").id == 19);
  // The `#` in the first, invalid tech section is a token: 6 tokens.
  CHECK(load(with(tiny(), "    format 35\n", "    format 35                     # sets format version 35\n"))
            .says("Badly formed technology name"));
}

TEST_CASE("the plot section is kept raw and parsed for plotting",
          "[cap:tech-file-format][req:Section registry and prerequisites]") {
  // M10 plot/route (tasks.md 4.3): the loader keeps the raw `plot` section
  // as before and hands it to the plot-output reader (Technology::plot()).
  const std::string section = "plot\n  style postscript\n    10 0 0 0 255\n    m1 10 S\n  style pnm\n    draw m1\nend\n";
  auto run = load(tiny() + section);
  REQUIRE(run.ok());
  bool raw_kept = false;
  for (const auto& raw : run.t().raw_sections()) {
    raw_kept = raw_kept || (raw.name == "plot" && raw.lines.size() == 5);
  }
  CHECK(raw_kept);
  const auto& plot = run.t().plot();
  CHECK(plot.has_section);
  REQUIRE(plot.ps_colors.size() == 1);
  CHECK(plot.ps_colors[0].index == 10);
  REQUIRE(plot.ps_styles.size() == 1);
  CHECK(plot.ps_styles[0].types.test(tech::test::type_of(run.t(), "metal1")));
  CHECK(plot.pnm_paint.size() == run.t().user_type_end());

  auto none = load(tiny());
  REQUIRE(none.ok());
  CHECK_FALSE(none.t().plot().has_section);
  CHECK(none.t().maze_styles().styles.empty());
}
