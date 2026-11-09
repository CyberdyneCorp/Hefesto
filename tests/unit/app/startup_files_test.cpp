// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// app-startup: installation root, search paths, startup file order.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/app/startup.hpp"
#include "hefesto/platform/file_system.hpp"

namespace app = hefesto::app;
using hefesto::platform::MemoryFileSystem;
using hefesto::platform::Path;

namespace {

struct Fixture {
  MemoryFileSystem fs;
  app::StartupEnvironment env;

  Fixture() {
    env.install_root = "/cad";
    env.home = "/home/u";
    env.working_directory = "/work";
  }

  void add(const Path& path) { (void)fs.write_atomic(path, "# startup\n"); }

  std::vector<std::string> order(const app::StartupOptions& options = {}) {
    std::vector<std::string> out;
    for (const auto& file : app::startup_files(fs, options, env)) {
      out.push_back(file.role + ":" + file.path.string() + (file.deprecated_name ? " (deprecated)" : ""));
    }
    return out;
  }
};

}  // namespace

TEST_CASE("startup files: pre, system, PDK, home, local", "[cap:app-startup][req:Startup file order]") {
  Fixture f;
  f.add("/cad/magic/sys/.initrc");
  f.add("/cad/magic/sys/.magicrc");
  f.add("/pdk/sky130A.magicrc");
  f.add("/home/u/.magicrc");
  f.add("/work/.magicrc");
  f.env.pdk_startup_file = Path{"/pdk/sky130A.magicrc"};
  CHECK(f.order() == std::vector<std::string>{"pre:/cad/magic/sys/.initrc", "system:/cad/magic/sys/.magicrc",
                                              "pdk:/pdk/sky130A.magicrc", "user:/home/u/.magicrc",
                                              "local:/work/.magicrc"});
}

TEST_CASE("deprecated .magic names and magic_setup", "[cap:app-startup][req:Startup file order]") {
  Fixture f;
  f.add("/home/u/.magic");
  f.add("/work/.magic");
  f.add("/work/magic_setup");
  CHECK(f.order() == std::vector<std::string>{"user:/home/u/.magic (deprecated)", "local:/work/.magic (deprecated)"});
  Fixture g;
  g.add("/work/magic_setup");
  CHECK(g.order() == std::vector<std::string>{"local:/work/magic_setup"});
}

TEST_CASE("the local file is skipped in the home directory", "[cap:app-startup][req:Startup file order]") {
  Fixture f;
  f.add("/home/u/.magicrc");
  f.env.working_directory = "/home/u";
  CHECK(f.order() == std::vector<std::string>{"user:/home/u/.magicrc"});
  app::StartupOptions absolute;
  absolute.rc_file = "/home/u/.magicrc";  // an absolute rcname is read even from HOME, and not as a home file
  CHECK(f.order(absolute) == std::vector<std::string>{"local:/home/u/.magicrc"});
}

TEST_CASE("the rcfile option reads ~/NAME then ./NAME; -norcfile reads neither", "[cap:app-startup][req:Startup file order]") {
  Fixture f;
  f.add("/home/u/my.rc");
  f.add("/work/my.rc");
  f.add("/home/u/.magicrc");
  f.add("/cad/magic/sys/.magicrc");
  app::StartupOptions rc;
  rc.rc_file = "my.rc";
  CHECK(f.order(rc) ==
        std::vector<std::string>{"system:/cad/magic/sys/.magicrc", "user:/home/u/my.rc", "local:/work/my.rc"});
  app::StartupOptions none;
  none.no_rc = true;
  CHECK(f.order(none) == std::vector<std::string>{"system:/cad/magic/sys/.magicrc"});
  Fixture missing;  // a missing non-default rcname: no ./.magic or magic_setup fallback
  missing.add("/work/.magic");
  CHECK(missing.order(rc).empty());
}

TEST_CASE("installation root: CAD_ROOT, else the built-in prefix",
          "[cap:app-startup][req:Installation root and search paths]") {
  CHECK(app::resolve_install_root(std::string{"/opt/eda/lib"}, "/usr/local/lib") == Path{"/opt/eda/lib"});
  CHECK(app::resolve_install_root(std::string{}, "/usr/local/lib") == Path{"/usr/local/lib"});
  CHECK(app::resolve_install_root(std::nullopt, "/usr/local/lib") == Path{"/usr/local/lib"});
  Fixture f;  // relocated install: the system startup file is looked up under the root
  f.env.install_root = "/opt/eda/lib";
  f.add("/opt/eda/lib/magic/sys/.magicrc");
  CHECK(f.order() == std::vector<std::string>{"system:/opt/eda/lib/magic/sys/.magicrc"});
}

TEST_CASE("system and cell library search paths", "[cap:app-startup][req:Installation root and search paths]") {
  CHECK(app::system_library_path() == std::vector<std::string>{".", "$CAD_ROOT/magic/sys", "$CAD_ROOT/magic/sys/current"});
  CHECK(app::cell_library_path(std::string{"scmos"}) ==
        std::vector<std::string>{"$CAD_ROOT/magic/scmos", "$CAD_ROOT/magic/sys/current", "$CAD_ROOT/magic/tutorial"});
  CHECK(app::cell_library_path(std::nullopt) ==
        std::vector<std::string>{"$CAD_ROOT/magic/sys/current", "$CAD_ROOT/magic/tutorial"});
}

TEST_CASE("the technology of a .mag file header", "[cap:app-startup][req:Default technology selection]") {
  CHECK(app::mag_technology("magic\ntech sky130A\nmagscale 1 2\n") == "sky130A");
  CHECK(app::mag_technology("magic\ntech   scmos\n") == "scmos");
  CHECK_FALSE(app::mag_technology("magic\ntimestamp 0\n").has_value());
  CHECK_FALSE(app::mag_technology("not magic\ntech sky130A\n").has_value());
  CHECK_FALSE(app::mag_technology("").has_value());
}
