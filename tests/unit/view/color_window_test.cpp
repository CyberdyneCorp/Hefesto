// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `color` window client (capability ui-color-window; Q-UI-37).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <sstream>
#include <string>

#include "hefesto/cmd/prompt.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/undo/undo_log.hpp"
#include "hefesto/view/color_window.hpp"
#include "hefesto/view/diag_codes.hpp"
#include "view_session.hpp"

namespace cmd = hefesto::cmd;
namespace db = hefesto::db;
namespace view = hefesto::view;
namespace display = hefesto::display;
using Catch::Approx;
using hefesto::test::all_text;
using hefesto::test::ViewSession;

namespace {

/// A session with a 256-entry color map and the color client's commands in
/// a registry of their own (they are the `color` client's, not global).
struct ColorSession : ViewSession {
  cmd::CommandRegistry color_registry;
  std::unique_ptr<cmd::Dispatcher> color_dispatcher;

  ColorSession() {
    editor.styles.emplace();
    editor.styles->colors.entries.resize(256);
    for (auto spec : view::color_client_commands()) {
      REQUIRE_FALSE(color_registry.add(std::move(spec)).has_value());
    }
    color_dispatcher = std::make_unique<cmd::Dispatcher>(color_registry, services.view(), project, script);
    color_dispatcher->set_editor(&editor);
  }

  cmd::CommandResult color(std::string name, std::vector<std::string> args = {}) {
    cmd::Invocation inv;
    inv.name = std::move(name);
    inv.args = std::move(args);
    return color_dispatcher->execute(inv);
  }

  void set_rgb(int index, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    auto& e = editor.styles->colors.entries[static_cast<std::size_t>(index)];
    e.r = r;
    e.g = g;
    e.b = b;
  }

  [[nodiscard]] std::array<int, 3> rgb(int index) const {
    const auto& e = editor.styles->colors.entries[static_cast<std::size_t>(index)];
    return {e.r, e.g, e.b};
  }

  view::ColorWindow& open(const std::string& octal) {
    REQUIRE(run("specialopen", {"color", octal}).ok());
    REQUIRE_FALSE(editor.color_map.windows.empty());
    return editor.color_map.windows.back();
  }

  void point_at(const view::ColorWindow& w, db::Point p) {
    editor.command_point = cmd::PointerState{w.id, p, db::Rect{p.x, p.y, p.x, p.y}};
  }
};

struct ScriptedHost final : cmd::PromptHost {
  explicit ScriptedHost(std::size_t answer) : answer_(answer) {}
  std::size_t ask(std::string_view question, std::span<const std::string>, std::size_t) override {
    asked = std::string(question);
    return answer_;
  }
  std::string asked;

 private:
  std::size_t answer_;
};

}  // namespace

// --- Color window creation and layout ----------------------------------------------

TEST_CASE("specialopen color opens a color window on an octal index", "[cap:ui-color-window][req:Color window creation and layout]") {
  ColorSession s;
  // Scenario "Caption shows octal".
  view::ColorWindow& w = s.open("47");
  CHECK(w.color == 39);
  CHECK(w.caption == "COLOR = 047");
  // ANDed with 0o377; no argument means 0.
  CHECK(s.open("777").color == 0377);
  REQUIRE(s.run("specialopen", {"color"}).ok());
  CHECK(s.editor.color_map.windows.back().color == 0);
  CHECK(s.editor.color_map.windows.back().caption == "COLOR = 00");
}

TEST_CASE("color windows share the window ids and the 31-window limit", "[cap:ui-color-window][req:Color window creation and layout]") {
  ColorSession s;
  db::CellDef* top = s.cell("top", {0, 0, 10, 10});
  view::LayoutWindow& layout = s.window(top);
  view::ColorWindow& c = s.open("1");
  CHECK(c.id != layout.id());
  for (std::size_t i = 2; i < view::kMaxWindows; ++i) {
    REQUIRE(s.run("specialopen", {"color"}).ok());
  }
  const auto full = s.run("specialopen", {"color"});
  CHECK_FALSE(full.ok());
  CHECK(full.diagnostics.back().code == view::codes::kWindowLimit);
  // Closing a color window frees its id for a layout window.
  const view::WindowId freed = s.editor.color_map.windows.front().id;
  CHECK(view::close_color_window(s.editor, freed));
  auto again = s.editor.windows.open(top, {0, 0, 99, 99});
  REQUIRE(again.ok());
  CHECK(again.value()->id() == freed);
}

TEST_CASE("the color window shows six bars, twelve pumps and the swatch", "[cap:ui-color-window][req:Color window creation and layout]") {
  ColorSession s;
  CHECK(view::color_bars().size() == 6);
  CHECK(view::color_pumps().size() == 12);
  CHECK(view::color_bars()[0].bar == db::Rect{2000, 8000, 10000, 9000});
  CHECK(view::color_bars()[5].name == "Value");
  CHECK(view::color_pumps()[7].rect == db::Rect{22500, 8000, 23500, 9000});
  CHECK(view::kColorWindowSurface == db::Rect{0, 1500, 24000, 17000});

  view::ColorWindow& w = s.open("5");
  s.set_rgb(5, 255, 0, 0);
  const display::HighlightScene scene = view::color_window_scene(s.editor, w);
  // Red bar filled to (255 + 0.5)/255 (clamped by the bar width truncation).
  const auto& red_fill = scene.items[0];
  CHECK(red_fill.shape == display::HighlightShape::box);
  CHECK(red_fill.style == 35);
  CHECK(red_fill.rect == db::Rect{2000, 8000, 2000 + static_cast<db::Coord>((255.5 / 255.0) * 8000), 9000});
  CHECK(scene.items[1].style == 15);  // outline in the bounding-box style
  CHECK(scene.items[2].text == "Red");
  // Green bar: (0 + 0.5)/255 of the width.
  CHECK(scene.items[3].rect.x1 == 2000 + static_cast<db::Coord>((0.5 / 255.0) * 8000));
  // The swatch takes the edited color.
  bool swatch = false;
  for (const auto& it : scene.items) {
    if (it.style == 24) {
      swatch = true;
      CHECK(it.rect == view::kCurrentColorArea);
      CHECK(it.resolved.r == 1.0F);
      CHECK(it.resolved.g == 0.0F);
    }
  }
  CHECK(swatch);
  CHECK(scene.items.back().text == "Color Being Edited");
}

// --- RGB/HSV conversion ------------------------------------------------------------------

TEST_CASE("RGB and HSV convert with Magic's formulas", "[cap:ui-color-window][req:RGB/HSV conversion]") {
  // Scenario "Pure red hue".
  const view::Hsv red = view::rgb_to_hsv({1, 0, 0});
  CHECK(red.h == 0.0);
  CHECK(red.s == 1.0);
  CHECK(red.v == 1.0);
  const view::Hsv gray = view::rgb_to_hsv({0.5, 0.5, 0.5});
  CHECK(gray.h == 0.0);
  CHECK(gray.s == 0.0);
  CHECK(gray.v == 0.5);
  const view::Hsv blue = view::rgb_to_hsv({0, 0, 1});
  CHECK(blue.h == Approx(4.0 / 6.0));
  const view::Hsv magenta = view::rgb_to_hsv({1, 0, 0.5});
  CHECK(magenta.h == Approx(1.0 - 0.5 / 6.0));  // negative hue wraps
  for (const view::Rgb c : {view::Rgb{0.2, 0.4, 0.6}, view::Rgb{0.9, 0.1, 0.3}, view::Rgb{0.3, 0.8, 0.1}}) {
    const view::Rgb back = view::hsv_to_rgb(view::rgb_to_hsv(c));
    CHECK(back.r == Approx(c.r));
    CHECK(back.g == Approx(c.g));
    CHECK(back.b == Approx(c.b));
  }
  const view::Rgb sector5 = view::hsv_to_rgb({0.9, 1.0, 1.0});
  CHECK(sector5.r == 1.0);
  CHECK(sector5.g == 0.0);
}

// --- Color window buttons ------------------------------------------------------------------

TEST_CASE("a press on a bar sets the component; pumps step it", "[cap:ui-color-window][req:Color window buttons]") {
  ColorSession s;
  view::ColorWindow& w = s.open("5");
  // Scenario "Click red bar midpoint".
  CHECK(view::press_button(s.editor, w, {6000, 8500}, view::Button::left));
  CHECK(s.rgb(5) == std::array<int, 3>{128, 0, 0});
  // Left on the red + pump adds 0.0078; another button 5x as much.
  CHECK(view::press_button(s.editor, w, {11000, 8500}, view::Button::left));
  CHECK(s.rgb(5)[0] == static_cast<int>((128.0 / 255.0 + 0.0078) * 255.0 + 0.5));
  const int before = s.rgb(5)[0];
  CHECK(view::press_button(s.editor, w, {1000, 8500}, view::Button::right));
  CHECK(s.rgb(5)[0] == static_cast<int>((before / 255.0 - 5 * 0.0078) * 255.0 + 0.5));
  // The swatch picks nothing (no eyedropper, Q-UI-37).
  CHECK_FALSE(view::press_button(s.editor, w, {10000, 13000}, view::Button::left));
}

// --- Color component update ---------------------------------------------------------------

TEST_CASE("component updates clamp, convert HSV back, clear the name and mark the map", "[cap:ui-color-window][req:Color component update]") {
  ColorSession s;
  view::ColorWindow& w = s.open("5");
  s.set_rgb(5, 184, 73, 83);
  s.editor.styles->colors.entries[5].name = "pinkish";
  view::update_component(s.editor, w, view::ColorComponent::green, 2.0, true);  // clamped to 1
  CHECK(s.rgb(5) == std::array<int, 3>{184, 255, 83});
  CHECK(s.editor.styles->colors.entries[5].name.empty());
  CHECK(s.editor.color_map.modified);
  // Hue + pump (UI-G-062): HSV of (184,73,83), h += 0.01, back to RGB.
  s.set_rgb(5, 184, 73, 83);
  view::update_component(s.editor, w, view::ColorComponent::hue, 0.01, false);
  view::Hsv hsv = view::rgb_to_hsv({184 / 255.0, 73 / 255.0, 83 / 255.0});
  hsv.h += 0.01;
  const view::Rgb rgb = view::hsv_to_rgb(hsv);
  CHECK(s.rgb(5) == std::array<int, 3>{static_cast<int>(rgb.r * 255.0 + 0.5), static_cast<int>(rgb.g * 255.0 + 0.5),
                                       static_cast<int>(rgb.b * 255.0 + 0.5)});
}

TEST_CASE("a color change is one undoable event with the old and new RGB", "[cap:ui-color-window][req:Color component update]") {
  // Scenario "Undo a color change": the event restores (184,73,83).
  ColorSession s;
  view::ColorWindow& w = s.open("5");
  s.set_rgb(5, 184, 73, 83);
  REQUIRE(view::press_button(s.editor, w, {11000, 8500}, view::Button::left));
  const auto raised = s.rgb(5);
  CHECK(raised[0] > 184);
  (void)s.editor.undo.close_step();  // editor::post_command closes the command's step
  const auto undone = s.editor.undo.undo(s.editor, 1);
  REQUIRE(undone.ok());
  CHECK(undone.value().steps == 1);
  CHECK(s.rgb(5) == std::array<int, 3>{184, 73, 83});
  CHECK(s.editor.undo.redo(s.editor, 1).steps == 1);
  CHECK(s.rgb(5) == raised);
}

// --- Color window commands -----------------------------------------------------------------

TEST_CASE("color prints, selects, steps and reports the edited color", "[cap:ui-color-window][req:Color window commands]") {
  ColorSession s;
  view::ColorWindow& w = s.open("47");
  s.set_rgb(39, 10, 20, 30);
  CHECK(all_text(s.color("color")) == "Current color is 47 octal (39 decimal) (red = 10, green = 20, blue = 30)");
  const auto get = s.color("color", {"get"});
  REQUIRE(get.ok());
  CHECK(get.value.to_string() == "39");
  const auto rgb = s.color("color", {"rgb"});
  REQUIRE(rgb.ok());
  CHECK(rgb.value.to_string() == "10 20 30");
  REQUIRE(s.color("color", {"next"}).ok());
  CHECK(w.color == 40);
  CHECK(w.caption == "COLOR = 050");
  REQUIRE(s.color("color", {"0x10"}).ok());
  CHECK(w.color == 16);
  REQUIRE(s.color("color", {"010"}).ok());
  CHECK(w.color == 8);
  const auto big = s.color("color", {"0x1ff"});
  CHECK_FALSE(big.ok());
  CHECK(big.diagnostics.back().code == view::codes::kColorRange);
  CHECK(big.diagnostics.back().text == "The colormap only has values from 0 to 255 (decimal).");
  CHECK(w.color == 8);
  const auto usage = s.color("color", {"bogus"});
  CHECK_FALSE(usage.ok());
  CHECK(usage.diagnostics.back().text == "Usage: color [#|next|last|get|rgb]");
  CHECK_FALSE(s.color("color", {"1", "2"}).ok());
  REQUIRE(s.color("color", {"lastly"}).ok());  // only the first 4 letters are checked
  CHECK(w.color == 7);
}

TEST_CASE("color next and last wrap around the map", "[cap:ui-color-window][req:Color window commands]") {
  ColorSession s;
  // Scenario "Next wraps".
  view::ColorWindow& w = s.open("377");
  REQUIRE(s.color("color", {"next"}).ok());
  CHECK(w.color == 0);
  REQUIRE(s.color("color", {"last"}).ok());
  CHECK(w.color == 255);
}

TEST_CASE("the color client registers color, load, pushbutton and save", "[cap:ui-color-window][req:Color window commands]") {
  const auto specs = view::color_client_commands();
  REQUIRE(specs.size() == 4);
  CHECK(specs[0].name == "color");
  CHECK(specs[1].name == "load");
  CHECK(specs[2].name == "pushbutton");
  CHECK(specs[3].name == "save");
  ColorSession s;
  const auto none = s.color("color");
  CHECK_FALSE(none.ok());  // no color window
  CHECK(none.diagnostics.back().code == view::codes::kNoColorWindow);
}

TEST_CASE("pushbutton presses at the current point and never releases", "[cap:ui-color-window][req:Color window commands]") {
  ColorSession s;
  view::ColorWindow& w = s.open("5");
  // Scenario "pushbutton is press-only".
  s.point_at(w, {6000, 8500});
  REQUIRE(s.color("pushbutton", {"left"}).ok());
  CHECK(s.rgb(5) == std::array<int, 3>{128, 0, 0});
  CHECK(s.color("pushbutton", {"M"}).ok());  // case-insensitive prefix
  CHECK(all_text(s.color("pushbutton", {"up"})) ==
        "Argument \"button\" must be one of \"left\", \"middle\", or \"right\".");
  CHECK(all_text(s.color("pushbutton")) == "Usage: pushbutton <button>");
}

TEST_CASE("color map save and load use techStyle.displayStyle.monitorType", "[cap:ui-color-window][req:Color window commands]") {
  ColorSession s;
  (void)s.open("5");
  s.set_rgb(5, 1, 2, 3);
  s.editor.color_map.modified = true;
  auto& fs = s.services.view().files;  // in memory (PortableOptions::in_memory)
  const std::filesystem::path dir = "/cmaps";
  const std::string stem = (dir / "mos").string();
  REQUIRE(s.color("save", {stem, "OpenGL", "std"}).ok());
  CHECK_FALSE(s.editor.color_map.modified);
  CHECK(fs.exists(stem + ".OpenGL.std.cmap"));
  s.set_rgb(5, 9, 9, 9);
  REQUIRE(s.color("load", {stem, "OpenGL", "std"}).ok());
  CHECK(s.rgb(5) == std::array<int, 3>{1, 2, 3});
  CHECK(all_text(s.color("save", {"a", "b"})) == "Usage: save [techStyle displayStyle monitorType]");
  CHECK(all_text(s.color("load", {"a"})) == "Usage: load [techStyle displayStyle monitorType]");
  const auto missing = s.color("load", {(dir / "none").string(), "OpenGL", "std"});
  CHECK_FALSE(missing.ok());
  CHECK(missing.diagnostics.back().code == view::codes::kColorFile);
}

TEST_CASE("the default color map name follows the styles and monitor", "[cap:ui-color-window][req:Color window commands]") {
  // Scenario "Save default name": styletype mos, OpenGL, monitor std.
  ColorSession s;
  (void)s.open("0");
  // tests/data/tech/tiny.tech has `styletype mos`.
  std::ifstream in(std::string{HEFESTO_SOURCE_DIR} + "/tests/data/tech/tiny.tech");
  std::stringstream text;
  text << in.rdbuf();
  hefesto::platform::MemoryFileSystem tech_fs;
  hefesto::diag::CollectingSink sink;
  auto tech = hefesto::tech::load_technology_text(text.str(), "/t/tiny.tech", tech_fs, {}, sink);
  REQUIRE(tech.ok());
  s.editor.technology = tech.value();
  REQUIRE(s.color("save").ok());
  CHECK(s.services.view().files.exists("mos.OpenGL.std.cmap"));
}

// --- Unsaved color map protection ------------------------------------------------------------

TEST_CASE("quit and load ask before losing color map changes", "[cap:ui-color-window][req:Unsaved color map protection]") {
  ColorSession s;
  view::ColorWindow& w = s.open("5");
  CHECK(view::check_color_map_written(s.editor));  // unmodified: no question
  REQUIRE(view::press_button(s.editor, w, {6000, 8500}, view::Button::left));
  // Headless: the default answer is "no", which vetoes.
  CHECK_FALSE(view::check_color_map_written(s.editor));
  ScriptedHost yes(1);
  s.editor.prompt = &yes;
  CHECK(view::check_color_map_written(s.editor));
  CHECK(yes.asked == "The color map has been modified.\n  Do you want to lose the changes? ");
  // Scenario "Quit vetoed": the exit check installed by specialopen says no.
  ScriptedHost no(0);
  s.editor.prompt = &no;
  REQUIRE(s.script.exit_checks.size() == 1);
  CHECK_FALSE(s.script.exit_checks.front()());
  // `load` is cancelled too: the map stays as edited.
  REQUIRE(s.color("load", {"/nonexistent/x", "y", "z"}).ok());
  CHECK(s.rgb(5)[0] == 128);
}

TEST_CASE("the color window commands are in the color client's table", "[cap:ui-color-window][req:Color window commands]") {
  hefesto::cmd::CommandRegistry registry;
  REQUIRE(hefesto::view::register_view_commands(registry));
  for (const char* name : {"color", "load", "save", "pushbutton"}) {
    INFO(name);
    CHECK(registry.find(name, "color") != nullptr);
  }
  CHECK(registry.find("color", "layout") == nullptr);  // only the color client has `color`
}
