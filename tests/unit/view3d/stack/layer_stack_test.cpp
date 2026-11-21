// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// view-3d "Render statement in the cifoutput section" and "Layer height
// and thickness for the layout-layer view" (M10 design D5, tasks 3.3 and
// 3.4; Q-MISC-72 compat, Q-MISC-77): the `render` attributes of the
// output style, their style resolution, the layout-layer and mask-layer
// stacks, the draw heights in both magic-compat modes and the physical
// heights. Owner: TECH (M10).

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/extract/tech/height_view.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"
#include "reference_data.hpp"

namespace display = hefesto::display;
namespace extract = hefesto::extract;
namespace tech = hefesto::tech;
namespace diag = hefesto::diag;
namespace mask = hefesto::mask;
namespace platform = hefesto::platform;
namespace view3d = hefesto::view3d;

namespace {

std::string tech_text(const std::string& cifoutput_body, const std::string& extract_body) {
  return "tech\n format 35\n stack\nend\n"
         "planes\n well,w\n active,a\n metal1,m1\n metal2,m2\n metal3,m3\nend\n"
         "types\n well nwell\n active ndiffusion,ndiff\n active polysilicon,poly\n metal1 metal1,m1\n"
         " metal2 metal2,m2\n metal3 metal3,m3\n metal1 via1\nend\n"
         "contact\n via1 metal1 metal2\nend\n"
         "styles\n styletype mos\nend\n"
         "compose\nend\n"
         "connect\n metal1 via1\n metal2 via1\nend\n"
         "cifoutput\n" +
         cifoutput_body + "end\ncifinput\nend\ndrc\nend\nextract\n" + extract_body + "end\n";
}

std::shared_ptr<const tech::Technology> load(const std::string& cifoutput_body, const std::string& extract_body = "") {
  platform::MemoryFileSystem fs;
  diag::CollectingSink sink;
  auto loaded = tech::load_technology_text(tech_text(cifoutput_body, extract_body), "/t/stack.tech", fs,
                                           tech::LoadOptions{}, sink);
  REQUIRE(loaded.ok());
  return loaded.value();
}

tech::TileType type_of(const tech::Technology& t, std::string_view name) {
  const auto found = t.find_type(name);
  REQUIRE(found.ok());
  return *found.id;
}

struct Styles {
  mask::OutputStyleSet set;
  std::vector<diag::Diagnostic> messages;
  [[nodiscard]] const mask::OutputStyle& current() const { return *set.current_style(); }
};

Styles output_styles(const tech::Technology& t) {
  diag::CollectingSink sink;
  Styles s;
  s.set = mask::read_output_styles(t, sink);
  s.messages = sink.take();
  REQUIRE(s.set.current_style() != nullptr);
  return s;
}

// A display style table: the 52 fixed slots, then 30 layout styles whose
// ordinals are 100 + position; position 20 is `metal1`, 5 is `metal2`.
display::StyleTable style_table() {
  display::StyleTable table;
  table.styles.resize(display::kFixedStyleCount);
  for (int i = 0; i < 30; ++i) {
    display::DisplayStyle s;
    s.ordinal = 100 + i;
    s.long_name = i == 20 ? "metal1" : i == 5 ? "metal2" : "style" + std::to_string(i);
    table.styles.push_back(s);
  }
  table.layout_count = 30;
  return table;
}

constexpr std::size_t kMetal1Style = display::kFixedStyleCount + 20;
constexpr std::size_t kMetal2Style = display::kFixedStyleCount + 5;

const std::string kGdsStyle =
    "style gdsii\n scalefactor 10 nanometers\n"
    " layer MET1 m1,via1\n  calma 68 20\n"
    " layer MET2 m2\n  calma 69 20\n"
    " layer MET1 m1\n  calma 68 44\n"
    " templayer TMP m3\n";

const std::string kOrdered =
    " style s\n planeorder well 0\n planeorder active 1\n planeorder metal1 2\n planeorder metal2 3\n"
    " planeorder metal3 4\n";

}  // namespace

// --- task 3.3: the render statement ----------------------------------------

TEST_CASE("Render by style name", "[cap:view-3d][req:Render statement in the cifoutput section]") {
  const auto t = load(kGdsStyle + " render MET1 metal1 1.376 0.36\n");
  const Styles s = output_styles(*t);
  CHECK(s.messages.empty());
  const mask::OutputLayer& met1 = s.current().layers[0];
  REQUIRE(met1.render.size() == 1);
  CHECK(met1.render[0] == mask::RenderAttrs{"metal1", 1.376F, 0.36F, 1});

  const display::StyleTable table = style_table();
  diag::CollectingSink sink;
  const view3d::LayerStack stack = view3d::build_mask_stack(s.current(), &table, true, sink);
  CHECK(sink.take().empty());
  REQUIRE(stack.size() == 4);
  const view3d::StackLayer& l = stack.layers()[0];
  CHECK(l.name == "MET1");
  CHECK(l.mask_layer == std::size_t{0});
  CHECK(l.style == kMetal1Style);
  CHECK(view3d::render_style_position(l.style) == 20);
  CHECK(l.height == 1.376F);
  CHECK(l.thickness == 0.36F);
  CHECK(l.source == view3d::ValueSource::render);
}

TEST_CASE("render by style number matches the style's ordinal", "[cap:view-3d][req:Render statement in the cifoutput section]") {
  const auto t = load(kGdsStyle + " render MET2 105 2.0 0.5\n");
  const Styles s = output_styles(*t);
  const display::StyleTable table = style_table();
  diag::CollectingSink sink;
  const view3d::LayerStack stack = view3d::build_mask_stack(s.current(), &table, false, sink);
  CHECK(stack.layers()[1].style == kMetal2Style);
  CHECK(stack.layers()[1].height == 2.0F);
}

TEST_CASE("Unknown layer", "[cap:view-3d][req:Render statement in the cifoutput section]") {
  const auto t = load(kGdsStyle + " render NOSUCH metal1 1 1\n");
  const Styles s = output_styles(*t);
  REQUIRE(s.messages.size() == 1);
  CHECK(s.messages[0].code == "MASK-STYLE-001");
  CHECK(s.messages[0].text == "Unknown layer name.");
  for (const mask::OutputLayer& layer : s.current().layers) {
    CHECK(layer.render.empty());
  }
}

TEST_CASE("render errors: argument count and non-numeric values ignore the line",
          "[cap:view-3d][req:Render statement in the cifoutput section]") {
  const auto t = load(kGdsStyle + " render MET1 metal1 1.0\n render MET1 metal1 high 0.36\n"
                                  " render MET1 metal1 1.0 thick\n");
  const Styles s = output_styles(*t);
  REQUIRE(s.messages.size() == 3);
  CHECK(s.messages[0].text == "Wrong number of arguments in render statement.");
  CHECK(s.messages[1].text == "Syntax: render <layer> <style> <height> <thick>");
  CHECK(s.messages[2].text == "Syntax: render <layer> <style> <height> <thick>");
  CHECK(s.current().layers[0].render.empty());
}

TEST_CASE("a render style that does not resolve is reported once per line and its statement ignored",
          "[cap:view-3d][req:Render statement in the cifoutput section]") {
  const auto t = load(kGdsStyle + " render MET1 metal1 1.376 0.36\n render MET1 nosuch 9 9\n"
                                  " render MET2 999 2 2\n");
  const Styles s = output_styles(*t);
  CHECK(s.messages.empty());  // the style is resolved by the 3D view
  const display::StyleTable table = style_table();
  diag::CollectingSink sink;
  const view3d::LayerStack stack = view3d::build_mask_stack(s.current(), &table, true, sink);
  const auto messages = sink.take();
  REQUIRE(messages.size() == 2);  // the `nosuch` line once (two MET1 layers), the `999` line once
  CHECK(messages[0].code == "VIEW3D-STACK-002");
  CHECK(messages[0].text == "Error:  Bad render style for CIF layer.");
  // MET1 keeps the earlier statement, as Magic ignores the bad one.
  CHECK(stack.layers()[0].style == kMetal1Style);
  CHECK(stack.layers()[0].height == 1.376F);
  // MET2 has no valid statement: the default.
  CHECK(stack.layers()[1].style == view3d::kPaleHighlightStyle);
  CHECK(stack.layers()[1].source == view3d::ValueSource::none);
}

TEST_CASE("two render lines with the same bad style report it twice, as Magic reports each line",
          "[cap:view-3d][req:Render statement in the cifoutput section]") {
  // Regression (PR #77 review): the error was deduplicated by style name.
  const auto t = load(kGdsStyle + " render MET1 nosuch 1 1\n render MET2 nosuch 2 2\n");
  const Styles s = output_styles(*t);
  CHECK(s.current().layers[0].render[0].statement == 1);
  CHECK(s.current().layers[1].render[0].statement == 2);
  const display::StyleTable table = style_table();
  diag::CollectingSink sink;
  (void)view3d::build_mask_stack(s.current(), &table, true, sink);
  const auto messages = sink.take();
  REQUIRE(messages.size() == 2);
  CHECK(messages[0].text == "Error:  Bad render style for CIF layer.");
  CHECK(messages[1].text == "Error:  Bad render style for CIF layer.");
}

TEST_CASE("several mask layers sharing a name all take the render statement",
          "[cap:view-3d][req:Render statement in the cifoutput section]") {
  const auto t = load(kGdsStyle + " render MET1 metal1 1.376 0.36\n");
  const Styles s = output_styles(*t);
  CHECK(s.current().layers[0].render.size() == 1);
  CHECK(s.current().layers[2].render.size() == 1);
  const display::StyleTable table = style_table();
  diag::NullSink sink;
  const view3d::LayerStack stack = view3d::build_mask_stack(s.current(), &table, true, sink);
  CHECK(stack.layers()[2].name == "MET1");
  CHECK(stack.layers()[2].style == kMetal1Style);
  CHECK(stack.layers()[2].height == 1.376F);
}

TEST_CASE("a mask layer without render has the pale-highlight style and h = t = 0",
          "[cap:view-3d][req:Render statement in the cifoutput section]") {
  const auto t = load(kGdsStyle + " render MET1 metal1 1.376 0.36\n");
  const Styles s = output_styles(*t);
  const display::StyleTable table = style_table();
  diag::NullSink sink;
  const view3d::LayerStack stack = view3d::build_mask_stack(s.current(), &table, false, sink);
  const view3d::StackLayer& met2 = stack.layers()[1];
  CHECK(met2.style == view3d::kPaleHighlightStyle);
  CHECK(view3d::render_style_position(met2.style) == -49);  // STYLE_PALEHIGHLIGHTS - TECHBEGINSTYLES
  CHECK(met2.height == 0.0F);
  CHECK(met2.thickness == 0.0F);
  CHECK(stack.draw_height(1) == view3d::DrawHeight{});
  CHECK_FALSE(stack.physical_height_um(1).has_value());
}

TEST_CASE("without a display style table the render heights apply with the pale-highlight style",
          "[cap:view-3d][req:Render statement in the cifoutput section]") {
  const auto t = load(kGdsStyle + " render MET1 metal1 1.376 0.36\n render MET1 nosuch 2 2\n");
  const Styles s = output_styles(*t);
  diag::CollectingSink sink;
  const view3d::LayerStack stack = view3d::build_mask_stack(s.current(), nullptr, true, sink);
  CHECK(sink.take().empty());
  CHECK(stack.layers()[0].style == view3d::kPaleHighlightStyle);
  CHECK(stack.layers()[0].height == 2.0F);  // the last statement
}

// --- task 3.4: the layer stacks ----------------------------------------------

TEST_CASE("Default height from plane order", "[cap:view-3d][req:Layer height and thickness for the layout-layer view]") {
  const auto t = load("", " style s\n planeorder well 0\n planeorder active 1\n planeorder metal1 2\n"
                          " planeorder metal3 3\n planeorder metal2 4\n");
  diag::NullSink sink;
  const auto heights = extract::HeightView::from_technology(*t, sink);
  display::TypeStyleMap styles;
  styles.styles_of_type.resize(tech::kMaxTileTypes);
  styles.styles_of_type[type_of(*t, "metal2")] = {kMetal2Style};
  const view3d::LayerStack stack = view3d::build_layout_stack(*t, heights, styles, true, 0.005);
  REQUIRE(stack.size() == 1);
  CHECK(stack.mode() == view3d::Mode::layout_layer);
  CHECK(stack.layers()[0].name == "metal2");
  CHECK(stack.layers()[0].source == view3d::ValueSource::extract_default);
  CHECK(stack.draw_height(0) == view3d::DrawHeight{static_cast<float>(0.1 * 4), 0.05F});
  const auto um = stack.physical_height_um(0);
  REQUIRE(um.has_value());
  CHECK(um->height_um == static_cast<double>(static_cast<float>(0.1 * 4)) * 0.005);
}

TEST_CASE("a type in two display styles gives two entries in style-table order (Q-MISC-77)",
          "[cap:view-3d][req:Layer height and thickness for the layout-layer view]") {
  const auto t = load("", kOrdered + " height metal1 1.0 0.5\n");
  diag::NullSink sink;
  const auto heights = extract::HeightView::from_technology(*t, sink);
  display::TypeStyleMap styles;
  styles.styles_of_type.resize(tech::kMaxTileTypes);
  styles.styles_of_type[type_of(*t, "via1")] = {kMetal1Style, kMetal2Style};
  styles.styles_of_type[type_of(*t, "metal1")] = {kMetal1Style, kMetal1Style};  // a repeat is dropped
  styles.styles_of_type[type_of(*t, "metal2")] = {kMetal2Style};
  const view3d::LayerStack stack = view3d::build_layout_stack(*t, heights, styles, true, 1.0);
  REQUIRE(stack.size() == 4);
  // Style 57 (metal2's) first: metal2 then via1 by type; then style 72.
  CHECK(stack.layers()[0].style == kMetal2Style);
  CHECK(stack.layers()[0].name == "metal2");
  CHECK(stack.layers()[1].style == kMetal2Style);
  CHECK(stack.layers()[1].name == "via1");
  CHECK(stack.layers()[2].style == kMetal1Style);
  CHECK(stack.layers()[2].name == "metal1");
  CHECK(stack.layers()[2].source == view3d::ValueSource::extract);
  CHECK(stack.layers()[3].name == "via1");
  CHECK(stack.find(type_of(*t, "via1"), kMetal1Style) == view3d::LayerId{3});
  CHECK_FALSE(stack.find(type_of(*t, "metal1"), kMetal2Style).has_value());
}

TEST_CASE("a split tile is drawn with the height of its left type",
          "[cap:view-3d][req:Layer height and thickness for the layout-layer view]") {
  const auto t = load("", kOrdered + " height metal1 1.0 0.5\n height poly 0.3 0.2\n");
  diag::NullSink sink;
  const auto heights = extract::HeightView::from_technology(*t, sink);
  display::TypeStyleMap styles;  // poly is in no style: its height still counts
  styles.styles_of_type.resize(tech::kMaxTileTypes);
  styles.styles_of_type[type_of(*t, "metal1")] = {kMetal1Style};
  const view3d::LayerStack stack = view3d::build_layout_stack(*t, heights, styles, true, 1.0);
  hefesto::db::TileView tile;
  tile.split = true;
  tile.left_type = type_of(*t, "poly");
  tile.right_type = type_of(*t, "metal1");
  tile.right_side = true;
  tile.type = tile.right_type;
  CHECK(stack.tile_height(tile) == view3d::DrawHeight{0.3F, 0.2F});
  tile.left_type = tech::kSpace;  // space has h = t = 0
  CHECK(stack.tile_height(tile) == view3d::DrawHeight{});
  hefesto::db::TileView plain;
  plain.type = type_of(*t, "metal1");
  CHECK(stack.tile_height(plain) == view3d::DrawHeight{1.0F, 0.5F});
}

TEST_CASE("no extract style gives h = t = 0 and no physical height",
          "[cap:view-3d][req:Layer height and thickness for the layout-layer view]") {
  const auto t = load("");
  display::TypeStyleMap styles;
  styles.styles_of_type.resize(tech::kMaxTileTypes);
  styles.styles_of_type[type_of(*t, "metal1")] = {kMetal1Style};
  const view3d::LayerStack stack = view3d::build_layout_stack(*t, extract::HeightView{}, styles, true, 1.0);
  REQUIRE(stack.size() == 1);
  CHECK(stack.layers()[0].source == view3d::ValueSource::none);
  CHECK(stack.draw_height(0) == view3d::DrawHeight{});
  CHECK_FALSE(stack.physical_height_um(0).has_value());
  hefesto::db::TileView plain;
  plain.type = type_of(*t, "metal1");
  CHECK(stack.tile_height(plain) == view3d::DrawHeight{});
  CHECK(stack.draw_height(7) == view3d::DrawHeight{});  // out of range
}

TEST_CASE("layout-layer draw heights do not depend on magic-compat (Q-MISC-72)",
          "[cap:view-3d][req:Layer height and thickness for the layout-layer view]") {
  const auto t = load("", kOrdered + " height metal1 1.0 0.5\n");
  diag::NullSink sink;
  const auto heights = extract::HeightView::from_technology(*t, sink);
  display::TypeStyleMap styles;
  styles.styles_of_type.resize(tech::kMaxTileTypes);
  styles.styles_of_type[type_of(*t, "metal1")] = {kMetal1Style};
  const auto on = view3d::build_layout_stack(*t, heights, styles, true, 0.01);
  const auto off = view3d::build_layout_stack(*t, heights, styles, false, 0.01);
  CHECK(on.draw_height(0) == off.draw_height(0));
  CHECK(on.physical_height_um(0) == off.physical_height_um(0));
}

namespace {

struct Sky130 {
  std::shared_ptr<const tech::Technology> tech;
  mask::OutputStyleSet styles;
};

std::optional<Sky130> load_pdk(const std::optional<std::filesystem::path>& file) {
  if (!file) {
    return std::nullopt;
  }
  platform::LocalFileSystem files;
  diag::CollectingSink sink;
  auto loaded = tech::load_technology(files, file->string(), tech::LoadOptions{}, sink);
  REQUIRE(loaded.ok());
  Sky130 out{loaded.value(), mask::read_output_styles(*loaded.value(), sink)};
  REQUIRE(out.styles.current_style() != nullptr);
  return out;
}

std::size_t layer_index(const mask::OutputStyle& style, std::string_view name) {
  const auto found = style.find_layer(name);
  REQUIRE(found.has_value());
  return *found;
}

}  // namespace

TEST_CASE("sky130A MET1 in mask-layer mode with magic-compat ON draws the render values as stored (Q-MISC-72)",
          "[cap:view-3d][req:Layer height and thickness for the layout-layer view]") {
  const auto pdk = load_pdk(hefesto::test::pinned_sky130a_tech());
  if (!pdk) {
    SKIP("PDK tech file not in .cache/pdks");
  }
  const mask::OutputStyle& style = *pdk->styles.current_style();
  diag::NullSink sink;
  const view3d::LayerStack on = view3d::build_mask_stack(style, nullptr, true, sink);
  const auto met1 = static_cast<view3d::LayerId>(layer_index(style, "MET1"));
  CHECK(on.draw_height(met1) == view3d::DrawHeight{1.3761F, 0.36F});
  CHECK(on.physical_height_um(met1) ==
        view3d::PhysicalHeight{static_cast<double>(1.3761F), static_cast<double>(0.36F)});
}

TEST_CASE("sky130A MET1 in mask-layer mode with magic-compat OFF converts by 0.01 µm per internal unit (Q-MISC-72)",
          "[cap:view-3d][req:Layer height and thickness for the layout-layer view]") {
  const auto pdk = load_pdk(hefesto::test::pinned_sky130a_tech());
  if (!pdk) {
    SKIP("PDK tech file not in .cache/pdks");
  }
  const mask::OutputStyle& style = *pdk->styles.current_style();
  diag::NullSink sink;
  const view3d::LayerStack off = view3d::build_mask_stack(style, nullptr, false, sink);
  const view3d::LayerStack on = view3d::build_mask_stack(style, nullptr, true, sink);
  CHECK(off.microns_per_unit() == 0.01);
  const auto met1 = static_cast<view3d::LayerId>(layer_index(style, "MET1"));
  const view3d::DrawHeight h = off.draw_height(met1);
  CHECK(h.height == static_cast<float>(static_cast<double>(1.3761F) / 0.01));
  CHECK(h.thickness == static_cast<float>(static_cast<double>(0.36F) / 0.01));
  // The physical height is the same in both modes.
  CHECK(off.physical_height_um(met1) == on.physical_height_um(met1));
  const auto warning = view3d::missing_height_warning(on, style);
  if (warning) {
    CHECK(warning->text.find(" MET1 ") == std::string::npos);
    CHECK(warning->text.find(" MET1;") == std::string::npos);
  }
}

TEST_CASE("gf180mcuD has no render lines: no physical heights and VIEW3D-STACK-001 names the layers",
          "[cap:view-3d][req:Layer height and thickness for the layout-layer view]") {
  const auto pdk = load_pdk(hefesto::test::pinned_gf180mcud_tech());
  if (!pdk) {
    SKIP("PDK tech file not in .cache/pdks");
  }
  const mask::OutputStyle& style = *pdk->styles.current_style();
  diag::NullSink sink;
  const view3d::LayerStack stack = view3d::build_mask_stack(style, nullptr, true, sink);
  REQUIRE(stack.size() == style.layers.size());
  for (std::size_t i = 0; i < stack.size(); ++i) {
    CHECK_FALSE(stack.physical_height_um(static_cast<view3d::LayerId>(i)).has_value());
    CHECK(stack.draw_height(static_cast<view3d::LayerId>(i)) == view3d::DrawHeight{});
  }
  const auto warning = view3d::missing_height_warning(stack, style);
  REQUIRE(warning.has_value());
  CHECK(warning->code == "VIEW3D-STACK-001");
  CHECK(warning->severity == diag::Severity::warning);
  const std::size_t met1 = layer_index(style, "MET1");
  CHECK(warning->text.find(" " + style.layers[met1].name) != std::string::npos);
  // The layout-layer stack has no such warning.
  CHECK_FALSE(view3d::missing_height_warning(view3d::LayerStack{}, style).has_value());
}

TEST_CASE("missing_height_warning names non-temporary mask layers once, in style order",
          "[cap:view-3d][req:Layer height and thickness for the layout-layer view]") {
  const auto t = load(kGdsStyle + " render MET2 metal2 2 1\n");
  const Styles s = output_styles(*t);
  diag::NullSink sink;
  const view3d::LayerStack stack = view3d::build_mask_stack(s.current(), nullptr, true, sink);
  const auto warning = view3d::missing_height_warning(stack, s.current());
  REQUIRE(warning.has_value());
  CHECK(warning->text.starts_with("No render height for mask layer(s): MET1;"));  // TMP is temporary
}
