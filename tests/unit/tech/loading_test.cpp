// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Capabilities tech-loading-and-scaling (file search, name resolution,
// partial reload, built-in technologies) and tech-styles-section (the raw
// styles section), plus the real technology files when available.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

#include "hefesto/tech/tech_file.hpp"
#include "reference_data.hpp"
#include "tech_fixture.hpp"

namespace tech = hefesto::tech;
namespace platform = hefesto::platform;
using tech::test::load;
using tech::test::load_in;
using tech::test::mask_of;
using tech::test::tiny;
using tech::test::with;

namespace {

std::string resolved(platform::FileSystem& fs, std::string_view name, std::string* suffix = nullptr) {
  tech::LoadOptions options;
  options.search_path = {"/sys"};
  auto path = tech::resolve_tech_file(fs, name, options, suffix);
  return path ? path.value().string() : "error: " + path.error().text;
}

}  // namespace

TEST_CASE("file search: direct names, then . and the system path", "[cap:tech-loading-and-scaling][req:File search rules]") {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("./tiny.tech", "x").ok());
  REQUIRE(fs.write_atomic("/sys/tiny.tech", "x").ok());
  REQUIRE(fs.write_atomic("/sys/other.tech", "x").ok());
  CHECK(resolved(fs, "tiny") == "tiny.tech");  // PaOpen's "." entry adds no "./"
  CHECK(resolved(fs, "other") == "/sys/other.tech");
  CHECK(resolved(fs, "/sys/tiny") == "/sys/tiny.tech");
  CHECK(resolved(fs, "/abs/tiny").starts_with("error:"));
  tech::SearchOptions search;
  search.expand = [](std::string_view name) { return std::optional<std::string>{"/sys" + std::string{name.substr(1)}}; };
  CHECK(tech::find_file(fs, "~/other", ".tech", search) == platform::Path{"/sys/other.tech"});
}

TEST_CASE("technology file name resolution", "[cap:tech-loading-and-scaling][req:Technology file name resolution]") {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/sys/SCN4M_SUBM.20.tech", "x").ok());
  REQUIRE(fs.write_atomic("/sys/tiny.tech", "x").ok());
  REQUIRE(fs.write_atomic("/sys/old.tech27", "x").ok());
  CHECK(resolved(fs, "SCN4M_SUBM.20") == "/sys/SCN4M_SUBM.20.tech");
  CHECK(resolved(fs, "tiny.tech") == "/sys/tiny.tech");
  std::string suffix;
  CHECK(resolved(fs, "old", &suffix) == "/sys/old.tech27");
  CHECK(suffix == ".tech27");
  CHECK(resolved(fs, "nothere") ==
        "error: Could not find file 'nothere.tech' in any of these directories:\n         . /sys");
  REQUIRE(fs.write_atomic("/sys/SCN4M_SUBM.20", "x").ok());
  CHECK(resolved(fs, "SCN4M_SUBM.20") == "/sys/SCN4M_SUBM.20");  // verbatim first for other extensions
}

TEST_CASE("readability pre-check", "[cap:tech-loading-and-scaling][req:Readability pre-check]") {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/a.tech", "# c\n\n  tech\nend\n").ok());
  REQUIRE(fs.write_atomic("/b.tech", "version\nend\ntech\nend\n").ok());
  REQUIRE(fs.write_atomic("/c.tech", "te\\\nch\n").ok());
  CHECK(tech::precheck_tech_file(fs, "/a.tech").ok());
  CHECK(tech::precheck_tech_file(fs, "/b.tech").error().text == "Technology file does not exist or is not readable.");
  CHECK(tech::precheck_tech_file(fs, "/c.tech").ok());
  CHECK_FALSE(tech::precheck_tech_file(fs, "/missing.tech").ok());
}

TEST_CASE("full loads start from a clean state", "[cap:tech-loading-and-scaling][req:Full technology load]") {
  const auto run = load(with(tiny(), "connect\n", "connectx\n"));
  CHECK_FALSE(run.ok());
  CHECK(run.says("Section \"connect\" was missing from /t/x.tech."));
  // Raw sections are kept for later milestones.
  const auto good = load(with(tiny(), "drc\nend", "drc\nwidth m1 3 \"m1 width\"\nend"));
  REQUIRE(good.ok());
  bool found = false;
  for (const auto& raw : good.t().raw_sections()) {
    if (raw.name == "drc") {
      REQUIRE(raw.lines.size() == 1);
      CHECK(raw.lines[0].tokens == std::vector<std::string>{"width", "m1", "3", "m1 width"});
      found = true;
    }
  }
  CHECK(found);
}

TEST_CASE("partial reload re-reads only the selected sections", "[cap:tech-loading-and-scaling][req:Partial section reload]") {
  platform::MemoryFileSystem fs;
  const auto first = load_in(fs, tiny(), true, "/t/p.tech");
  REQUIRE(first.ok());
  // Edit the file on disk: the drc section changes, and a broken planes line appears.
  REQUIRE(fs.write_atomic("/t/p.tech", with(with(tiny(), "drc\nend", "drc\nwidth m1 4 \"w\"\nend"), "    well,w\n",
                                            "    well,w,well\n"))
              .ok());
  hefesto::diag::CollectingSink sink;
  auto reloaded = tech::reload_sections(fs, first.t(), {"drc"}, tech::LoadOptions{}, sink);
  REQUIRE(reloaded.ok());
  const auto& t = *reloaded.value();
  CHECK(t.plane_count() == first.t().plane_count());
  CHECK(t.paint_result(7, 10, 11) == 12);
  std::size_t drc_sections = 0;
  for (const auto& raw : t.raw_sections()) {
    if (raw.name == "drc") {
      ++drc_sections;
      CHECK(raw.lines.at(0).tokens.at(2) == "4");
    }
  }
  CHECK(drc_sections == 1);
  // A selected section missing from the file fails the reload.
  REQUIRE(fs.write_atomic("/t/p.tech", with(tiny(), "drc\nend\n", "")).ok());
  CHECK_FALSE(tech::reload_sections(fs, first.t(), {"drc"}, tech::LoadOptions{}, sink).ok());
  CHECK(tech::reload_sections(fs, *tech::builtin_technology(), {"drc"}, tech::LoadOptions{}, sink).error().text ==
        "Invalid technology file load.");
}

TEST_CASE("the built-in minimum technology", "[cap:tech-loading-and-scaling][req:Built-in technologies]") {
  const auto t = tech::builtin_technology();
  CHECK(t->name() == "minimum");
  CHECK(t->plane_count() == 6);
  CHECK(t->user_type_end() == 9);
  CHECK(t->type_count() == 9);
  CHECK(t->file_name().empty());
  CHECK(t->format_version() == 28);
  CHECK(t->styles().style_family == "mos");
  CHECK(t->version().description_lines.back() == "Minimum technology file structure");
  CHECK(tech::Technology{}.name() == "minimum");
  // Magic's own minimum.tech.in loads the same way.
  platform::LocalFileSystem local;
  const std::string magic_minimum = std::string{HEFESTO_MAGIC_REF_DIR} + "/scmos/minimum.tech.in";
  if (local.exists(magic_minimum)) {
    hefesto::diag::CollectingSink sink;
    auto loaded = tech::load_technology(local, magic_minimum, tech::LoadOptions{}, sink);
    REQUIRE(loaded.ok());
    CHECK(loaded.value()->name() == "minimum");
    CHECK(loaded.value()->plane_count() == 6);
  }
}

TEST_CASE("styles section syntax", "[cap:tech-styles-section][req:styles section syntax]") {
  const auto t = tech::test::tiny_tech();
  const auto& styles = t->styles();
  REQUIRE(styles.lines.size() == 6);
  CHECK(styles.lines[2].type == 12);
  CHECK(styles.lines[2].refs == std::vector<std::string>{"6", "7"});
  auto run = load(with(tiny(), "    poly 1\n", "    nosuchlayer 3\n"));
  CHECK(run.says("Unrecognized layer (type) name \"nosuchlayer\""));
  CHECK_FALSE(run.ok());
  run = load(with(tiny(), "    poly 1\n", "    poly\n"));
  CHECK(run.says("Badly formed line in \"style\" section"));
  CHECK(run.ok());
}

TEST_CASE("styletype selects the family and resets the mapping", "[cap:tech-styles-section][req:styletype statement]") {
  const auto t = tech::test::tiny_tech();
  CHECK(t->styles().style_family == "mos");
  const std::string long_family(60, 'f');
  auto run = load(with(tiny(), "    styletype mos\n", "    poly 9\n    styletype " + long_family + " /d1 /d2\n"));
  REQUIRE(run.ok());  // Q-TECH-24: no display files needed
  CHECK(run.t().styles().style_family.size() == 49);
  CHECK(run.t().styles().search_dirs == std::vector<std::string>{"/d1", "/d2"});
  CHECK(run.t().styles().lines.size() == 6);  // the poly 9 line before styletype was cleared
  // A second styles section starts from an empty mapping (Q-TECH-24).
  run = load(with(tiny(), "compose\n", "styles\n    metal1 5\nend\ncompose\n"));
  REQUIRE(run.ok());
  CHECK(run.t().styles().lines.size() == 1);
  CHECK(run.t().styles().style_family.empty());
}

TEST_CASE("type-to-style references are kept raw with their lines", "[cap:tech-styles-section][req:Type to style mapping]") {
  const auto run = load(with(tiny(), "    poly 1\n", "    poly 1 nosuchstyle 20\n"));
  REQUIRE(run.ok());
  const auto& line = run.t().styles().lines.at(0);
  CHECK(line.refs == std::vector<std::string>{"1", "nosuchstyle", "20"});
  CHECK(line.line > 0);
}

TEST_CASE("stacked types inherit the styles of their same-plane component",
          "[cap:tech-styles-section][req:Contact style propagation]") {
  const auto run = load(with(tiny(), "    pc 1 20 32\n", "    pc 1 20 32\n    m2c 20 21 33\n"));
  REQUIRE(run.ok());
  const auto& lines = run.t().styles().lines;
  CHECK(lines.at(5).type == 15);
  CHECK(lines.at(5).stacked.empty());  // pc's home plane is active, pc+m2c's is metal1
  CHECK(lines.at(6).type == 17);
  CHECK(lines.at(6).stacked == mask_of({19}));
}

TEST_CASE("Magic's scmos technology loads", "[cap:tech-compose-and-connect][req:Compose finalization order]") {
  platform::LocalFileSystem local;
  if (!local.exists(HEFESTO_SCMOS_TECH)) {
    SKIP("scmos.tech not generated (no build/ref/magic tree)");
  }
  hefesto::diag::CollectingSink sink;
  auto loaded = tech::load_technology(local, HEFESTO_SCMOS_TECH, tech::LoadOptions{}, sink);
  for (const auto& d : sink.snapshot()) {
    if (d.severity == hefesto::diag::Severity::error) {
      UNSCOPED_INFO(d.code << ": " << d.text);
    }
  }
  REQUIRE(loaded.ok());
  const auto& t = *loaded.value();
  CHECK(t.name() == "scmos");
  const auto pdc = tech::test::type_of(t, "pdc");
  const auto ndc = tech::test::type_of(t, "ndc");
  const auto pwell = tech::test::type_of(t, "pwell");
  CHECK(t.is_contact(pdc));
  CHECK(t.paint_result(*t.home_plane(pdc), pdc, pwell) == ndc);  // paint pdc pwell ndc
  CHECK(t.connects_to(tech::test::type_of(t, "m1")).test(pdc));
}

namespace {

std::optional<std::filesystem::path> sky130_tech() {
  return hefesto::test::pinned_sky130a_tech();  // the exact pinned version only
}

}  // namespace

TEST_CASE("the pinned sky130A technology loads", "[cap:tech-loading-and-scaling][req:Full technology load]") {
  const auto file = sky130_tech();
  if (!file) {
    SKIP("sky130A.tech not in .cache/pdks");
  }
  platform::LocalFileSystem local;
  hefesto::diag::CollectingSink sink;
  auto loaded = tech::load_technology(local, file->string(), tech::LoadOptions{}, sink);
  for (const auto& d : sink.snapshot()) {
    if (d.severity == hefesto::diag::Severity::error) {
      UNSCOPED_INFO(d.code << ": " << d.text);
    }
  }
  REQUIRE(loaded.ok());
  const auto& t = *loaded.value();
  CHECK(t.name() == "sky130A");
  CHECK(t.format_version() >= 34);
  CHECK(t.fringe_halo_default());
  const auto m1 = tech::test::type_of(t, "metal1");
  const auto via1 = tech::test::type_of(t, "via1");
  CHECK(t.is_contact(via1));
  CHECK(t.connects_to(m1).test(via1));
  CHECK(t.type_count() > t.user_type_end());  // stacked contacts were generated
  CHECK(t.styles().style_family == "mos");  // sky130A.tech line 431
  REQUIRE(t.cif_output_grid().has_value());
  CHECK(t.cif_output_grid()->scale_factor > 0);
}
