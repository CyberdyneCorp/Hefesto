// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-pdk-manager "Headless and scripted operation" and "Per-project
// version pinning": the `pdk` command over an in-memory catalog.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "fakes.hpp"
#include "hefesto/app/project_file.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/pdk/commands.hpp"
#include "hefesto/pdk/diag_codes.hpp"
#include "hefesto/platform/services.hpp"
#include "temp_dir.hpp"

namespace cmd = hefesto::cmd;
namespace pdk = hefesto::pdk;
namespace fs = std::filesystem;
using hefesto::test::TarWriter;

namespace {

constexpr const char* kBase = "https://catalog.example/pdks";
const std::string kHash = "0fe599b2afb6708d281543108caf8310912f54af";
const std::string kOld = "1111111111111111111111111111111111111111";

class Forwarding final : public pdk::Fetcher {
 public:
  explicit Forwarding(pdk::Fetcher& to) : to_(to) {}
  hefesto::diag::Outcome<std::string> get_text(std::string_view url) override { return to_.get_text(url); }
  hefesto::diag::Status download(std::string_view url, const hefesto::platform::Path& to,
                                 const std::function<void(std::uint64_t, std::uint64_t)>& progress) override {
    return to_.download(url, to, progress);
  }

 private:
  pdk::Fetcher& to_;
};

class Session {
 public:
  Session()
      : services_(hefesto::platform::PortableOptions{.storage_root = dir.path() / "storage", .pdk_roots = {},
                                                     .in_memory = false, .inline_tasks = true}),
        dispatcher_(registry_, services_.view(), project_, script_session_) {
    pdk::CommandHosts hosts;
    hosts.fetcher = [this](hefesto::platform::PlatformServices&) { return std::make_unique<Forwarding>(fetcher); };
    hosts.decompressor = [](hefesto::platform::PlatformServices&) {
      return std::make_unique<hefesto::test::CopyDecompressor>();
    };
    hosts.install_host = [] { return std::make_unique<hefesto::test::FakeHost>(); };
    REQUIRE(pdk::register_pdk_commands(registry_, std::move(hosts)));
    dispatcher_.set_editor(&editor);
    root = dir.path() / "root";
    editor.settings.set(hefesto::app::SettingLayer::user, "pdk-root", root.string());
    publish();
  }

  void publish() {
    fetcher.documents[std::string{kBase} + "/sky130/manifest.json"] =
        R"({"pdk":"sky130","versions":[{"version":")" + kHash + R"(","date":"2024-08-17T01:34:30Z"},
           {"version":")" + kOld + R"(","date":"2023-01-01T00:00:00Z"},
           {"version":"2222222222222222222222222222222222222222","date":"2025-01-01T00:00:00Z","prerelease":true}]})";
    std::string assets;
    const auto add = [&](const std::string& content, const std::string& tar) {
      const std::string url = std::string{kBase} + "/sky130/" + kHash + "/" + content + ".tar.zst";
      fetcher.documents[url] = tar;
      assets += std::string{assets.empty() ? "" : ","} + R"({"content":")" + content + R"(","filename":")" + content +
                R"(.tar.zst","url":")" + url + R"(","sha256":")" + pdk::sha256_hex(tar) +
                R"(","size":)" + std::to_string(tar.size()) + "}";
    };
    add("common", TarWriter{}.file("sky130A/libs.tech/magic/sky130A.tech", "tech\n").finish());
    add("sky130_fd_sc_hd", TarWriter{}.file("sky130A/libs.ref/sky130_fd_sc_hd/mag/x.mag", "magic\n").finish());
    fetcher.documents[std::string{kBase} + "/sky130/" + kHash + "/manifest.json"] = "{\"assets\":[" + assets + "]}";
  }

  cmd::CommandResult run(std::vector<std::string> args) {
    return dispatcher_.execute(cmd::Invocation{"pdk", std::move(args), cmd::Origin::cli, std::nullopt});
  }

  hefesto::test::TempDir dir;
  hefesto::test::MapFetcher fetcher;
  hefesto::editor::EditorState editor;
  fs::path root;

 private:
  hefesto::platform::PortableServices services_;
  hefesto::settings::ProjectSettings project_;
  cmd::ScriptSession script_session_{project_};
  cmd::CommandRegistry registry_;
  cmd::Dispatcher dispatcher_;
};

}  // namespace

TEST_CASE("CI install: headless, pinned hash, acceptance flags", "[cap:hefesto-pdk-manager][req:Headless and scripted operation]") {
  Session s;
  const auto r = s.run({"install", "sky130A", kHash, "-lib", "sky130_fd_sc_hd", "-source", kBase, "-accept-license",
                        "-accept-unverified", "-enable"});
  REQUIRE(r.ok());
  CHECK(r.value.to_string() == kHash);
  CHECK(fs::exists(s.root / "ciel/sky130/versions" / kHash / "sky130A/libs.ref/sky130_fd_sc_hd/mag/x.mag"));
  CHECK(fs::exists(s.root / "sky130A/libs.tech/magic/sky130A.tech"));  // enabled
  CHECK(s.run({"root"}).value.to_string() == s.root.string());
  const auto list = s.run({"list"});
  REQUIRE(list.ok());
  CHECK(list.value.to_string().find("sky130A " + kHash + " 2024-08-17T01:34:30Z enabled sky130_fd_sc_hd") != std::string::npos);
  const auto info = s.run({"info", "sky130", kHash});
  REQUIRE(info.ok());
  CHECK(info.value.to_string().find("{repository open_pdks}") != std::string::npos);
  CHECK(info.value.to_string().find("common.tar.zst") != std::string::npos);
}

TEST_CASE("a headless install without acceptance fails and never prompts",
          "[cap:hefesto-pdk-manager][req:Headless and scripted operation]") {
  Session s;
  const auto r = s.run({"install", "sky130A", kHash, "-lib", "sky130_fd_sc_hd", "-source", kBase});
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.back().code == pdk::codes::kLicense);
  const auto unverified =
      s.run({"install", "sky130A", kHash, "-lib", "sky130_fd_sc_hd", "-source", kBase, "-accept-license"});
  REQUIRE_FALSE(unverified.ok());
  CHECK(unverified.diagnostics.back().code == pdk::codes::kUnverified);
  CHECK(s.fetcher.downloads.empty());
}

TEST_CASE("latest picks the newest stable version; remote hides prereleases",
          "[cap:hefesto-pdk-manager][req:Version identity and ordering]") {
  Session s;
  const auto remote = s.run({"remote", "sky130", "-source", kBase});
  REQUIRE(remote.ok());
  CHECK(remote.value.to_string() ==
        "{" + kHash + " 2024-08-17T01:34:30Z stable} {" + kOld + " 2023-01-01T00:00:00Z stable}");
  CHECK(s.run({"remote", "sky130", "-source", kBase, "-prereleases"}).value.to_string().starts_with("{2222"));
  const auto r = s.run({"install", "sky130A", "latest", "-lib", "sky130_fd_sc_hd", "-source", kBase, "-accept-license",
                        "-accept-unverified"});
  REQUIRE(r.ok());
  CHECK(r.value.to_string() == kHash);
}

TEST_CASE("pin writes the project file; re-pinning warns about stale results",
          "[cap:hefesto-pdk-manager][req:Per-project version pinning]") {
  Session s;
  REQUIRE(s.run({"install", "sky130A", kHash, "-lib", "sky130_fd_sc_hd", "-source", kBase, "-accept-license",
                 "-accept-unverified"})
              .ok());
  const auto project = s.dir.path() / "proj";
  fs::create_directories(project);
  const auto pin = s.run({"pin", "sky130A", "-project", project.string(), "-lib", "sky130_fd_sc_hd"});
  REQUIRE(pin.ok());  // the newest installed version
  hefesto::platform::LocalFileSystem files;
  auto loaded = hefesto::app::load_project(files, project);
  REQUIRE(loaded.ok());
  CHECK(loaded.value().pdk == hefesto::app::PdkPin{"sky130", "sky130A", kHash, {"sky130_fd_sc_hd"}});
  const auto repin = s.run({"pin", "sky130A", kOld, "-project", project.string()});
  REQUIRE(repin.ok());
  bool warned = false;
  for (const auto& d : repin.diagnostics) {
    warned = warned || d.code == pdk::codes::kRepin;
  }
  CHECK(warned);
  CHECK(hefesto::app::load_project(files, project).value().pdk->version == kOld);
}

TEST_CASE("an update does not move project pins", "[cap:hefesto-pdk-manager][req:Updates]") {
  Session s;
  const auto project = s.dir.path() / "proj";
  fs::create_directories(project);
  REQUIRE(s.run({"pin", "sky130A", kOld, "-project", project.string()}).ok());
  REQUIRE(s.run({"install", "sky130A", "latest", "-lib", "sky130_fd_sc_hd", "-source", kBase, "-accept-license",
                 "-accept-unverified", "-enable"})
              .ok());
  hefesto::platform::LocalFileSystem files;
  CHECK(hefesto::app::load_project(files, project).value().pdk->version == kOld);
}

TEST_CASE("pdk usage errors and uninstall", "[cap:hefesto-pdk-manager][req:Uninstall and cleanup]") {
  Session s;
  CHECK(s.run({}).diagnostics.back().code == hefesto::diag::codes::kCmdUsage);
  CHECK(s.run({"frobnicate"}).diagnostics.back().code == hefesto::diag::codes::kCmdUsage);
  CHECK(s.run({"install", "sky130A", "-bogus"}).diagnostics.back().code == hefesto::diag::codes::kCmdUsage);
  CHECK(s.run({"install", "sky90"}).diagnostics.back().code == pdk::codes::kSelect);
  CHECK(s.run({"uninstall", "sky130", kHash}).diagnostics.back().code == pdk::codes::kNotInstalled);
  REQUIRE(s.run({"install", "sky130A", kHash, "-lib", "sky130_fd_sc_hd", "-source", kBase, "-accept-license",
                 "-accept-unverified"})
              .ok());
  REQUIRE(s.run({"uninstall", "sky130", kHash}).ok());
  CHECK_FALSE(fs::exists(s.root / "ciel/sky130/versions" / kHash));
}
