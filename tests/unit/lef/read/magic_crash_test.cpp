// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Hefesto-only regression: the first input of golden case MASK-OS-016, on
// which Magic 8.3.694 exits with SIGSEGV (golden-capture run 44743146734;
// kept under tests/golden/MASK/magic-crash/, since the case was
// re-authored). `lef read` of `VIA V1`, a name the tech section's
// `cut via1 via1 V1` already binds to the shared via1 cut record, is a via
// redefinition of one alias (Q-LEFDEF-8, fixed in both modes), and then
// `lef write cont -tech` writes the technology section and the via. Both
// modes must run it without fault.
//
// Owner: LEFR (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/ui/line.hpp"

namespace {

namespace cmd = hefesto::cmd;
namespace diag = hefesto::diag;

/// One file of tests/golden/MASK/magic-crash/MASK-OS-016/.
std::string crash_input(std::string_view name) {
  const std::filesystem::path path =
      std::filesystem::path{HEFESTO_SOURCE_DIR} / "tests/golden/MASK/magic-crash/MASK-OS-016" / name;
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  diag::CollectingSink printed;
  std::vector<diag::Diagnostic> messages;

  explicit Session(bool magic_compat) {
    project.compat.magic_compat = magic_compat;
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    dispatcher->add_sink(printed);
  }

  void file(const std::string& path, const std::string& text) {
    REQUIRE(services.view().files.write_atomic(path, text).ok());
  }

  [[nodiscard]] std::string read(const std::string& path) {
    auto text = services.view().files.read(path);
    REQUIRE(text.ok());
    return text.value();
  }

  cmd::CommandResult run(std::string_view line) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : hefesto::ui::run_command_line(*dispatcher, line, cmd::Origin::script)) {
      for (const auto& d : r.diagnostics) {
        messages.push_back(d);
      }
      last = std::move(r);
    }
    for (const auto& d : printed.take()) {
      messages.push_back(d);
    }
    return last;
  }

  [[nodiscard]] bool said(std::string_view text) const {
    return std::any_of(messages.begin(), messages.end(),
                       [&](const diag::Diagnostic& d) { return d.text.find(text) != std::string::npos; });
  }
};

}  // namespace

TEST_CASE("MASK-OS-016's first input: a LEF VIA named after the tech section's cut alias, then lef write -tech",
          "[cap:lef-read][req:Via redefinition][magic-crash]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s(on);
    s.file("/t/masktest.tech", crash_input("masktest.tech"));
    s.file("/t/v.lef", crash_input("v.lef"));
    for (const char* line : {"drc off", "tech load /t/masktest -noprompt", "snap internal", "load cont"}) {
      REQUIRE(s.run(line).ok());
    }
    // Magic's batch session has its layout window; the script points at it.
    hefesto::db::CellDef* cont = s.editor.cells.find("cont");
    REQUIRE(cont != nullptr);
    REQUIRE(s.editor.windows.open(cont, hefesto::view::ScreenRect{0, 0, 499, 499}).ok());
    for (const char* line : {"lef read /t/v.lef", "box 0 0 4 4", "paint via1"}) {
      INFO(line);
      const auto result = s.run(line);
      for (const auto& d : result.diagnostics) {
        UNSCOPED_INFO(d.code << ": " << d.text);
      }
      REQUIRE(result.ok());
    }
    CHECK(s.said("Cut type \"V1\" redefined."));
    REQUIRE(s.run("lef write cont -tech").ok());
    for (const auto& d : s.messages) {
      UNSCOPED_INFO(d.code << ": " << d.text);
    }
    const std::string lef = s.read("cont.lef");
    UNSCOPED_INFO(lef);
    // The redefined name is a via record of its own and the cut layer keeps
    // its other name (Q-LEFDEF-8). The via record is a LAYER block only with
    // magic-compat ON (Q-LEFDEF-127): its cut is the grown via1 cut.
    CHECK(lef.find("LAYER via1\n  TYPE CUT ;\n") != std::string::npos);
    CHECK((lef.find("LAYER V1\n  TYPE CUT ;\n  CUT AREA ") != std::string::npos) == on);
    CHECK((lef.find("END V1\n") != std::string::npos) == on);
    CHECK(lef.find("MACRO cont\n") != std::string::npos);
    CHECK(lef.find("END LIBRARY\n") != std::string::npos);
  }
}
