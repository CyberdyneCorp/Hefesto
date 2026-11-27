// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `xschemrc` files read as data in the sandbox (capability
// schematic-symbols-and-libraries "xschemrc as data", "Script execution
// permission", "Library search path"; Q-SCHEM-23, Q-SCHEM-25). The
// Hefesto-written rc files below follow the shape of the PDK ones; the
// scenarios over the three pinned PDK `xschemrc` files skip when the xschem
// data sets are not fetched (scripts/fetch_pdk.py).
//
// Owner: SYMLIB.

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iterator>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/lib/reference.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "reference_data.hpp"

namespace schem = hefesto::schem;
namespace diag = hefesto::diag;
namespace platform = hefesto::platform;
namespace fs = std::filesystem;

namespace {

constexpr const char* kRc = "[cap:schematic-symbols-and-libraries][req:xschemrc as data]";
constexpr const char* kPermission = "[cap:schematic-symbols-and-libraries][req:Script execution permission]";

class TempDir {
 public:
  TempDir() {
    static std::atomic<int> counter{0};
    path_ = fs::canonical(fs::temp_directory_path()) /
            ("hefesto-xschemrc-" + std::to_string(::getpid()) + "-" + std::to_string(counter.fetch_add(1)));
    fs::remove_all(path_);
    fs::create_directories(path_);
  }
  ~TempDir() {
    std::error_code ignored;
    fs::remove_all(path_, ignored);
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  [[nodiscard]] const fs::path& path() const noexcept { return path_; }

  void write(const fs::path& relative, std::string_view text) const {
    fs::create_directories((path_ / relative).parent_path());
    std::ofstream out(path_ / relative, std::ios::binary);
    out << text;
  }
  void mkdir(const fs::path& relative) const { fs::create_directories(path_ / relative); }

 private:
  fs::path path_;
};

std::vector<diag::Diagnostic> with_code(const std::vector<diag::Diagnostic>& all, std::string_view code) {
  std::vector<diag::Diagnostic> out;
  std::copy_if(all.begin(), all.end(), std::back_inserter(out), [code](const auto& d) { return d.code == code; });
  return out;
}

// An IHP-shaped tree: <root>/pdks/ihp-sg13g2 with its library folders, the
// system library at <root>/share and a document folder <root>/work.
constexpr std::string_view kIhpShapedRc = R"tcl(
set XSCHEM_LIBRARY_PATH {}
append XSCHEM_LIBRARY_PATH :${XSCHEM_SHAREDIR}/xschem_library/devices
append XSCHEM_LIBRARY_PATH :${XSCHEM_SHAREDIR}/xschem_library
append XSCHEM_LIBRARY_PATH :$USER_CONF_DIR/xschem_library
array unset dircolor
set dircolor(sg13g2_pr$) blue
set netlist_dir $env(PWD)/simulations
set to_pdf {ps2pdf -dAutoRotatePages=/None}
if {[info exists env(PDK_ROOT)] && $env(PDK_ROOT) ne {}} {
  if {![file isdir $env(PDK_ROOT)]} { puts stderr "Warning: PDK_ROOT not found" }
  set PDK_ROOT $env(PDK_ROOT)
}
if {[info exists PDK_ROOT]} {
  if {[info exists env(PDK)]} { set PDK $env(PDK) } else { set PDK ihp-sg13g2 }
  set SG13G2_MODELS ${PDK_ROOT}/${PDK}/libs.tech/ngspice/models
  puts stderr "SG13G2_MODELS: $SG13G2_MODELS"
}
set XSCHEM_START_WINDOW ${PDK_ROOT}/${PDK}/libs.tech/xschem/start_page.sch
append XSCHEM_LIBRARY_PATH :${PDK_ROOT}/${PDK}/libs.tech/xschem
append XSCHEM_LIBRARY_PATH :${PDK_ROOT}/${PDK}/libs.tech/xschem/sg13g2_pr
if {[file exists ${PDK_ROOT}/${PDK}/libs.tech/xschem/sg13g2_pr/helper.tcl]} {
  source ${PDK_ROOT}/${PDK}/libs.tech/xschem/sg13g2_pr/helper.tcl
}
append XSCHEM_LIBRARY_PATH :${PDK_ROOT}/${PDK}/libs.ref/sg13g2_stdcell/sym/xschem
wm title . {IHP}
set xschem_execute_scripts yes
)tcl";

struct IhpShaped {
  TempDir dir;
  schem::XschemrcInput input;

  explicit IhpShaped(std::string_view rc = kIhpShapedRc) {
    dir.write("pdks/ihp-sg13g2/libs.tech/xschem/xschemrc", rc);
    dir.write("pdks/ihp-sg13g2/libs.tech/xschem/sg13g2_pr/helper.tcl", "proc helper_proc {} {return 1}\n");
    dir.mkdir("pdks/ihp-sg13g2/libs.ref/sg13g2_stdcell/sym/xschem");
    dir.mkdir("share/xschem_library/devices");
    dir.mkdir("work");
    input = schem::pdk_xschemrc_input(dir.path() / "pdks", "ihp-sg13g2", dir.path() / "share",
                                      dir.path() / "conf", dir.path() / "work");
  }

  [[nodiscard]] std::string under(std::string_view relative) const { return (dir.path() / relative).generic_string(); }
};

std::vector<platform::Path> directories_after_system(const schem::SearchPath& path, std::size_t system) {
  const auto& all = path.directories();
  return {all.begin() + static_cast<std::ptrdiff_t>(std::min(system, all.size())), all.end()};
}

}  // namespace

TEST_CASE("an IHP-shaped xschemrc gives the library path after the system folders", kRc) {
  IhpShaped ihp;
  platform::LocalFileSystem files;
  const auto result = schem::SearchPath::from_xschemrc(files, ihp.input);
  CHECK(with_code(result.diagnostics, "SCHEM-LIB-006").empty());
  CHECK(result.search_path.directories() ==
        std::vector<platform::Path>{ihp.under("share/xschem_library/devices"), ihp.under("share/xschem_library"),
                                    ihp.under("pdks/ihp-sg13g2/libs.tech/xschem"),
                                    ihp.under("pdks/ihp-sg13g2/libs.tech/xschem/sg13g2_pr"),
                                    ihp.under("pdks/ihp-sg13g2/libs.ref/sg13g2_stdcell/sym/xschem")});
  // Variables tcleval values read stay defined; env(PWD) is the document folder.
  CHECK(result.variables.at("SG13G2_MODELS") == ihp.under("pdks/ihp-sg13g2/libs.tech/ngspice/models"));
  CHECK(result.variables.at("netlist_dir") == ihp.under("work") + "/simulations");
  CHECK(result.variables.at("PDK") == "ihp-sg13g2");
  CHECK(result.variables.count("tcl_version") == 0);  // interpreter built-ins are not PDK variables
  // puts output is a note, not process output.
  const auto notes = with_code(result.diagnostics, "SCHEM-LIB-008");
  REQUIRE(notes.size() == 1);
  CHECK(notes[0].text == "xschemrc: SG13G2_MODELS: " + ihp.under("pdks/ihp-sg13g2/libs.tech/ngspice/models"));
}

TEST_CASE("xschem_execute_scripts ignored", kPermission) {
  IhpShaped ihp;
  platform::LocalFileSystem files;
  const auto result = schem::SearchPath::from_xschemrc(files, ihp.input);
  const auto logged = with_code(result.diagnostics, "SCHEM-LIB-007");
  REQUIRE(logged.size() == 1);
  CHECK(logged[0].severity == diag::Severity::info);
  CHECK(logged[0].text.find("xschem_execute_scripts yes") != std::string::npos);
  CHECK(logged[0].text.find("ignored") != std::string::npos);
}

TEST_CASE("the document folder's xschemrc is evaluated after the PDK's", kRc) {
  IhpShaped ihp;
  ihp.dir.write("work/xschemrc", "append XSCHEM_LIBRARY_PATH :.\nset SG13G2_MODELS /mine\n");
  platform::LocalFileSystem files;
  const auto result = schem::SearchPath::from_xschemrc(files, ihp.input);
  CHECK(result.variables.at("SG13G2_MODELS") == "/mine");
  REQUIRE_FALSE(result.search_path.directories().empty());
  CHECK(result.search_path.directories().back() == ".");
}

TEST_CASE("an xschemrc error keeps the settings made before it", kRc) {
  IhpShaped ihp("set XSCHEM_LIBRARY_PATH $XSCHEM_SHAREDIR/xschem_library\nerror boom\nset LATER 1\n");
  platform::LocalFileSystem files;
  const auto result = schem::SearchPath::from_xschemrc(files, ihp.input);
  const auto errors = with_code(result.diagnostics, "SCHEM-LIB-006");
  REQUIRE(errors.size() == 1);
  CHECK(errors[0].text.find("boom") != std::string::npos);
  CHECK(result.variables.count("LATER") == 0);
  CHECK(result.search_path.directories() == std::vector<platform::Path>{ihp.under("share/xschem_library")});
}

TEST_CASE("an xschemrc runs in the sandbox", kRc) {
  IhpShaped ihp(
      "set XSCHEM_LIBRARY_PATH {}\n"
      "if {[catch {exec ls} err]} { set exec_denied 1 }\n"
      "if {[catch {open /etc/hosts r} err]} { set read_denied 1 }\n"
      "if {[catch {open $env(PWD)/out.txt w} err]} { set write_denied 1 }\n");
  platform::LocalFileSystem files;
  const auto result = schem::SearchPath::from_xschemrc(files, ihp.input);
  CHECK(result.variables.count("exec_denied") == 1);
  CHECK(result.variables.count("read_denied") == 1);
  CHECK(result.variables.count("write_denied") == 1);
  CHECK_FALSE(fs::exists(ihp.dir.path() / "work" / "out.txt"));
  CHECK(result.search_path.directories() == std::vector<platform::Path>{"."});
}

TEST_CASE("a missing xschemrc is reported and gives the document folder", kRc) {
  TempDir dir;
  schem::XschemrcInput input;
  input.rc_file = dir.path() / "none" / "xschemrc";
  input.document_folder = dir.path();
  platform::LocalFileSystem files;
  const auto result = schem::SearchPath::from_xschemrc(files, input);
  CHECK(with_code(result.diagnostics, "SCHEM-LIB-006").size() == 1);
  CHECK(result.search_path.directories() == std::vector<platform::Path>{"."});
}

TEST_CASE("IHP library path", kRc) {
  const auto tree = hefesto::test::pinned_xschem(hefesto::test::kIhpSg13g2);
  const auto stdcells =
      hefesto::test::pinned_xschem(hefesto::test::kIhpSg13g2, hefesto::test::kIhpStdcellXschemSymbols);
  if (!tree || !stdcells) SKIP("the ihp-xschem data set is not fetched");
  const fs::path variant = tree->parent_path().parent_path();
  TempDir work;
  TempDir share;
  share.mkdir("xschem_library/devices");
  const auto input = schem::pdk_xschemrc_input(variant.parent_path(), variant.filename().string(), share.path(),
                                               share.path() / "conf", work.path());
  platform::LocalFileSystem files;
  const auto result = schem::SearchPath::from_xschemrc(files, input);
  const auto after = directories_after_system(result.search_path, 2);
  REQUIRE(after.size() >= 3);
  CHECK(after[0] == tree->generic_string());
  CHECK(after[1] == (*tree / "sg13g2_pr").generic_string());
  CHECK(after[2] == stdcells->generic_string());
}

TEST_CASE("xschem_execute_scripts ignored for the three PDK xschemrc files", kPermission) {
  for (const auto* pdk : {&hefesto::test::kSky130A, &hefesto::test::kGf180mcuD, &hefesto::test::kIhpSg13g2}) {
    CAPTURE(pdk->variant);
    const auto tree = hefesto::test::pinned_xschem(*pdk);
    if (!tree) {
      WARN("xschem data set not fetched: " << pdk->variant);
      continue;
    }
    const fs::path variant = tree->parent_path().parent_path();
    TempDir work;
    const auto input = schem::pdk_xschemrc_input(variant.parent_path(), variant.filename().string(), work.path(),
                                                 work.path() / "conf", work.path());
    platform::LocalFileSystem files;
    const auto result = schem::SearchPath::from_xschemrc(files, input);
    const auto set = result.variables.find("xschem_execute_scripts");
    // Whatever the file sets (sky130: yes) has no effect, and is logged.
    if (set != result.variables.end()) CHECK(with_code(result.diagnostics, "SCHEM-LIB-007").size() == 1);
    CHECK_FALSE(result.search_path.directories().empty());
    // Q-SCHEM-38 (a): the PDK file calls no command outside the GUI-only list.
    for (const auto& d : with_code(result.diagnostics, "SCHEM-LIB-006")) FAIL_CHECK(d.text);
  }
}

// Input limit approved 2026-11-21 (Q-SCHEM-32): an endless loop in an rc
// file hung the opening. The limit is shortened here; the default is
// checked on its own.
TEST_CASE("Endless loop in an xschemrc", kRc) {
  CHECK(schem::kXschemrcTimeLimit == std::chrono::seconds{10});
  CHECK(schem::XschemrcInput{}.time_limit == std::chrono::seconds{10});
  TempDir dir;
  dir.mkdir("a");
  dir.mkdir("b");
  dir.write("work/xschemrc", "set XSCHEM_LIBRARY_PATH " + (dir.path() / "a").generic_string() +
                                 "\nwhile 1 {}\nset XSCHEM_LIBRARY_PATH " + (dir.path() / "b").generic_string() +
                                 "\n");
  schem::XschemrcInput input;
  input.document_folder = dir.path() / "work";
  input.time_limit = std::chrono::milliseconds{300};
  platform::LocalFileSystem files;
  const auto start = std::chrono::steady_clock::now();
  const auto result = schem::SearchPath::from_xschemrc(files, input);
  CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds{10});
  const auto stops = with_code(result.diagnostics, "SCHEM-LIB-010");
  REQUIRE(stops.size() == 1);
  CHECK(stops[0].text == "xschemrc " + (dir.path() / "work" / "xschemrc").generic_string() +
                             ": evaluation stopped after 0.3 s");
  CHECK(result.search_path.directories() == std::vector<platform::Path>{(dir.path() / "a").generic_string()});
}

TEST_CASE("an endless loop that catches errors is still stopped", kRc) {
  TempDir dir;
  dir.write("work/xschemrc", "while 1 { catch { while 1 {} } }\nset LATER 1\n");
  schem::XschemrcInput input;
  input.document_folder = dir.path() / "work";
  input.time_limit = std::chrono::milliseconds{200};
  platform::LocalFileSystem files;
  const auto result = schem::SearchPath::from_xschemrc(files, input);
  CHECK(with_code(result.diagnostics, "SCHEM-LIB-010").size() == 1);
  CHECK(result.variables.count("LATER") == 0);
}

TEST_CASE("Synthetic HOME and PWD", kRc) {
  TempDir dir;
  dir.mkdir("s/lib");
  dir.write("s/xschemrc", "set XSCHEM_LIBRARY_PATH $env(PWD):$env(HOME)/lib\n");
  schem::XschemrcInput input;
  input.document_folder = dir.path() / "s";
  platform::LocalFileSystem files;
  const auto result = schem::SearchPath::from_xschemrc(files, input);
  CHECK(result.search_path.directories() ==
        std::vector<platform::Path>{(dir.path() / "s").generic_string(), (dir.path() / "s" / "lib").generic_string()});
}

// Q-SCHEM-38 option (a), approved 2026-11-22, both modes: round 1 accepted
// every unknown command through `unknown`, so a typo in an rc file was
// silently ignored and the settings after it were still made.
TEST_CASE("Unknown command in an xschemrc", kRc) {
  for (const bool compat : {true, false}) {  // the same in both modes of xschem-compat
    CAPTURE(compat);
    TempDir dir;
    dir.mkdir("a");
    dir.mkdir("b");
    dir.write("work/xschemrc", "set XSCHEM_LIBRARY_PATH " + (dir.path() / "a").generic_string() +
                                   "\nfrobnicate_colors 1 2\nset XSCHEM_LIBRARY_PATH " +
                                   (dir.path() / "b").generic_string() + "\nset LATER 1\n");
    schem::XschemrcInput input;
    input.document_folder = dir.path() / "work";
    platform::LocalFileSystem files;
    const auto result = schem::SearchPath::from_xschemrc(files, input);
    const auto errors = with_code(result.diagnostics, "SCHEM-LIB-006");
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].text.find("invalid command name \"frobnicate_colors\"") != std::string::npos);
    CHECK(result.variables.count("LATER") == 0);
    CHECK(result.search_path.directories() == std::vector<platform::Path>{(dir.path() / "a").generic_string()});
  }
}

// Q-SCHEM-52 (approved 2026-11-27; no mode switch: both modes of
// xschem-compat give the same path): before any xschemrc runs, the search
// path is the system library.
namespace {

constexpr const char* kSearchPath = "[cap:schematic-symbols-and-libraries][req:Library search path]";

struct PlainFolder {
  TempDir dir;
  schem::XschemrcInput input;

  PlainFolder() {
    dir.write("share/xschem_library/devices/res.sym", "v {xschem version=3.4.8RC file_version=1.3}\nK {type=resistor}\n");
    dir.mkdir("w/s");
    dir.mkdir("w/lib");
    input.document_folder = dir.path() / "w" / "s";
    input.variables = {{"XSCHEM_SHAREDIR", (dir.path() / "share").generic_string()},
                       {"USER_CONF_DIR", (dir.path() / "w" / "s" / ".xschem").generic_string()}};
  }

  [[nodiscard]] platform::Path under(std::string_view relative) const { return dir.path() / relative; }
};

}  // namespace

TEST_CASE("Default search path without an xschemrc", kSearchPath) {
  PlainFolder plain;
  platform::LocalFileSystem files;
  const auto result = schem::SearchPath::from_xschemrc(files, plain.input);
  CHECK(result.diagnostics.empty());
  CHECK(result.search_path.directories() == std::vector<platform::Path>{plain.under("share/xschem_library/devices")});
  CHECK(schem::default_xschem_library_path(plain.under("share")) ==
        plain.under("share/xschem_library/devices").generic_string());
  const auto found = schem::resolve_reference("res.sym", plain.input.document_folder, result.search_path, files);
  CHECK(found.file == plain.under("share/xschem_library/devices/res.sym"));
}

TEST_CASE("xschemrc replaces the default search path", kSearchPath) {
  PlainFolder plain;
  platform::LocalFileSystem files;
  const std::string lib = plain.under("w/lib").generic_string();
  plain.dir.write("w/s/xschemrc", "set XSCHEM_LIBRARY_PATH " + lib + "\n");
  CHECK(schem::SearchPath::from_xschemrc(files, plain.input).search_path.directories() ==
        std::vector<platform::Path>{lib});
  plain.dir.write("w/s/xschemrc", "append XSCHEM_LIBRARY_PATH :" + lib + "\n");
  CHECK(schem::SearchPath::from_xschemrc(files, plain.input).search_path.directories() ==
        std::vector<platform::Path>{plain.under("share/xschem_library/devices"), lib});
}
