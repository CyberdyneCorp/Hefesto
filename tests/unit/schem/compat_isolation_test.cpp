// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `xschem-compat` governs only schematic output (capability
// hefesto-magic-compat-mode "xschem-compat setting for xschem quirks",
// tasks.md 5.3): switching it through the `setting` command leaves the
// layout output byte-identical. Hefesto has no `.ext` writer or LVS yet
// (M7); their parts of the scenario are added with them.
//
// Owner: SCAFFOLD (COMPAT's isolation check, over GDSW's fixture).

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <string_view>

#include "../gds/write/write_fixture.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/ui/line.hpp"
#include "hefesto/view/window.hpp"

namespace {

using namespace gds_write_test;  // NOLINT(google-build-using-namespace): the fixture's aliases

// A command session over GDSW's fixture: in-memory files and a window on
// `top` (metal1 (0,0)-(4,2)).
struct Session {
  Fixture f;
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  std::unique_ptr<cmd::Dispatcher> dispatcher;

  Session() {
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&f.ed);
    db::CellDef& top = f.cell("top");
    f.paint(top, f.m1, db::Rect{0, 0, 4, 2});
    REQUIRE(f.ed.windows.open(&top, hefesto::view::ScreenRect{0, 0, 999, 999}).ok());
  }

  cmd::CommandResult run(std::string_view line) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : hefesto::ui::run_command_line(*dispatcher, line, cmd::Origin::script)) {
      last = std::move(r);
    }
    return last;
  }

  // The GDS stream of `path` with the 24 date bytes of BGNLIB and BGNSTR
  // cleared (their modification date is the clock's).
  std::string gds_without_dates(const std::string& path) {
    auto read = services.view().files.read(path);
    REQUIRE(read.ok());
    std::string bytes = read.value();
    for (const std::size_t at : {std::size_t{22}, std::size_t{62 + 4 + 12}}) {
      REQUIRE(bytes.size() >= at + 12);
      for (std::size_t i = 0; i < 12; ++i) {
        bytes[at + i] = 0;
      }
    }
    return bytes;
  }
};

}  // namespace

TEST_CASE("xschem-compat off leaves layout output unchanged",
          "[cap:hefesto-magic-compat-mode][req:xschem-compat setting for xschem quirks]") {
  Session s;
  REQUIRE(s.run("gds datestamp 0").ok());
  REQUIRE(s.run("gds write before").ok());
  REQUIRE(s.run("setting set xschem-compat off").ok());
  CHECK_FALSE(s.project.compat.xschem_compat);
  CHECK(s.project.compat.magic_compat);
  REQUIRE(s.run("gds write after").ok());
  CHECK(s.gds_without_dates("before.gds") == s.gds_without_dates("after.gds"));
}
