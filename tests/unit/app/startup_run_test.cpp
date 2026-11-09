// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// app-startup: the startup sequence (run_startup). `tech` and `load` are
// replaced by recording commands, so these tests check what startup asks
// for, independent of the TECH and CELL implementations.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/app/diag_codes.hpp"
#include "hefesto/app/project_file.hpp"
#include "hefesto/app/startup.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/script.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "temp_dir.hpp"

namespace app = hefesto::app;
namespace cmd = hefesto::cmd;
namespace diag = hefesto::diag;
using hefesto::test::TempDir;
using hefesto::test::write_file;

namespace {

constexpr const char* kHashA = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr const char* kHashB = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

// A session whose `tech`, `load` and `lef` commands only record their arguments.
class Session {
 public:
  Session()
      : services_(hefesto::platform::PortableOptions{.storage_root = "/storage", .pdk_roots = {}, .in_memory = true,
                                                     .inline_tasks = true}),
        dispatcher_(registry_, services_.view(), project_, script_session_) {
    for (const char* name : {"tech", "load", "lef"}) {
      (void)registry_.add(cmd::CommandSpec{name, name, "recording fake", [this, name](cmd::CommandContext&, const cmd::Invocation& inv) {
                                             std::string line = name;
                                             for (const auto& a : inv.args) {
                                               line += " " + a;
                                             }
                                             calls.push_back(line);
                                             if (line.find("fail") != std::string::npos) {
                                               return cmd::CommandResult::failure(diag::make("CMD-USAGE-001", "fake failure"));
                                             }
                                             return cmd::CommandResult::success();
                                           }});
    }
    dispatcher_.set_editor(&editor);
    env.home = dir.path() / "home";
    env.working_directory = dir.path() / "work";
    std::filesystem::create_directories(env.home);
    std::filesystem::create_directories(env.working_directory);
  }

  diag::Outcome<app::StartupResult> start(const app::StartupOptions& options) {
    return app::run_startup(dispatcher_, project_, fs, options, env, sink);
  }

  [[nodiscard]] bool reported(std::string_view code, std::string_view text = {}) {
    for (const auto& d : sink.snapshot()) {
      if (d.code == code && d.text.find(text) != std::string::npos) {
        return true;
      }
    }
    return false;
  }

  // A ciel install of sky130A version `hash` under `root` with a tech and startup file.
  static void install(const std::filesystem::path& root, const std::string& hash) {
    const auto magic = root / "ciel/sky130/versions" / hash / "sky130A/libs.tech/magic";
    write_file(magic / "sky130A.tech", "tech\n  sky130A\nend\n");
    write_file(magic / "sky130A.magicrc", "set VDD VPWR\nset GND VGND\n");
  }

  TempDir dir;
  hefesto::platform::LocalFileSystem fs;
  app::StartupEnvironment env;
  diag::CollectingSink sink;
  hefesto::editor::EditorState editor;
  hefesto::settings::ProjectSettings project_;
  std::vector<std::string> calls;

 private:
  hefesto::platform::PortableServices services_;
  cmd::ScriptSession script_session_{project_};
  cmd::CommandRegistry registry_;
  cmd::Dispatcher dispatcher_;
};

app::StartupOptions with_files(std::vector<std::string> names) {
  app::StartupOptions options;
  for (const auto& name : names) {
    options.files.push_back(app::classify_file_argument(name));
  }
  return options;
}

}  // namespace

TEST_CASE("nothing specified keeps the minimum technology", "[cap:app-startup][req:Default technology selection]") {
  Session s;
  const auto r = s.start({});
  REQUIRE(r.ok());
  CHECK(r.value().technology == "minimum");
  CHECK(s.calls.empty());
}

TEST_CASE("startup opens the layout window unless -nowindow", "[cap:app-startup][req:Startup file order]") {
  Session s;
  REQUIRE(s.start({}).ok());
  REQUIRE(s.editor.windows.current() != nullptr);  // a null-display frame, before any file is loaded
  CHECK(s.editor.windows.current()->root() == nullptr);
  CHECK_FALSE(s.editor.styles.has_value());  // the minimum technology names no styletype files here
  Session headless;
  app::StartupOptions options;
  options.no_window = true;
  REQUIRE(headless.start(options).ok());
  CHECK(headless.editor.windows.current() == nullptr);
}

TEST_CASE("the technology of the first file is loaded before the file",
          "[cap:app-startup][req:Default technology selection]") {
  Session s;
  write_file(s.env.working_directory / "inv.mag", "magic\ntech sky130A\nmagscale 1 2\n");
  REQUIRE(s.start(with_files({"inv.mag"})).ok());
  CHECK(s.calls == std::vector<std::string>{"tech load sky130A", "load inv"});
  CHECK(s.reported(app::codes::kFileLoading, "Loading \"inv\" from command line."));
}

TEST_CASE("option T overrides the file's technology; a failed -T is retried, not the file",
          "[cap:app-startup][req:Startup file order]") {
  Session s;
  write_file(s.env.working_directory / "inv.mag", "magic\ntech sky130A\n");
  app::StartupOptions options = with_files({"inv.mag"});
  options.technology = "gf180mcuD";
  REQUIRE(s.start(options).ok());
  CHECK(s.calls == std::vector<std::string>{"tech load gf180mcuD", "load inv"});

  Session f;
  write_file(f.env.working_directory / "inv.mag", "magic\ntech sky130A\n");
  options.technology = "failtech";
  const auto r = f.start(options);
  REQUIRE(r.ok());
  CHECK(f.calls == std::vector<std::string>{"tech load failtech", "tech load failtech", "load inv"});
  CHECK(f.reported(app::codes::kTechFailed, "Failed to load technology \"failtech\""));
  CHECK(r.value().technology == "minimum");
}

TEST_CASE("files load in command-line order and the window ends on the first",
          "[cap:app-startup][req:Command-line file arguments]") {
  Session s;
  REQUIRE(s.start(with_files({"a.mag", "b"})).ok());
  CHECK(s.calls == std::vector<std::string>{"load a", "load b", "load a"});
}

TEST_CASE("LEF files are read; unsupported types are reported and skipped",
          "[cap:app-startup][req:Command-line file arguments]") {
  Session s;
  const auto r = s.start(with_files({"cells.lef", "top.gds"}));
  REQUIRE(r.ok());
  CHECK(s.calls == std::vector<std::string>{"lef read cells.lef"});
  CHECK(s.reported(app::codes::kFileUnsupported, "GDS file \"top.gds\""));
  CHECK(r.value().loaded_files == std::vector<std::string>{"cells.lef"});
}

TEST_CASE("a .tcl argument is evaluated as a command script", "[cap:app-startup][req:Command-line file arguments]") {
  Session s;
  const auto script = s.dir.path() / "init.tcl";
  write_file(script, "load fromscript\n");
  REQUIRE(s.start(with_files({script.string()})).ok());
  CHECK(s.calls == std::vector<std::string>{"load fromscript"});
}

TEST_CASE("option r reports that crash recovery is not available yet", "[cap:app-startup][req:Crash recovery at startup]") {
  Session s;
  app::StartupOptions options = with_files({"a"});
  options.recover = true;
  REQUIRE(s.start(options).ok());
  CHECK(s.calls.empty());  // recovery replaces loading the command-line files
  CHECK(s.reported(app::codes::kRecoverUnavailable));
}

TEST_CASE("startup without an editor session fails", "[cap:app-startup][req:Startup sequence]") {
  hefesto::platform::PortableServices services{
      hefesto::platform::PortableOptions{.storage_root = "/s", .pdk_roots = {}, .in_memory = true, .inline_tasks = true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession session{project};
  cmd::CommandRegistry registry;
  cmd::Dispatcher dispatcher{registry, services.view(), project, session};
  diag::NullSink sink;
  const auto r = app::run_startup(dispatcher, project, services.files(), {}, {}, sink);
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().code == diag::codes::kCmdSession);
}

TEST_CASE("settings layers: user preferences, project, session; compat handed to the project settings",
          "[cap:hefesto-settings][req:Layered settings resolution]") {
  Session s;
  write_file(s.dir.path() / "prefs.json", R"({"units":"lambda","magic-compat":"off"})");
  s.env.user_preferences = s.dir.path() / "prefs.json";
  app::ProjectFile project;
  project.extra_settings["units"] = "microns";
  project.settings.compat.magic_compat = true;
  const auto folder = s.dir.path() / "proj";
  std::filesystem::create_directories(folder);
  REQUIRE(app::save_project(s.fs, folder, project).ok());
  s.env.project_folder = folder;
  app::StartupOptions options;
  options.session_settings = {{"snap", "internal"}, {"script-strict", "on"}};
  const auto r = s.start(options);
  REQUIRE(r.ok());
  REQUIRE(r.value().project.has_value());
  CHECK(s.editor.settings.effective("units")->value == "microns");
  CHECK(s.editor.settings.effective("units")->layer == app::SettingLayer::project);
  CHECK(s.editor.settings.effective("snap")->layer == app::SettingLayer::session);
  CHECK(s.project_.compat.magic_compat);  // project beats the user preference
  CHECK(s.project_.script_strict);        // session
}

TEST_CASE("startup files are located and their plain settings applied", "[cap:hefesto-settings][req:Magic startup-file compatibility]") {
  Session s;
  write_file(s.env.home / ".magicrc", "set GND VSS\nmacro a \"box\"\n");
  write_file(s.env.working_directory / ".magic", "set SUB VSUBS\n");
  const auto r = s.start({});
  REQUIRE(r.ok());
  REQUIRE(r.value().startup_files.size() == 2);
  CHECK(s.editor.settings.effective("gnd")->value == "VSS");
  CHECK(s.editor.settings.effective("gnd")->layer == app::SettingLayer::user);
  CHECK(s.editor.settings.effective("sub")->value == "VSUBS");
  CHECK(s.reported(app::codes::kStartupFile, "1 plain setting(s) applied"));
  CHECK(s.reported(app::codes::kStartupNote,
                   R"(Note:  Use of the file name ".magic" is deprecated.  Please change this to ".magicrc".)"));
}

TEST_CASE("a project opens with its pinned version, not the enabled one",
          "[cap:hefesto-pdk-manager][req:Per-project version pinning]") {
  Session s;
  const auto root = s.dir.path() / "pdks";
  Session::install(root, kHashA);
  Session::install(root, kHashB);
  write_file(root / "ciel/sky130/current", std::string{kHashB} + "\n");
  app::ProjectFile project;
  project.pdk = app::PdkPin{"sky130", "sky130A", kHashA, {}};
  const auto folder = s.dir.path() / "proj";
  std::filesystem::create_directories(folder);
  REQUIRE(app::save_project(s.fs, folder, project).ok());
  s.env.project_folder = folder;
  s.env.pdk_root = root;
  const auto r = s.start({});
  REQUIRE(r.ok());
  REQUIRE(s.calls.size() == 1);
  CHECK(s.calls[0] == "tech load " + (root / "ciel/sky130/versions" / kHashA / "sky130A/libs.tech/magic/sky130A.tech").string());
  // The pinned version's startup file seeds the PDK layer.
  CHECK(s.editor.settings.effective("vdd")->value == "VPWR");
  CHECK(s.editor.settings.effective("vdd")->layer == app::SettingLayer::pdk);
}

TEST_CASE("a missing pinned version is reported and the minimum technology is kept",
          "[cap:hefesto-pdk-manager][req:Per-project version pinning]") {
  Session s;
  const auto root = s.dir.path() / "pdks";
  Session::install(root, kHashB);  // another version is installed and enabled
  write_file(root / "ciel/sky130/current", std::string{kHashB} + "\n");
  app::ProjectFile project;
  project.pdk = app::PdkPin{"sky130", "sky130A", kHashA, {}};
  const auto folder = s.dir.path() / "proj";
  std::filesystem::create_directories(folder);
  REQUIRE(app::save_project(s.fs, folder, project).ok());
  write_file(s.env.working_directory / "inv.mag", "magic\ntech sky130A\n");
  s.env.project_folder = folder;
  s.env.pdk_root = root;
  app::StartupOptions options = with_files({"inv"});
  const auto r = s.start(options);
  REQUIRE(r.ok());
  CHECK(s.reported(app::codes::kPinMissing, std::string{"pdk install sky130A "} + kHashA));
  CHECK(s.calls == std::vector<std::string>{"load inv"});  // no substitute version, no file technology
  CHECK(r.value().technology == "minimum");
}

TEST_CASE("a technology name resolves through the PDK root", "[cap:hefesto-files-and-pdks][req:PDK store]") {
  Session s;
  const auto root = s.dir.path() / "pdks";
  Session::install(root, kHashB);
  write_file(root / "ciel/sky130/current", std::string{kHashB} + "\n");
  s.env.pdk_root = root;
  app::StartupOptions options;
  options.technology = "sky130A";
  REQUIRE(s.start(options).ok());
  REQUIRE(s.calls.size() == 1);
  CHECK(s.calls[0] == "tech load " + (root / "ciel/sky130/versions" / kHashB / "sky130A/libs.tech/magic/sky130A.tech").string());
}

TEST_CASE("an explicit PDK root becomes the session pdk-root setting",
          "[cap:hefesto-pdk-manager][req:Install layout compatible with ciel]") {
  Session s;
  s.env.pdk_root = s.dir.path() / "pdks";
  REQUIRE(s.start({}).ok());
  const auto root = s.editor.settings.effective("pdk-root");
  REQUIRE(root.has_value());
  CHECK(root->value == (s.dir.path() / "pdks").string());
  CHECK(root->layer == app::SettingLayer::session);
}
