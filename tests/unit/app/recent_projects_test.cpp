// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The recent-projects list behind hefesto-pdk-manager "Uninstall and
// cleanup" (the projects known to pin a version).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/app/diag_codes.hpp"
#include "hefesto/app/project_file.hpp"
#include "hefesto/app/recent_projects.hpp"
#include "hefesto/platform/file_system.hpp"
#include "temp_dir.hpp"

namespace app = hefesto::app;
namespace fs = std::filesystem;
using hefesto::test::TempDir;
using hefesto::test::write_file;

TEST_CASE("recent projects: most recent first, no duplicates, bounded",
          "[cap:hefesto-pdk-manager][req:Uninstall and cleanup]") {
  TempDir dir;
  hefesto::platform::LocalFileSystem files;
  const auto list = dir.path() / "recent.json";
  REQUIRE(app::load_recent_projects(files, list).value().empty());  // missing file: empty
  REQUIRE(app::note_recent_project(files, list, "/p/a").ok());
  REQUIRE(app::note_recent_project(files, list, "/p/b").ok());
  REQUIRE(app::note_recent_project(files, list, "/p/./a").ok());
  CHECK(app::load_recent_projects(files, list).value() == std::vector<fs::path>{"/p/a", "/p/b"});
  for (int i = 0; i < 30; ++i) {
    REQUIRE(app::note_recent_project(files, list, "/q/" + std::to_string(i)).ok());
  }
  const auto bounded = app::load_recent_projects(files, list).value();
  CHECK(bounded.size() == app::kMaxRecentProjects);
  CHECK(bounded.front() == fs::path{"/q/29"});
}

TEST_CASE("a malformed recent-projects list is reported and replaced on the next note",
          "[cap:hefesto-pdk-manager][req:Uninstall and cleanup]") {
  TempDir dir;
  hefesto::platform::LocalFileSystem files;
  const auto list = dir.path() / "recent.json";
  for (const char* bad : {"[]", "{", R"({"projects":"x"})", R"({"projects":[1]})", R"({"projects":[""]})"}) {
    write_file(list, bad);
    const auto loaded = app::load_recent_projects(files, list);
    REQUIRE_FALSE(loaded.ok());
    CHECK(loaded.error().code == app::codes::kRecentProjects);
  }
  REQUIRE(app::note_recent_project(files, list, "/p/a").ok());
  CHECK(app::load_recent_projects(files, list).value() == std::vector<fs::path>{"/p/a"});
}

TEST_CASE("projects pinning a version are found through their project files",
          "[cap:hefesto-pdk-manager][req:Uninstall and cleanup]") {
  TempDir dir;
  hefesto::platform::LocalFileSystem files;
  const std::string hash(40, 'a');
  const auto pinned = dir.path() / "pinned";
  const auto other = dir.path() / "other";
  fs::create_directories(pinned);
  fs::create_directories(other);
  app::ProjectFile project;
  project.pdk = app::PdkPin{"sky130", "sky130A", hash, {}};
  REQUIRE(app::save_project(files, pinned, project).ok());
  project.pdk->version = std::string(40, 'b');
  REQUIRE(app::save_project(files, other, project).ok());
  const std::vector<fs::path> folders{pinned, other, dir.path() / "gone"};
  CHECK(app::projects_pinning(files, folders, "sky130", hash) == std::vector<fs::path>{pinned});
  CHECK(app::projects_pinning(files, folders, "gf180mcu", hash).empty());
}
