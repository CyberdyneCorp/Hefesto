// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The M4 security review (openspec/changes/m4-scripting tasks.md 13.3, design
// D19; docs/contributing/security.md): what a script reaches through the
// engine commands bound into its interpreter and through the trust store.
//
// The test cases named "security fix S-NN" are the attacks the review found
// not denied; each is the regression test of its fix (docs/contributing/
// security.md, "Findings and fixes").

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/pdk/manager.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/platform/process_runner.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/script/binding/bind.hpp"
#include "hefesto/script/pdk/tech_rc.hpp"
#include "hefesto/script/pdk/trust.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "pdk_fixture.hpp"

namespace script = hefesto::script;
namespace cmd = hefesto::cmd;
namespace fs = std::filesystem;
using script::PdkIdentity;
using script::test::TempDir;
using script::test::write_file;

#define ENFORCED "[cap:hefesto-security][req:Script sandbox enforcement]"
#define CONSENT "[cap:hefesto-security][req:Consent before implicit script execution]"

namespace {

const PdkIdentity kSky130{"sky130", "sky130A", "5740c4a81c42f9fe4af5ef2ee258a99151053d28"};

// A process runner that records each request and runs nothing.
class RecordingRunner final : public hefesto::platform::ProcessRunner {
 public:
  explicit RecordingRunner(std::vector<std::string>& programs) : programs_(programs) {}
  [[nodiscard]] bool supported() const noexcept override { return true; }
  hefesto::diag::Outcome<hefesto::platform::ProcessOutput> run(
      const hefesto::platform::ProcessRequest& request) override {
    programs_.push_back(request.program);
    return hefesto::platform::ProcessOutput{};
  }

 private:
  std::vector<std::string>& programs_;
};

// A host with no environment, for the PDK manager.
class NoHost final : public hefesto::pdk::InstallHost {
 public:
  hefesto::diag::Status make_symlink(const hefesto::platform::Path&, const hefesto::platform::Path&) override {
    return hefesto::diag::Done{};
  }
  hefesto::diag::Outcome<std::optional<hefesto::platform::Path>> read_symlink(const hefesto::platform::Path&) override {
    return std::optional<hefesto::platform::Path>{};
  }
  std::optional<std::string> environment(std::string_view) override { return std::nullopt; }
  hefesto::platform::Path home_directory() override { return {}; }
};

// A session whose interpreter grants `project` only (and the store root when
// given, as app startup does), with every engine command bound and a process
// runner that records what it was asked to run.
struct EngineSession {
  explicit EngineSession(const fs::path& project, const fs::path& store_root = {}) {
    services.set_process_runner(std::make_unique<RecordingRunner>(programs));
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), settings, session);
    dispatcher->set_editor(&editor);
    auto* unnamed = editor.cells.create("(UNNAMED)", editor.technology->plane_count()).value();
    unnamed->flags().available = true;
    editor.edit_cell = unnamed;
    editor.edit_root = unnamed;
    REQUIRE(editor.windows.open_frame(unnamed).ok());
    script::SandboxPolicy policy;
    policy.granted_roots.push_back(project);
    if (!store_root.empty()) {
      policy.read_only_roots.push_back(store_root);  // as app startup grants the PDK store (S-03)
      policy.environment["PDK_ROOT"] = store_root.string();
    }
    (void)script::open_project_interpreter(editor, std::move(policy));
    REQUIRE(script::bind_registry(*editor.script.interp, registry, *dispatcher).ok());
  }
  ~EngineSession() { script::close_project_interpreter(editor); }
  EngineSession(const EngineSession&) = delete;
  EngineSession& operator=(const EngineSession&) = delete;

  cmd::CommandResult run(std::string_view script) { return editor.script.interp->evaluate(script); }

  std::vector<std::string> programs;
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, false, true}};
  hefesto::settings::ProjectSettings settings;
  cmd::ScriptSession session{settings};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
};

}  // namespace

// ---- denied today ----------------------------------------------------------------------

TEST_CASE("security: the Tcl process and socket commands stay denied with the engine commands bound", ENFORCED) {
  TempDir project;
  EngineSession s{project.path()};
  for (const char* script : {"exec ls", "open {|ls}", "socket example.com 80", "magic::exec ls"}) {
    INFO(script);
    CHECK_FALSE(s.run(script).ok());
  }
  CHECK(s.programs.empty());
}

TEST_CASE("security: a script outside the store with no store record is not trusted", CONSENT) {
  TempDir store;
  write_file(store.path() / "ciel/sky130/versions" / kSky130.version / "sky130A/libs.tech/magic/sky130A.tech", "tech\n");
  hefesto::platform::LocalFileSystem fs;
  NoHost host;
  auto backend = script::make_pdk_store_trust(fs, host, store.path());
  // Never trusted: no family, no version (a PDK found outside the ciel layout).
  CHECK_FALSE(backend->trusted(PdkIdentity{"sky130A", "sky130A", ""}));
  CHECK_FALSE(backend->trusted(PdkIdentity{"no-such-family", "x", kSky130.version}));
  CHECK_FALSE(backend->trusted(kSky130));
}

// ---- findings of the review, fixed ----------------------------------------------------------

TEST_CASE("security fix S-01: the engine's shell command does not run a process for a script", ENFORCED) {
  TempDir project;
  EngineSession s{project.path()};
  const auto script = project.path() / "attack.tcl";
  write_file(script, "shell touch pwned\nmagic::shell id\n");
  (void)s.editor.script.interp->source(script);
  CHECK(s.programs.empty());  // today: {"/bin/sh", "/bin/sh"}
}

TEST_CASE("security fix S-02: engine commands that write files stay within the granted roots", ENFORCED) {
  TempDir base;
  const fs::path project = base.path() / "project";
  const fs::path outside = base.path() / "outside";
  fs::create_directories(project);
  fs::create_directories(outside);
  EngineSession s{project};
  (void)s.run("save " + (outside / "evil").string());
  (void)s.run("feedback save " + (outside / "feedback.txt").string());
  CHECK_FALSE(fs::exists(outside / "evil.mag"));
  CHECK_FALSE(fs::exists(outside / "feedback.txt"));
}

TEST_CASE("security fix S-03: a script cannot mark a PDK version trusted", CONSENT) {
  TempDir base;
  const fs::path project = fs::canonical(base.path()) / "project";
  const fs::path store = fs::canonical(base.path()) / "store";
  fs::create_directories(project);
  write_file(store / "ciel/sky130/versions" / kSky130.version / "sky130A/libs.tech/magic/sky130A.tech", "tech\n");
  {
    // The store root is granted read-only (as app startup grants it); a
    // script that was allowed to run once tries to write the version's
    // provenance record, and to run `pdk trust`.
    EngineSession s{project, store};
    (void)s.run("file mkdir $env(PDK_ROOT)/ciel/sky130/.hefesto\n"
                "set f [open $env(PDK_ROOT)/ciel/sky130/.hefesto/" + kSky130.version + ".json w]\n"
                "puts $f {{\"family\":\"sky130\",\"version\":\"" + kSky130.version + "\",\"trusted\":true}}\n"
                "close $f\n");
    const auto trusted = s.run("pdk trust sky130A " + kSky130.version);
    CHECK_FALSE(trusted.ok());
  }
  hefesto::platform::LocalFileSystem fs;
  NoHost host;
  CHECK_FALSE(script::make_pdk_store_trust(fs, host, store)->trusted(kSky130));
}

TEST_CASE("security fix S-09: an rc file outside the store does not inherit a store version's trust", CONSENT) {
  TempDir base;
  const fs::path store = base.path() / "store";
  write_file(store / "ciel/sky130/versions" / kSky130.version / "sky130A/libs.tech/magic/sky130A.tech", "tech\n");
  hefesto::platform::LocalFileSystem fs;
  NoHost host;
  REQUIRE(script::make_pdk_store_trust(fs, host, store)->record(kSky130).ok());  // the user trusted it
  // A downloaded folder that copies the store's directory layout.
  const fs::path rc = base.path() / "download/ciel/sky130/versions" / kSky130.version /
                      "sky130A/libs.tech/magic/sky130A.magicrc";
  write_file(rc, "set ::spoofed_rc_ran 1\n");
  const auto identity = script::pdk_identity_of(rc);
  REQUIRE(identity.has_value());
  CHECK(*identity == kSky130);  // the identity comes from the path alone

  EngineSession s{base.path()};  // the rc file is readable: only consent stands in the way
  s.editor.script.pdk_trust.set_backend(script::make_pdk_store_trust(fs, host, store));
  const auto run = script::run_tech_magicrc(s.editor, *identity, rc, nullptr);  // no consent host: decline
  CHECK_FALSE(run.ok());
  CHECK_FALSE(s.editor.script.interp->get_var("spoofed_rc_ran").has_value());
}

TEST_CASE("security fix S-01: a script's procedure called from a typed line is still a script", ENFORCED) {
  TempDir project;
  EngineSession s{project.path()};
  const auto script = project.path() / "helpers.tcl";
  write_file(script, "proc run_it {} { shell id }\n");
  REQUIRE(s.editor.script.interp->source(script).ok());
  (void)s.editor.script.interp->evaluate("run_it", cmd::Origin::gui);
  CHECK(s.programs.empty());
  // Key macros can be defined by scripts: they count as scripts too.
  (void)s.dispatcher->execute(cmd::Invocation{"shell", {"id"}, cmd::Origin::key, std::nullopt});
  CHECK(s.programs.empty());
}

TEST_CASE("security fix S-01: shell is refused for a typed line too", ENFORCED) {
  // User decision 2026-11-11 (docs/contributing/security.md, S-01): no
  // origin runs a process, typed console, command-bar and hefesto-cli lines
  // included. Before it, a typed line ran /bin/sh.
  TempDir project;
  EngineSession s{project.path()};
  for (const auto origin : {cmd::Origin::gui, cmd::Origin::cli}) {
    // The Tcl error a console shows (SCRIPT-TCL-001 carries the command's message).
    const auto typed = s.editor.script.interp->evaluate("shell id", origin);
    REQUIRE_FALSE(typed.ok());
    REQUIRE_FALSE(typed.diagnostics.empty());
    CHECK(typed.diagnostics.back().text ==
          "shell: process execution is not permitted (Hefesto runs no external programs)");
    // The command's own refusal, as the dispatcher reports it.
    const auto direct = s.dispatcher->execute(cmd::Invocation{"shell", {"id"}, origin, std::nullopt});
    REQUIRE_FALSE(direct.diagnostics.empty());
    CHECK(direct.diagnostics.back().code == "SCRIPT-SANDBOX-001");
  }
  CHECK(s.programs.empty());
}

TEST_CASE("security fix S-03: pdk subcommands that change trust or the store refuse a script", CONSENT) {
  TempDir project;
  EngineSession s{project.path()};
  for (const char* line : {"pdk trust sky130A abc", "pdk untrust sky130A abc", "pdk enable sky130 abc",
                           "pdk uninstall sky130 abc", "pdk install sky130A", "pdk pin sky130A"}) {
    INFO(line);
    const auto result = s.run(line);
    CHECK_FALSE(result.ok());
    CHECK(s.programs.empty());
  }
}

TEST_CASE("security fix S-02: an engine command never reads a device file", ENFORCED) {
  // /dev granted as if it were the project: the read is still refused, at once.
  EngineSession s{fs::path{"/dev"}};
  const auto result = s.run("readspice /dev/zero");
  CHECK(s.programs.empty());
  static_cast<void>(result);  // readspice reports the failure quietly; reaching this line is the test
}
