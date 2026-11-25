// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// File › Read GDS and Write GDS (openspec/changes/archive/2026-11-24-m5-mask-gds-cif tasks.md
// 12.1, design D16): enabled in M5, each item issues its logged
// `magic::promptload gds` / `magic::promptsave gds`, which asks the native
// picker and runs `gds read` / `gds write`.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/gui/wrapper.hpp"
#include "hefesto/platform/file_picker.hpp"
#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/gui_script.hpp"
#include "hefesto/shell/menu_bar_model.hpp"
#include "hefesto/ui/line.hpp"
#include "shell_fixture.hpp"

using namespace hefesto;
using namespace hefesto::shell;

namespace {

constexpr std::string_view kTech =
    "tech\n format 35\n gdsmenu\nend\nplanes\n metal1,m1\nend\ntypes\n metal1 metal1,m1\nend\n"
    "contact\nend\nstyles\n styletype mos\n metal1 20\nend\ncompose\nend\nconnect\nend\n"
    "cifoutput\nstyle gdsii\n scalefactor 10\n layer CM1 metal1\n  calma 49 0\nend\n"
    "cifinput\nstyle gdsii\n scalefactor 10\n layer metal1 CM1\n  calma CM1 49 0\nend\n"
    "drc\nend\nextract\nend\n";

const MenuBarItem& file_item(const MenuBarView& view, std::string_view label) {
  const auto menu = std::find_if(view.menus.begin(), view.menus.end(),
                                 [](const MenuBarMenu& m) { return m.title == "File"; });
  REQUIRE(menu != view.menus.end());
  const auto found = std::find_if(menu->items.begin(), menu->items.end(),
                                  [label](const MenuBarItem& i) { return i.label == label; });
  REQUIRE(found != menu->items.end());
  return *found;
}

/// A desktop core session with its interpreter, a scripted picker and the
/// technology loaded; `cell` (if given) is drawn in the window.
struct MenuSession {
  CoreSession core{platform::PortableOptions{.storage_root = "/app", .pdk_roots = {}, .in_memory = true,
                                             .inline_tasks = true}};
  platform::ScriptedFilePicker picker;

  explicit MenuSession(const std::string& cell) {
    (void)hefesto::shell::test::bind_interpreter(core);
    core.editor().file_picker = &picker;
    REQUIRE(core.files().create_directories("/work").ok());
    REQUIRE(core.files().write_atomic("/work/gdsmenu.tech", std::string{kTech}).ok());
    ok("tech load /work/gdsmenu.tech -noprompt");
    ok("load " + cell);
    db::CellDef* def = core.editor().cells.find(cell);
    REQUIRE(def != nullptr);
    if (core.editor().windows.current() == nullptr) REQUIRE(core.editor().windows.open_frame(def).ok());
  }

  void ok(std::string_view line) {
    for (auto& r : hefesto::ui::run_command_line(core.dispatcher(), line, cmd::Origin::gui)) {
      INFO(line << " -> " << (r.diagnostics.empty() ? std::string{} : r.diagnostics.back().text));
      REQUIRE(r.ok());
    }
  }

  /// Chooses the item as the window does.
  void choose(const MenuBarItem& item) {
    const cmd::CommandResult r = run_gui_script(core.editor(), core.dispatcher(), menu_item_script(item));
    INFO(gui_result_text(r));
    REQUIRE(r.ok());
  }

  [[nodiscard]] bool logged(std::string_view line) const {
    const std::vector<std::string> history = core.command_log().history();
    return std::find(history.begin(), history.end(), line) != history.end();
  }
};

}  // namespace

TEST_CASE("Read GDS and Write GDS are enabled", "[cap:ui-gui-managers][req:Wrapper menus]") {
  const MenuBarView view = menu_bar_view(gui::wrapper_menus(".layout1"), {}, {}, {});
  const MenuBarItem& read_item = file_item(view, "Read GDS");
  const MenuBarItem& write_item = file_item(view, "Write GDS");
  CHECK(read_item.enabled);
  CHECK(write_item.enabled);
  CHECK(read_item.tooltip.empty());
  CHECK(write_item.tooltip.empty());
  CHECK(read_item.script == "magic::promptload gds");
  CHECK(write_item.script == "magic::promptsave gds");
  CHECK(later_milestone("magic::promptload gds") == std::nullopt);
  CHECK(later_milestone("magic::promptsave gds") == std::nullopt);
  // Magic's wrapper has no CIF item; `promptload cif` stays a console command.
  const auto file = std::find_if(view.menus.begin(), view.menus.end(),
                                 [](const MenuBarMenu& m) { return m.title == "File"; });
  REQUIRE(file != view.menus.end());
  CHECK(std::none_of(file->items.begin(), file->items.end(),
                     [](const MenuBarItem& i) { return i.script.find(" cif") != std::string::npos; }));
}

TEST_CASE("Write GDS then Read GDS issue their logged commands", "[cap:ui-gui-managers][req:File prompts]") {
  const MenuBarView view = menu_bar_view(gui::wrapper_menus(".layout1"), {}, {}, {});
  std::string bytes;
  {
    MenuSession writer("chip");
    writer.ok("snap internal; box 0 0 4 2; paint m1");
    writer.picker.push_answer(std::string{"/work/chip.gds"});
    writer.choose(file_item(view, "Write GDS"));
    CHECK(writer.picker.requests().size() == 1);
    // The command log holds the item's command (resolved name, as typed
    // otherwise); the `gds write` it runs is a nested command.
    CHECK(writer.logged("promptsave gds"));
    auto written = writer.core.files().read("/work/chip.gds");
    REQUIRE(written.ok());
    bytes = written.value();
  }
  MenuSession reader("scratch");
  REQUIRE(reader.core.files().write_atomic("/work/chip.gds", bytes).ok());
  reader.picker.push_answer(std::string{"/work/chip.gds"});
  reader.choose(file_item(view, "Read GDS"));
  CHECK(reader.logged("promptload gds"));
  const view::LayoutWindow* window = reader.core.editor().windows.current();
  REQUIRE(window != nullptr);
  REQUIRE(window->root() != nullptr);
  CHECK(window->root()->name() == "chip");
}
