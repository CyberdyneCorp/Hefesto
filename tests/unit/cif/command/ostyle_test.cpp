// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `cif ostyle <name>` through the dispatcher: selecting the output style
// that the drc section's `cifstyle` names reloads the DRC rules, as
// CIFLoadStyle's DRCReloadCurStyle does (cif/CIFtech.c:2297-2298).
// Owner: CIF.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/drc/rules/details.hpp"
#include "hefesto/drc/types.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"

namespace cmd = hefesto::cmd;

namespace {

/// tests/data/tech/tiny.tech with two cifoutput styles and a drc section
/// whose cif rules use the second one.
std::string technology_text() {
  std::ifstream in{std::filesystem::path{HEFESTO_SOURCE_DIR} / "tests/data/tech/tiny.tech", std::ios::binary};
  std::string text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
  const std::string empty = "cifoutput\nend\n\ncifinput\nend\n";
  const std::size_t at = text.find(empty);
  REQUIRE(at != std::string::npos);
  text.replace(at, empty.size(),
               "cifoutput\nstyle lambda\n    scalefactor 1\n    layer CM1 m1\n"
               "style coarse\n    scalefactor 10\n    layer CM1 m1\nend\n\ncifinput\nend\n");
  const std::string drc = "drc\nend\n";
  const std::size_t d = text.find(drc);
  REQUIRE(d != std::string::npos);
  text.replace(d, drc.size(), "drc\n    cifstyle coarse\n    cifwidth CM1 300 \"w\"\nend\n");
  return text;
}

struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  cmd::Dispatcher dispatcher{registry, services.view(), project, script};

  Session() {
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher.set_editor(&editor);
    REQUIRE(services.view().files.write_atomic("/t/ostyle.tech", technology_text()).ok());
    REQUIRE(run("tech", {"load", "/t/ostyle.tech", "-noprompt"}).ok());
  }

  cmd::CommandResult run(std::string name, std::vector<std::string> args) {
    return dispatcher.execute(cmd::Invocation{std::move(name), std::move(args), cmd::Origin::script, std::nullopt});
  }
};

}  // namespace

TEST_CASE("cif ostyle reloads a DRC style that binds the selected cifstyle",
          "[cap:mask-output-styles][req:Output style selection]") {
  Session s;
  REQUIRE(s.editor.drc.rules != nullptr);
  REQUIRE(s.editor.drc.rules->details() != nullptr);
  CHECK_FALSE(s.editor.drc.rules->details()->cif_bound);  // `lambda` is current after the load
  REQUIRE(s.run("cif", {"ostyle", "coarse"}).ok());
  CHECK(s.editor.drc.cif.current()->name == "coarse");
  REQUIRE(s.editor.drc.rules->details() != nullptr);
  CHECK(s.editor.drc.rules->details()->cif_bound);
  CHECK(s.editor.drc.rules->details()->mask_layer_rules.size() == 1);
}
