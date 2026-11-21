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
#include <iterator>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "hefesto/platform/file_system.hpp"
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
toolbar_show
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
  }
}
