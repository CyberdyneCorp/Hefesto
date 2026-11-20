// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The M6 desktop items (openspec/changes/m6-lef-def-oasis tasks.md 14.1,
// 14.2; design D16): File > Open and drag and drop through the generic open
// path, so an OASIS file is recognised by content whatever its name, and
// `oasis read` / `oasis write` through the M5 progress and cancel hosts as
// the main window wires them (MaskSession::read_host, write_host,
// emit_host).

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>
#include <vector>

#include "hefesto/gds/stream/io.hpp"
#include "hefesto/gui/wrapper.hpp"
#include "hefesto/mask/session.hpp"
#include "hefesto/oasis/codec/encoder.hpp"
#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/mask_job_host.hpp"
#include "hefesto/shell/menu_bar_model.hpp"
#include "hefesto/shell/open_files.hpp"
#include "hefesto/ui/line.hpp"
#include "shell_fixture.hpp"

using namespace hefesto;
using namespace hefesto::shell;

namespace {

// metal1 read from and written to GDS/OASIS layer 49/0.
constexpr std::string_view kTech =
    "tech\n format 35\n m6open\nend\nplanes\n metal1,m1\nend\ntypes\n metal1 metal1,m1\nend\n"
    "contact\nend\nstyles\n styletype mos\n metal1 20\nend\ncompose\nend\nconnect\nend\n"
    "cifoutput\nstyle gdsii\n scalefactor 10\n layer CM1 metal1\n  calma 49 0\nend\n"
    "cifinput\nstyle gdsii\n scalefactor 10\n layer metal1 CM1\n  calma CM1 49 0\nend\n"
    "drc\nend\nextract\nend\n";

platform::PortableOptions test_options() {
  return platform::PortableOptions{.storage_root = "/app", .pdk_roots = {}, .in_memory = true, .inline_tasks = true};
}

/// An OASIS file: cell `inv` (a rectangle on 49/0) placed in `TOP`.
std::string oasis_file() {
  gds::MemorySink sink;
  oasis::Encoder encoder(sink, oasis::EncoderOptions{false, oasis::ValidationScheme::crc32, 0, 1000.0});
  REQUIRE(encoder.begin().ok());
  REQUIRE(encoder.begin_cell("inv").ok());
  oasis::Element r;
  r.kind = oasis::ElementKind::rectangle;
  r.layer = 49;
  r.size = {100, 200};
  REQUIRE(encoder.element(r).ok());
  REQUIRE(encoder.begin_cell("TOP").ok());
  oasis::Element p;
  p.kind = oasis::ElementKind::placement;
  p.name = "inv";
  REQUIRE(encoder.element(p).ok());
  REQUIRE(encoder.finish().ok());
  const std::vector<std::byte>& bytes = sink.bytes();
  return std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

struct OpenSession {
  CoreSession core{test_options()};

  OpenSession() {
    (void)hefesto::shell::test::bind_interpreter(core);
    REQUIRE(core.files().create_directories("/work").ok());
    REQUIRE(core.files().write_atomic("/work/m6open.tech", std::string{kTech}).ok());
    ok("tech load /work/m6open.tech -noprompt");
    ok("load top");
    if (core.editor().windows.current() == nullptr) {
      REQUIRE(core.editor().windows.open_frame(core.editor().cells.find("top")).ok());
    }
    REQUIRE(core.files().write_atomic("/work/chip.gds", oasis_file()).ok());
  }

  void ok(std::string_view line) {
    for (auto& r : hefesto::ui::run_command_line(core.dispatcher(), line, cmd::Origin::gui)) {
      INFO(line << " -> " << (r.diagnostics.empty() ? std::string{} : r.diagnostics.back().text));
      REQUIRE(r.ok());
    }
  }

  std::vector<cmd::CommandResult> run(std::string_view line) {
    return hefesto::ui::run_command_line(core.dispatcher(), line, cmd::Origin::gui);
  }

  /// MainWindow::open_paths.
  void open(const std::vector<std::string>& paths) {
    const OpenPlan plan = plan_open(core.files(), paths);
    REQUIRE(plan.problems.empty());
    for (const std::string& line : plan.lines) ok(line);
  }
};

}  // namespace

TEST_CASE("the desktop opens an OASIS file named .gds as OASIS",
          "[cap:oasis-command][req:Format recognition on open]") {
  OpenSession s;
  const OpenPlan plan = plan_open(s.core.files(), std::vector<std::string>{"/work/chip.gds"});
  REQUIRE(plan.lines == std::vector<std::string>{"oasis read /work/chip.gds"});
  s.open({"/work/chip.gds"});
  REQUIRE(s.core.editor().cells.find("inv") != nullptr);
  CHECK(s.core.editor().windows.current()->root()->name() == "TOP");
  CHECK(kOpenFilterPatterns == "*");  // the picker offers every file
}

TEST_CASE("a dropped file that is not a layout is reported and the others open",
          "[cap:oasis-command][req:Format recognition on open]") {
  OpenSession s;
  REQUIRE(s.core.files().write_atomic("/work/notes.txt", "VERSION 5.8 ;\n").ok());
  const OpenPlan plan = plan_open(s.core.files(), std::vector<std::string>{"/work/notes.txt", "/work/chip.gds"});
  REQUIRE(plan.problems.size() == 1);
  CHECK(plan.problems.front().code == "OASIS-CMD-005");
  CHECK(plan.lines == std::vector<std::string>{"oasis read /work/chip.gds"});
}

TEST_CASE("oasis read runs through the desktop's read host",
          "[cap:mask-hierarchical-generation][req:Progress reporting and interruptibility]") {
  OpenSession s;
  MaskJobHost host(s.core.editor().database_lock);
  std::vector<std::string> labels;
  std::size_t frames = 0;
  host.set_pump([&frames] { ++frames; }, std::chrono::milliseconds{0});
  s.core.editor().mask.read_host = [&](std::string_view label, const mask::ProgressWork& work) {
    labels.emplace_back(label);
    return host.run_read(label, work);
  };
  s.ok("oasis read /work/chip.gds");
  CHECK(labels == std::vector<std::string>{"chip.gds"});
  CHECK(s.core.editor().cells.find("inv") != nullptr);
  CHECK_FALSE(host.busy());
  s.core.editor().mask.read_host = {};
}

TEST_CASE("a cancelled OASIS write leaves no output file",
          "[cap:mask-hierarchical-generation][req:Progress reporting and interruptibility]") {
  OpenSession s;
  s.open({"/work/chip.gds"});
  MaskJobHost host(s.core.editor().database_lock);
  mask::MaskSession& m = s.core.editor().mask;
  std::vector<std::string> labels;
  m.write_host = [&](std::string_view label, const mask::ProgressWork& work) {
    labels.emplace_back(label);
    return host.run_job(MaskJobKind::write, label, work);
  };
  m.emit_host = [&host](std::string_view label, const mask::ProgressWork& work) {
    return host.run_emit(label, work);
  };
  // The Interrupt arrives with the first frame the write's progress draws.
  host.set_pump([&host] { (void)host.request_cancel(); }, std::chrono::milliseconds{0});
  const std::vector<cmd::CommandResult> stopped = s.run("oasis write /work/early");
  REQUIRE(stopped.size() == 1);
  CHECK_FALSE(stopped.front().ok());
  CHECK(labels == std::vector<std::string>{"early.oas"});
  CHECK_FALSE(s.core.files().exists("/work/early.oas"));
  const auto listed = s.core.files().list("/work");
  REQUIRE(listed.ok());
  CHECK(listed.value().size() == 2);  // m6open.tech and chip.gds: no temporary file is left
  m.write_host = {};
  m.emit_host = {};
}

TEST_CASE("the desktop adds no LEF, DEF or OASIS menu item (Open Question 5 (a))",
          "[cap:ui-gui-managers][req:Wrapper menus]") {
  const MenuBarView view = menu_bar_view(gui::wrapper_menus(".layout1"), {}, {}, {});
  std::vector<std::string> titles;
  for (const MenuBarMenu& m : view.menus) {
    titles.push_back(m.title);
    for (const MenuBarItem& i : m.items) {
      INFO(m.title << " > " << i.label << ": " << i.script);
      for (const std::string command : {"lef", "def", "oasis"}) {
        for (const std::string& prefix : {command + " ", "magic::" + command + " "}) {
          CHECK_FALSE(i.script.starts_with(prefix));
        }
        CHECK(i.script != command);
      }
    }
  }
  CHECK(titles == std::vector<std::string>{"File", "Edit", "Cell", "Window", "Layers", "Drc", "Options"});
}
