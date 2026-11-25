// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The symbol library browser (schematic_library.hpp; M8 task 13.2) and the
// desktop install root (M8-D10, approved 2026-11-23).

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "hefesto/platform/file_system.hpp"
#include "hefesto/shell/schematic_library.hpp"

using namespace hefesto::shell;
using hefesto::platform::MemoryFileSystem;
using hefesto::platform::Path;

namespace {

void add(MemoryFileSystem& fs, const Path& file) {
  (void)fs.write_atomic(file, "v {xschem version=3.4.8RC file_version=1.3}\n");
}

}  // namespace

TEST_CASE("the library browser lists the search path, then the system library",
          "[cap:schematic-symbols-and-libraries][req:System library]") {
  MemoryFileSystem fs;
  add(fs, "/pdk/xschem/sky130_fd_pr/nfet_01v8.sym");
  add(fs, "/pdk/xschem/sky130_fd_pr/pfet_01v8.sym");
  add(fs, "/pdk/xschem/sky130_fd_pr/notes.txt");
  add(fs, "/cad/assets/schematic/xschem_library/devices/res.sym");
  add(fs, "/cad/assets/schematic/xschem_library/devices/ipin.sym");

  const auto folders = library_folders({"/pdk/xschem/sky130_fd_pr"}, "/cad");
  REQUIRE(folders.size() == 2);
  CHECK(folders[0].label == "sky130_fd_pr");
  CHECK_FALSE(folders[0].system);
  CHECK(folders[1].dir == system_library_dir("/cad"));
  CHECK(folders[1].label == "devices");
  CHECK(folders[1].system);
  // Already on the search path (the PDK xschemrc appends it): listed once.
  const auto once = library_folders({"/cad/assets/schematic/xschem_library/devices/"}, "/cad");
  REQUIRE(once.size() == 1);
  CHECK(once[0].system);
  CHECK(library_folders({}, Path{}).empty());

  SymbolLibraryBrowser browser;
  browser.load(fs, folders);
  CHECK(browser.title() == "Symbols · sky130_fd_pr");
  CHECK(SymbolLibraryBrowser::placeholder() == "Search symbols");
  std::vector<std::string> names;
  for (const LibraryEntry& e : browser.visible()) names.push_back(e.name);
  CHECK(names == std::vector<std::string>{"nfet_01v8.sym", "pfet_01v8.sym", "ipin.sym", "res.sym"});

  browser.set_filter("FET");  // case-insensitive
  REQUIRE(browser.visible().size() == 2);
  browser.set_filter("res");
  REQUIRE(browser.visible().size() == 1);
  CHECK(browser.visible()[0].folder == 1);
  browser.set_filter("zzz");
  CHECK(browser.visible().empty());
}

TEST_CASE("placing from the library issues schematic place", "[cap:schematic-editing][req:Placing instances]") {
  MemoryFileSystem fs;
  add(fs, "/cad/assets/schematic/xschem_library/devices/res.sym");
  SymbolLibraryBrowser browser;
  browser.load(fs, library_folders({}, "/cad"));
  CHECK(browser.place_command(0, {0, -30}) == "schematic place res.sym 0 -30");
  CHECK(browser.place_command(0, {12.5, 0.1}) == "schematic place res.sym 12.5 0.1");
  CHECK_FALSE(browser.place_command(1, {0, 0}).has_value());
  CHECK(coordinate_text(-0.0) == "-0");
  CHECK(coordinate_text(1e21) == "1e+21");
}

TEST_CASE("the search path list result is parsed as a Tcl list",
          "[cap:schematic-symbols-and-libraries][req:Library search path]") {
  CHECK(parse_path_list("/a {/b c} /d") == std::vector<Path>{"/a", "/b c", "/d"});
  CHECK(parse_path_list("").empty());
  CHECK(parse_path_list("{unbalanced").empty());
}

TEST_CASE("the desktop install root: CAD_ROOT, else the executable's",
          "[cap:app-startup][req:Installation root and search paths]") {
  MemoryFileSystem fs;
  add(fs, "/nonexistent-hefesto/build/src/shell/desktop/assets/schematic/xschem_library/devices/res.sym");
  const Path exe_dir = "/nonexistent-hefesto/build/src/shell/desktop";
  CHECK(desktop_install_root(fs, exe_dir, std::nullopt) == exe_dir);
  CHECK(desktop_install_root(fs, exe_dir, std::string{}) == exe_dir);
  CHECK(desktop_install_root(fs, exe_dir, std::string{"/cad"}) == Path{"/cad"});
  CHECK(desktop_install_root(fs, "/nonexistent-hefesto/bin", std::nullopt).empty());
  CHECK(desktop_install_root(fs, Path{}, std::nullopt).empty());
  CHECK(system_library_dir(Path{}).empty());
}

// Regression (review of PR #101): the install root came from argv[0], so
// `hefesto` started from PATH (argv[0] "hefesto") resolved against the
// working directory, whose `assets/` became the system library and a
// read-only sandbox root. shell.cpp now passes the folder the operating
// system reports (platform_posix::executable_dir, tested with the CLI), and
// a relative folder is ignored.
TEST_CASE("the desktop install root never comes from the working directory",
          "[cap:app-startup][req:Installation root and search paths]") {
  const std::filesystem::path cwd =
      std::filesystem::temp_directory_path() / ("hefesto-install-root-" + std::to_string(::getpid()));
  std::filesystem::create_directories(cwd / "assets/schematic/xschem_library/devices");
  hefesto::platform::LocalFileSystem fs;
  const auto saved = std::filesystem::current_path();
  std::filesystem::current_path(cwd);
  const Path from_argv0 = desktop_install_root(fs, Path{"hefesto"}.parent_path(), std::nullopt);
  const Path from_dot = desktop_install_root(fs, Path{"."}, std::nullopt);
  const Path from_bin = desktop_install_root(fs, Path{"bin"}, std::nullopt);
  std::filesystem::current_path(saved);
  std::filesystem::remove_all(cwd);
  CHECK(from_argv0.empty());
  CHECK(from_dot.empty());
  CHECK(from_bin.empty());
}

// Regression (round-4 open item): load_assets found the fonts and icons from
// argv[0], so `hefesto` started from PATH looked in the working directory's
// assets/. It now passes the folder the operating system reports for the
// executable (platform_posix::executable_dir) to desktop_asset_dir, which
// ignores a relative folder; the override and the source fallback stay.
TEST_CASE("the desktop asset folder comes from the executable's folder",
          "[cap:hefesto-design-system][req:Typography]") {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / ("hefesto-assets-" + std::to_string(::getpid()));
  const auto font = [](const std::filesystem::path& assets) {
    std::filesystem::create_directories(assets / "fonts");
    (void)hefesto::platform::LocalFileSystem{}.write_atomic(assets / "fonts/Inter-Regular.ttf", "font");
  };
  font(root / "bin/assets");
  font(root / "cwd/assets");
  font(root / "override");
  const auto saved = std::filesystem::current_path();
  std::filesystem::current_path(root / "cwd");
  const auto from_exe = desktop_asset_dir({}, root / "bin", {});
  const auto from_argv0 = desktop_asset_dir({}, Path{"hefesto"}.parent_path(), {});
  const auto from_relative = desktop_asset_dir({}, Path{"."}, {});
  const auto overridden = desktop_asset_dir(root / "override", root / "bin", {});
  const auto source = desktop_asset_dir({}, Path{}, root / "override");
  std::filesystem::current_path(saved);
  std::filesystem::remove_all(root);
  CHECK(from_exe == root / "bin/assets");
  CHECK_FALSE(from_argv0.has_value());
  CHECK_FALSE(from_relative.has_value());
  CHECK(overridden == root / "override");
  CHECK(source == root / "override");
}
