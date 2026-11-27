// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The technology `plot` section (capability plot-output; openspec/changes/
// m10-plot-route tasks.md 4.1, 4.2): the section structure, the postscript
// and pnm style lines, the legacy devices and the PNM colour sources. The
// message texts are Magic 8.3.694's (plot/plotMain.c, plotPS.c, plotPNM.c),
// the MISC-PLOT-20 tech-load messages among them.
//
// Owner: PLOTTECH.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/plot/tech/plot_tech.hpp"
#include "hefesto/tech/loader.hpp"
#include "plot/tech/reader.hpp"
#include "reference_data.hpp"

namespace diag = hefesto::diag;
namespace display = hefesto::display;
namespace plot = hefesto::plot;
namespace platform = hefesto::platform;
namespace tech = hefesto::tech;

namespace {

constexpr std::string_view kBase = R"(tech
    format 35
    plottest
end
planes
    active,a
    metal1,m1
    metal2,m2
end
types
    active  polysilicon,poly
    active  ndiffusion,ndiff
    metal1  metal1,m1
    metal2  metal2,m2
    active  polycontact,pc
    metal1  m2contact,m2c
end
contact
    pc   poly  metal1
    m2c  metal1 metal2
end
styles
    styletype mos
    poly 1
    ndiff 2
    metal1 20
    metal2 21
    pc 1 20 32
end
compose
end
connect
end
cifoutput
end
cifinput
end
drc
end
extract
end
)";

struct Loaded {
  std::shared_ptr<const tech::Technology> tech;
  std::vector<diag::Diagnostic> messages;
};

Loaded load(std::string_view plot_section, bool magic_compat = true, platform::MemoryFileSystem* files = nullptr) {
  platform::MemoryFileSystem own;
  platform::MemoryFileSystem& fs = files != nullptr ? *files : own;
  diag::CollectingSink sink;
  tech::LoadOptions options;
  options.magic_compat = magic_compat;
  const std::string text = std::string{kBase} + std::string{plot_section};
  auto loaded = tech::load_technology_text(text, "/t/plottest.tech", fs, options, sink);
  REQUIRE(loaded.ok());
  return Loaded{loaded.value(), sink.take()};
}

std::vector<std::string> texts(const std::vector<diag::Diagnostic>& messages) {
  std::vector<std::string> out;
  for (const auto& d : messages) {
    out.push_back(d.text);
  }
  return out;
}

tech::TileType type(const tech::Technology& t, std::string_view name) {
  const auto found = t.find_type(name);
  REQUIRE(found.ok());
  return *found.id;
}

tech::TypeMask mask_of(const tech::Technology& t, std::initializer_list<std::string_view> names) {
  tech::TypeMask mask;
  for (const auto name : names) {
    mask.set(type(t, name));
  }
  return mask;
}

/// A session display style set: three technology styles (blue, red,
/// green) after the 52 fixed slots; poly is drawn by blue then red, metal1
/// by green, and `fixed_name` names fixed slot 10.
display::StyleSet session_styles(const tech::Technology& t) {
  display::StyleSet set;
  set.display_class = display::DisplayClass::bit7;
  set.colors.entries = {{0, 0, 0, "black"}, {0, 0, 255, "blue"}, {255, 0, 0, "red"}, {0, 255, 0, "green"}};
  set.table.styles.resize(display::kFixedStyleCount + 3);
  set.table.styles[10].long_name = "fixed_name";
  set.table.styles[10].mask = 0x40;
  set.table.styles[10].color = 3;
  const std::array<std::pair<int, const char*>, 3> layout = {{{1, "blue_style"}, {2, "red_style"}, {3, "green_style"}}};
  for (std::size_t j = 0; j < layout.size(); ++j) {
    auto& s = set.table.styles[display::kFixedStyleCount + j];
    s.mask = 1 << j;
    s.color = layout[j].first;
    s.long_name = layout[j].second;
  }
  set.table.layout_count = 3;
  set.types.styles_of_type.resize(t.type_count());
  set.types.styles_of_type[type(t, "poly")] = {display::kFixedStyleCount, display::kFixedStyleCount + 1};
  set.types.styles_of_type[type(t, "metal1")] = {display::kFixedStyleCount + 2};
  return set;
}

/// The technology's last `plot` section read again with `source`.
plot::PlotTech reread(const tech::Technology& t, const plot::StyleSource& source,
                      std::vector<diag::Diagnostic>* messages = nullptr) {
  const tech::RawSection* section = nullptr;
  for (const auto& raw : t.raw_sections()) {
    if (raw.name == "plot") {
      section = &raw;
    }
  }
  diag::CollectingSink sink;
  plot::PlotTech out = plot::read_plot_section(section, t, source, sink);
  if (messages != nullptr) {
    *messages = sink.take();
  }
  return out;
}

std::string asset(std::string_view name) {
  std::ifstream in(std::string{HEFESTO_SOURCE_DIR} + "/assets/plot/" + std::string{name}, std::ios::binary);
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

}  // namespace

// --- Technology plot section structure -----------------------------------

TEST_CASE("unknown plot style skipped", "[cap:plot-output][req:Technology plot section structure]") {
  const Loaded l = load("plot\n  style laser\n  m1 1 S\n  style postscript\n  m1 1 S\nend\n");
  CHECK(texts(l.messages) == std::vector<std::string>{"Plot style \"laser\" doesn't exist.  Ignoring."});
  const plot::PlotTech& p = l.tech->plot();
  CHECK(p.has_section);
  REQUIRE(p.ps_styles.size() == 1);
  CHECK(p.ps_styles[0].types == mask_of(*l.tech, {"metal1"}));
}

TEST_CASE("plot section structure errors use Magic's texts",
          "[cap:plot-output][req:Technology plot section structure]") {
  // MISC-PLOT-20: a three-field style line, a line before any style.
  const Loaded l = load("plot\n  m1 1 S\n  style postscript extra\n  m2 1 S\n  style postscript\n  m2 2 S\nend\n");
  CHECK(texts(l.messages) == std::vector<std::string>{"Must declare a plot style before anything else.",
                                                      "\"style\" lines must have exactly two arguments"});
  // The line before any style and the line after the bad style line (no
  // style yet: skipping) are ignored; only the last line is kept.
  REQUIRE(l.tech->plot().ps_styles.size() == 1);
  CHECK(l.tech->plot().ps_styles[0].color == 2);
  for (const auto& d : l.messages) {
    REQUIRE(d.related.has_value());
    CHECK(d.related->name == "plot");  // printed with TechPrintLine's context
  }
}

TEST_CASE("a bad style line keeps the current style", "[cap:plot-output][req:Technology plot section structure]") {
  const Loaded l = load("plot\n  style postscript\n  style\n  m1 1 S\nend\n");
  CHECK(texts(l.messages) == std::vector<std::string>{"\"style\" lines must have exactly two arguments"});
  CHECK(l.tech->plot().ps_styles.size() == 1);
}

TEST_CASE("no plot section", "[cap:plot-output][req:Technology plot section structure]") {
  const Loaded l = load("");
  CHECK(l.messages.empty());
  const plot::PlotTech& p = l.tech->plot();
  CHECK_FALSE(p.has_section);
  CHECK(p.pnm_paint.empty());
  CHECK(p.ps_styles.empty());
}

TEST_CASE("the last plot section is read", "[cap:plot-output][req:Technology plot section structure]") {
  const Loaded l =
      load("plot\n  style postscript\n  m1 1 S\n  m2 2 S\nend\nplot\n  style postscript\n  poly 3 S\nend\n");
  REQUIRE(l.tech->plot().ps_styles.size() == 1);
  CHECK(l.tech->plot().ps_styles[0].color == 3);
}

// --- PostScript style lines ---------------------------------------------------

TEST_CASE("postscript lists in reverse order", "[cap:plot-output][req:PostScript style lines]") {
  const Loaded l = load(
      "plot\n style postscript\n"
      " 5 C3C3C3C3 3C3C3C3C C3C3C3C3 3C3C3C3C C3C3C3C3 3C3C3C3C C3C3C3C3 3C3C3C3C\n"
      " 10 0 0 0 255\n 12 200 0 50 0\n poly 12 5\n m1 10 S\n pc,m2c 10 X\nend\n");
  CHECK(l.messages.empty());
  const plot::PlotTech& p = l.tech->plot();
  REQUIRE(p.ps_colors.size() == 2);
  CHECK(p.ps_colors[0].index == 12);
  CHECK(p.ps_colors[0].cmyk == std::array<std::uint8_t, 4>{200, 0, 50, 0});
  CHECK(p.ps_colors[1].index == 10);
  REQUIRE(p.ps_patterns.size() == 1);
  CHECK(p.ps_patterns[0].index == 5);
  CHECK(p.ps_patterns[0].rows[0] == 0xC3C3C3C3U);
  CHECK(p.ps_patterns[0].rows[1] == 0x3C3C3C3CU);
  REQUIRE(p.ps_styles.size() == 3);
  CHECK(p.ps_styles[0].types == mask_of(*l.tech, {"pc", "m2c"}));
  CHECK(p.ps_styles[0].stipple == plot::kPsCross);
  CHECK(p.ps_styles[1].types == mask_of(*l.tech, {"metal1"}));
  CHECK(p.ps_styles[1].stipple == plot::kPsSolid);
  CHECK(p.ps_styles[2].types == mask_of(*l.tech, {"poly"}));
  CHECK(p.ps_styles[2].stipple == 5);
}

TEST_CASE("postscript line errors use Magic's texts", "[cap:plot-output][req:PostScript style lines]") {
  // MISC-PLOT-20: a four-field line, a non-integer second field, a bad fill.
  const Loaded l = load("plot\n style postscript\n m1 1 S x\n m1 x S\n m1 1 Q\n m1 1 B\nend\n");
  CHECK(texts(l.messages) ==
        std::vector<std::string>{"\"ps\" lines must have either 9, 5, or 3 arguments.", "2nd field must be an integer",
                                 "3rd field must be an integer or \"S\", \"X\", or \"B\"."});
  REQUIRE(l.tech->plot().ps_styles.size() == 1);
  CHECK(l.tech->plot().ps_styles[0].stipple == plot::kPsBorder);
}

TEST_CASE("postscript colour components keep their low eight bits", "[cap:plot-output][req:PostScript style lines]") {
  const Loaded l = load("plot\n style postscript\n 3 300 -1 256 7x\nend\n");
  CHECK(l.messages.empty());
  REQUIRE(l.tech->plot().ps_colors.size() == 1);
  CHECK(l.tech->plot().ps_colors[0].cmyk == std::array<std::uint8_t, 4>{44, 255, 0, 7});
}

TEST_CASE("postscript layer types are restricted to user types", "[cap:plot-output][req:PostScript style lines]") {
  const Loaded l = load("plot\n style postscript\n *metal1,nosuch 1 S\nend\n");
  REQUIRE(l.tech->plot().ps_styles.size() == 1);
  const tech::TypeMask types = l.tech->plot().ps_styles[0].types;
  CHECK(types.test(type(*l.tech, "metal1")));
  for (tech::TileType t = 0; t < tech::kFirstUserType; ++t) {
    CHECK_FALSE(types.test(t));
  }
  // DBTechNoisyNameMask reports the unknown name; the line is kept.
  CHECK(texts(l.messages) == std::vector<std::string>{"Unrecognized layer (type) name \"nosuch\""});
}

TEST_CASE("non-numeric postscript fields are reported and ignored in both modes",
          "[cap:plot-output][req:PostScript style lines]") {
  // Q-MISC-50 (2), fixed in both modes: Magic stores an indeterminate value.
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    const Loaded l = load(
        "plot\n style postscript\n x 1 2 3 4\n 4 1 2 z 4\n"
        " q 0 0 0 0 0 0 0 0\n 6 FF FF FF FF FF FF FF zz\n 7 1 2 3 4\nend\n",
        compat);
    CHECK(texts(l.messages) == std::vector<std::string>{
                                   "\"ps\" line field \"x\" is not an integer", "\"ps\" line field \"z\" is not an integer",
                                   "\"ps\" line field \"q\" is not an integer", "\"ps\" line field \"zz\" is not an integer"});
    REQUIRE(l.tech->plot().ps_colors.size() == 1);
    CHECK(l.tech->plot().ps_colors[0].index == 7);
    CHECK(l.tech->plot().ps_patterns.empty());
  }
}

// --- PNM style lines ------------------------------------------------------------

TEST_CASE("blending two display styles", "[cap:plot-output][req:PNM style lines]") {
  const Loaded l = load("plot\n style pnm\n draw poly\nend\n");
  const display::StyleSet set = session_styles(*l.tech);
  plot::StyleSource source;
  source.session = &set;
  const plot::PlotTech p = reread(*l.tech, source);
  const auto poly = type(*l.tech, "poly");
  REQUIRE(p.pnm_paint.size() > poly);
  CHECK(p.pnm_paint[poly].color == plot::Rgb{128, 0, 0});
  CHECK(p.pnm_paint[poly].wmask == 3);
  // Only poly was drawn, so `default` is not applied at the end.
  CHECK(p.pnm_paint[type(*l.tech, "metal1")] == plot::PnmPaint{});
}

TEST_CASE("default applies when no type has a colour", "[cap:plot-output][req:PNM style lines]") {
  const Loaded l = load("plot\n style pnm\n nothing\n draw nosuch\nend\n");
  const display::StyleSet set = session_styles(*l.tech);
  plot::StyleSource source;
  source.session = &set;
  std::vector<diag::Diagnostic> messages;
  const plot::PlotTech p = reread(*l.tech, source, &messages);
  CHECK(messages.empty());  // unknown keyword and `draw <unknown>` are silent, as Magic
  CHECK(p.pnm_paint[type(*l.tech, "poly")].color == plot::Rgb{128, 0, 0});
  CHECK(p.pnm_paint[type(*l.tech, "metal1")].color == plot::Rgb{0, 255, 0});
  CHECK(p.pnm_paint[type(*l.tech, "metal1")].wmask == 4);
  CHECK(p.pnm_paint[type(*l.tech, "metal2")] == plot::PnmPaint{});
}

TEST_CASE("draw with a display style by name", "[cap:plot-output][req:PNM style lines]") {
  const Loaded l = load(
      "plot\n style pnm\n draw poly\n draw poly fixed_name\n draw metal1 nosuch\n draw nosuch red_style\nend\n");
  const display::StyleSet set = session_styles(*l.tech);
  plot::StyleSource source;
  source.session = &set;
  std::vector<diag::Diagnostic> messages;
  const plot::PlotTech p = reread(*l.tech, source, &messages);
  CHECK(texts(messages) == std::vector<std::string>{"Unknown drawing style \"nosuch\" for PNM plot.",
                                                    "Unknown magic layer \"nosuch\" for PNM plot."});
  for (const auto& d : messages) {
    CHECK(std::holds_alternative<std::monostate>(d.location));  // TxError: no line context
  }
  // The named style replaces poly's colour; metal1 keeps its (white) colour.
  CHECK(p.pnm_paint[type(*l.tech, "poly")] == plot::PnmPaint{0x40, plot::Rgb{0, 255, 0}});
  CHECK(p.pnm_paint[type(*l.tech, "metal1")] == plot::PnmPaint{});
}

TEST_CASE("map blends the colours of other types", "[cap:plot-output][req:PNM style lines]") {
  const Loaded l = load("plot\n style pnm\n draw poly\n draw metal1\n map pc poly metal1 nosuch\nend\n");
  const display::StyleSet set = session_styles(*l.tech);
  plot::StyleSource source;
  source.session = &set;
  const plot::PlotTech p = reread(*l.tech, source);
  const plot::Rgb expected = plot::Rgb{0, 128, 0};  // white, then (128,0,0) -> (128,0,0), then (0,255,0)
  CHECK(p.pnm_paint[type(*l.tech, "pc")].color == expected);
  CHECK(p.pnm_paint[type(*l.tech, "pc")].wmask == 7);
}

TEST_CASE("a bare map line is reported and ignored in both modes", "[cap:plot-output][req:PNM style lines]") {
  // Q-MISC-50 (1), fixed in both modes: Magic dereferences argv[1].
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    const Loaded l = load("plot\n style pnm\n map\n draw poly\nend\n", compat);
    CHECK(texts(l.messages) == std::vector<std::string>{"\"map\" lines need at least one type"});
    REQUIRE(l.messages.size() == 1);
    CHECK(l.messages[0].related.has_value());
  }
}

TEST_CASE("regression: map and draw skip types outside the user types", "[cap:plot-output][req:PNM style lines]") {
  // Magic's `map` checks only i >= 0 for a source type and reads
  // PaintStyles (DBNumUserLayers entries) out of bounds for a stacked
  // contact; Hefesto skips such types in both modes, as targets too.
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    // The test technology with pc and m2c stackable (a stacked pc+m2c type).
    const auto with_stacking = [&](const std::string& lines) {
      std::string text{kBase};
      const std::string contacts = "    m2c  metal1 metal2\nend\n";
      const auto at = text.find(contacts);
      REQUIRE(at != std::string::npos);
      text.insert(at + contacts.size() - 4, "    stackable pc m2c\n");
      text += "plot\n style pnm\n" + lines + "end\n";
      diag::CollectingSink sink;
      tech::LoadOptions options;
      options.magic_compat = compat;
      platform::MemoryFileSystem fs;
      auto loaded = tech::load_technology_text(text, "/t/plottest.tech", fs, options, sink);
      REQUIRE(loaded.ok());
      return Loaded{loaded.value(), sink.take()};
    };
    const Loaded base = with_stacking("");
    std::string stacked;
    for (std::size_t t = base.tech->user_type_end(); t < base.tech->type_count() && stacked.empty(); ++t) {
      stacked = std::string{base.tech->type_name(static_cast<tech::TileType>(t))};
    }
    REQUIRE_FALSE(stacked.empty());
    const auto found = base.tech->find_type(stacked);
    REQUIRE(found.ok());
    REQUIRE(*found.id >= base.tech->user_type_end());
    const Loaded l = with_stacking(" draw poly\n draw metal1\n map metal1 " + stacked + "\n map " + stacked +
                                   " poly\n draw " + stacked + "\n draw " + stacked + " blue_style\n");
    const display::StyleSet set = session_styles(*l.tech);
    plot::StyleSource source;
    source.session = &set;
    std::vector<diag::Diagnostic> messages;
    const plot::PlotTech p = reread(*l.tech, source, &messages);
    CHECK(texts(messages) == std::vector<std::string>{"Unknown magic layer \"" + stacked + "\" for PNM plot."});
    CHECK(p.pnm_paint.size() == l.tech->user_type_end());
    // metal1 is as `draw metal1` left it: the stacked source added nothing.
    CHECK(p.pnm_paint[type(*l.tech, "metal1")] == plot::PnmPaint{4, plot::Rgb{0, 255, 0}});
    CHECK(p.pnm_paint[type(*l.tech, "poly")] == plot::PnmPaint{3, plot::Rgb{128, 0, 0}});
  }
}

TEST_CASE("plot colour map and dstyle files", "[cap:plot-output][req:PNM style lines]") {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/t/my.cmap", "# comment\n10 20 30\n\n40 50 60\n").ok());
  REQUIRE(fs.write_atomic("/t/my.dstyle",
                          "# header\n\ndisplay_styles 7\n1 5 1 0 solid 0 a first\n2 2 9 0 solid 0 b second\n")
              .ok());
  const Loaded l = load("plot\n style pnm\n color my.cmap\n dstyle my.dstyle\n draw poly first\n draw metal1 second\n"
                        " draw metal2 nosuch\nend\n",
                        true, &fs);
  const display::StyleSet set = session_styles(*l.tech);
  plot::StyleSource source;
  source.session = &set;
  source.files = &fs;
  source.search_dirs = {"/t"};
  std::vector<diag::Diagnostic> messages;
  const plot::PlotTech p = reread(*l.tech, source, &messages);
  CHECK(messages.empty());  // an unknown name with a dstyle file loaded is silent, as Magic
  CHECK_FALSE(p.init_failed);
  CHECK(p.pnm_paint[type(*l.tech, "poly")] == plot::PnmPaint{5, plot::Rgb{40, 50, 60}});
  // Colour 9 is outside the plot map and the display map: black.
  CHECK(p.pnm_paint[type(*l.tech, "metal1")] == plot::PnmPaint{2, plot::Rgb{0, 0, 0}});
  CHECK(p.pnm_paint[type(*l.tech, "metal2")] == plot::PnmPaint{});
}

TEST_CASE("PNM file errors disable PNM plotting", "[cap:plot-output][req:PNM style lines]") {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/t/bad.cmap", "1 2 3\nx y z\n").ok());
  REQUIRE(fs.write_atomic("/t/bad.dstyle", "\nnot_display_styles\n").ok());
  const Loaded l = load("plot\n style pnm\n color nosuch.cmap\nend\n");
  CHECK(texts(l.messages) == std::vector<std::string>{"Couldn't open colormap file \"nosuch.cmap\""});
  CHECK(l.tech->plot().init_failed);

  const Loaded l2 = load("plot\n style pnm\n color bad.cmap\n dstyle bad.dstyle\n dstyle nosuch\nend\n");
  plot::StyleSource source;
  source.files = &fs;
  source.search_dirs = {"/t"};
  std::vector<diag::Diagnostic> messages;
  const plot::PlotTech p = reread(*l2.tech, source, &messages);
  CHECK(texts(messages) == std::vector<std::string>{"Format error in colormap file",
                                                    "Format error in display style file",
                                                    "PNM plot: Could not open display style file"});
  CHECK(p.init_failed);
}

namespace {

/// A MemoryFileSystem that records the largest read it served and fails
/// the test on a whole-file read.
class BoundedReads final : public platform::FileSystem {
 public:
  platform::MemoryFileSystem files;
  std::size_t largest = 0;

  hefesto::diag::Outcome<std::string> read(const platform::Path& path) override {
    FAIL_CHECK("whole-file read of " << path.string());
    return files.read(path);
  }
  hefesto::diag::Outcome<std::string> read_at_most(const platform::Path& path, std::size_t max_bytes) override {
    auto text = files.read_at_most(path, max_bytes);
    if (text.ok()) {
      largest = std::max(largest, text.value().size());
    }
    return text;
  }
  hefesto::diag::Status write_atomic(const platform::Path& path, std::string_view data) override {
    return files.write_atomic(path, data);
  }
  hefesto::diag::Status append(const platform::Path& path, std::string_view data) override {
    return files.append(path, data);
  }
  bool exists(const platform::Path& path) override { return files.exists(path); }
  hefesto::diag::Outcome<std::uint64_t> size(const platform::Path& path) override { return files.size(path); }
  hefesto::diag::Outcome<std::vector<platform::Path>> list(const platform::Path& directory) override {
    return files.list(directory);
  }
  hefesto::diag::Status create_directories(const platform::Path& directory) override {
    return files.create_directories(directory);
  }
  hefesto::diag::Status rename(const platform::Path& from, const platform::Path& to) override {
    return files.rename(from, to);
  }
  hefesto::diag::Status remove(const platform::Path& path) override { return files.remove(path); }
};

}  // namespace

TEST_CASE("regression: oversized colour map and dstyle files are format errors",
          "[cap:plot-output][req:Platform mechanism of plot output]") {
  // Q-MISC-56, both modes: the loader read the whole file named by a
  // `color` or `dstyle` line at every technology load.
  const std::string comments(2 * plot::detail::kMaxPlotFileBytes, '#');
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    const Loaded l = load("plot\n style pnm\n color big.cmap\n dstyle big.dstyle\nend\n", compat);
    BoundedReads fs;
    REQUIRE(fs.files.write_atomic("/t/big.cmap", comments + "\n1 2 3\n").ok());
    REQUIRE(fs.files.write_atomic("/t/big.dstyle", comments + "\n").ok());
    plot::StyleSource source;
    source.files = &fs;
    source.search_dirs = {"/t"};
    std::vector<diag::Diagnostic> messages;
    const plot::PlotTech p = reread(*l.tech, source, &messages);
    CHECK(texts(messages) ==
          std::vector<std::string>{"Format error in colormap file", "Format error in display style file"});
    CHECK(p.init_failed);
    CHECK(fs.largest == plot::detail::kMaxPlotFileBytes + 1);

    // A file at the limit is read in full.
    std::string at_limit(plot::detail::kMaxPlotFileBytes - 6, '#');
    at_limit += "\n1 2 3";
    REQUIRE(at_limit.size() == plot::detail::kMaxPlotFileBytes);
    REQUIRE(fs.files.write_atomic("/t/edge.cmap", at_limit).ok());
    const Loaded edge = load("plot\n style pnm\n color edge.cmap\nend\n", compat);
    std::vector<diag::Diagnostic> edge_messages;
    const plot::PlotTech q = reread(*edge.tech, source, &edge_messages);
    CHECK(edge_messages.empty());
    CHECK_FALSE(q.init_failed);
  }
}

TEST_CASE("colour map files hold at most 128 colours", "[cap:plot-output][req:PNM style lines]") {
  CHECK(plot::detail::blend(plot::Rgb{}, plot::Rgb{0, 0, 255}) == plot::Rgb{0, 0, 255});
  CHECK(plot::detail::blend(plot::Rgb{0, 0, 255}, plot::Rgb{255, 0, 0}) == plot::Rgb{128, 0, 0});
  std::string text;
  for (int i = 0; i < 129; ++i) {
    text += "1 2 3\n";
  }
  const plot::detail::ColormapParse parsed = plot::detail::parse_pnm_colormap(text);
  CHECK_FALSE(parsed.ok);
  CHECK(parsed.colors.size() == 128);
  CHECK(plot::detail::parse_pnm_colormap(text.substr(6)).ok);
}

// --- Raster printer and legacy plot devices ---------------------------------

TEST_CASE("versatec lines accepted", "[cap:plot-output][req:Raster printer and legacy plot devices]") {
  // MISC-PLOT-20: well-formed and malformed legacy lines, no message.
  const Loaded l = load(
      "plot\n style versatec\n m1 X\n m2 1 2 3\n"
      " m1 0 1 2 3 4 5 6 7 8 9 A B C D E F\n style colorversatec\n m1 black 1\n m1\n"
      " style gremlin\n m1 9\n pc X\n style pixels\n anything at all\nend\n");
  CHECK(l.messages.empty());
  const plot::PlotTech& p = l.tech->plot();
  CHECK(p.ps_styles.empty());
  CHECK(p.ps_patterns.empty());
  CHECK(p.ps_colors.empty());
}

// --- Platform mechanism of plot output ------------------------------------------

TEST_CASE("PNM colour files without a name come from the bundled copies",
          "[cap:plot-output][req:Platform mechanism of plot output]") {
  const Loaded l = load("plot\n style pnm\n color\n dstyle\n draw metal1 metal1\nend\n");
  CHECK(l.messages.empty());
  const plot::PlotTech& p = l.tech->plot();
  CHECK_FALSE(p.init_failed);
  // mos.7bit.mraster_dstyle: style `metal1` has mask 0x18 and colour 24 of
  // mos.7bit.mraster.cmap.
  CHECK(p.pnm_paint[type(*l.tech, "metal1")].wmask != 0);
}

TEST_CASE("default PNM colours without a display style set use the bundled 7bit files",
          "[cap:plot-output][req:Platform mechanism of plot output]") {
  // The iPad scenario: no display style set loaded, no Magic installation.
  // The colours equal those of a session that loaded the bundled
  // mos.7bit.dstyle and mos.7bit.std.cmap as its display styles (what
  // Magic's null display loads).
  const Loaded l = load("plot\n style pnm\n default\nend\n");
  CHECK(l.messages.empty());
  diag::NullSink quiet;
  auto map = display::parse_color_map(asset("mos.7bit.std.cmap"), "mos.7bit.std.cmap", true);
  REQUIRE(map.ok());
  display::StyleSet set;
  set.display_class = display::DisplayClass::bit7;
  set.colors = map.value();
  auto table = display::parse_dstyle(asset("mos.7bit.dstyle"), "mos.7bit.dstyle", set.colors, true, quiet,
                                     display::display_plane_mask(display::DisplayClass::bit7), 5);
  REQUIRE(table.ok());
  set.table = table.value();
  set.types = display::resolve_type_styles(*l.tech, set.table, quiet);
  plot::StyleSource source;
  source.session = &set;
  const plot::PlotTech from_session = reread(*l.tech, source);
  CHECK(l.tech->plot().pnm_paint == from_session.pnm_paint);
  CHECK(l.tech->plot().pnm_paint[type(*l.tech, "metal1")].wmask != 0);
  CHECK(l.tech->plot().pnm_paint[type(*l.tech, "poly")].color != plot::Rgb{});
}

TEST_CASE("the PDK plot sections load with no new message",
          "[cap:plot-output][req:Platform mechanism of plot output]") {
  for (const auto& pdk : {hefesto::test::kSky130A, hefesto::test::kGf180mcuD, hefesto::test::kIhpSg13g2}) {
    const auto path = hefesto::test::pinned_tech(pdk);
    if (!path) {
      WARN("PDK " << pdk.variant << " not fetched; skipped");
      continue;
    }
    INFO(pdk.variant);
    platform::LocalFileSystem fs;
    diag::CollectingSink sink;
    tech::LoadOptions options;
    options.search_path = {path->parent_path()};
    auto loaded = tech::load_technology(fs, *path, options, sink);
    REQUIRE(loaded.ok());
    for (const auto& d : sink.take()) {
      const bool plot_line = d.related.has_value() && d.related->name == "plot";
      CHECK_FALSE(d.code.starts_with("PLOT-"));
      CHECK_FALSE(plot_line);
    }
    const plot::PlotTech& p = loaded.value()->plot();
    CHECK(p.has_section);
    CHECK_FALSE(p.init_failed);
  }
}
