// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Security review, task 12.1 (design D14): hostile Tcl symbol generators
// (schem/lib generator, run when a schematic that places `name(args)` is
// opened, without a prompt) and hostile `xschemrc` files (schem/lib
// xschemrc). Every denial SHALL give the sandbox diagnostic, also when the
// script catches it; endless or flooding scripts SHALL stop, in both modes
// of `xschem-compat`.
//
// Owner: SECURITY.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <string>

#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/lib/generator.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"
#include "hefesto/schem/lib/xschemrc.hpp"
#include "security_support.hpp"

namespace schem = hefesto::schem;
namespace diag = hefesto::diag;
namespace platform = hefesto::platform;
namespace sec = hefesto::test::security;
using std::chrono::milliseconds;

// Regression (review finding S8-01, generators): run_tcl_generator kept only
// SCHEM-LIB-012 and dropped the sandbox's diagnostics, so a generator's
// denied `exec` (or one it caught and went on) was never reported.
TEST_CASE("hostile symbol generators are denied with the sandbox diagnostic", sec::kPermissionModel) {
  for (const sec::Hostile& hostile : sec::hostile_operations()) {
    INFO(hostile.name);
    sec::TempFolder library;
    sec::TempFolder outside;
    outside.write("evil.tcl", "puts {v {xschem version=3.4.8RC file_version=1.2}}\n");
    platform::LocalFileSystem fs;
    library.write("gen.tcl", sec::with_outside(hostile.script, outside.str()) + "\n");
    const auto output = schem::run_tcl_generator(fs, {library.path() / "gen.tcl", {}, library.path()});
    CHECK_FALSE(output.text.has_value());
    const auto denials = sec::with_code_prefix(output.diagnostics, "SCRIPT-SANDBOX-");
    REQUIRE(denials.size() == 1);
    CHECK(denials[0].code == hostile.code);
    CHECK(sec::with_code_prefix(output.diagnostics, "SCHEM-LIB-012").size() == 1);
    CHECK_FALSE(std::filesystem::exists(outside.path() / "written"));
    CHECK(std::filesystem::exists(outside.path() / "evil.tcl"));
  }
}

TEST_CASE("a denial a generator catches is reported and the symbol is still made", sec::kSandbox) {
  for (const bool compat : {true, false}) {
    CAPTURE(compat);
    sec::TempFolder library;
    library.write("gen.tcl", "catch {exec id}\ncatch {socket example.com 80}\n"
                             "puts {v {xschem version=3.4.8RC file_version=1.2}}\n");
    platform::LocalFileSystem fs;
    diag::CollectingSink sink;
    schem::SymbolCache cache;
    schem::Document top;
    top.path = library.path() / "top.sch";
    const schem::SearchPath path{{library.path()}};
    const schem::Symbol& symbol = cache.resolve({"gen.tcl(1)"}, top, {path, fs, &sink, compat});
    CHECK_FALSE(symbol.placeholder);
    const auto denials = sec::with_code_prefix(sink.take(), "SCRIPT-SANDBOX-");
    REQUIRE(denials.size() == 2);
    CHECK(denials[0].code == "SCRIPT-SANDBOX-001");
    CHECK(denials[1].code == "SCRIPT-SANDBOX-002");
    CHECK(denials[0].text.find("caught by the script's catch") != std::string::npos);
  }
}

// Generator stdout flooding: what `puts` prints is bounded (here 1 MiB),
// and `puts` to another channel is dropped, not written to the process.
TEST_CASE("a generator that floods its output is stopped", sec::kPermissionModel) {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/lib/flood.tcl", "set x [string repeat x 65536]\nwhile 1 { catch { puts $x } }\n").ok());
  REQUIRE(fs.write_atomic("/lib/stderr.tcl", "for {set i 0} {$i < 1000} {incr i} { puts stderr flood }\n"
                                             "puts {v {xschem version=3.4.8RC file_version=1.2}}\n")
              .ok());
  schem::GeneratorInput flood{"/lib/flood.tcl", {}, "/w"};
  flood.max_output = std::size_t{1} << 20;
  flood.time_limit = milliseconds{1000};  // the `catch` loop goes on after the cap until the time limit
  const auto start = std::chrono::steady_clock::now();
  const auto flooded = schem::run_tcl_generator(fs, flood);
  CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds{5});
  CHECK_FALSE(flooded.text.has_value());
  REQUIRE_FALSE(flooded.diagnostics.empty());
  CHECK(flooded.diagnostics[0].text.ends_with("it printed more than 1048576 bytes"));
  const auto quiet = schem::run_tcl_generator(fs, {"/lib/stderr.tcl", {}, "/w"});
  REQUIRE(quiet.text.has_value());
  CHECK(*quiet.text == "v {xschem version=3.4.8RC file_version=1.2}\n");
}

TEST_CASE("an endless generator is stopped, whatever it waits on", sec::kPermissionModel) {
  for (const std::string script : {"while 1 {}", "after 600000", "vwait ::forever", "while 1 {catch {after 600000}}"}) {
    INFO(script);
    platform::MemoryFileSystem fs;
    REQUIRE(fs.write_atomic("/lib/hang.tcl", script + "\n").ok());
    schem::GeneratorInput input{"/lib/hang.tcl", {}, "/w"};
    input.time_limit = milliseconds{200};
    const auto start = std::chrono::steady_clock::now();
    const auto output = schem::run_tcl_generator(fs, input);
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds{5});
    CHECK_FALSE(output.text.has_value());
    REQUIRE(output.diagnostics.size() == 1);
    CHECK(output.diagnostics[0].text.ends_with("stopped after 200 ms"));
  }
}

// `xschemrc` files run when a document is opened (no prompt): every denial
// is reported, the settings before and after a caught denial are kept.
TEST_CASE("hostile operations in an xschemrc are denied and reported", sec::kPermissionModel) {
  for (const bool compat : {true, false}) {  // the reader does not depend on the mode
    CAPTURE(compat);
    sec::TempFolder document;
    sec::TempFolder outside;
    outside.write("evil.tcl", "set ::EVIL 1\n");
    std::string rc = "set BEFORE 1\n";
    for (const sec::Hostile& hostile : sec::hostile_operations()) {
      rc += "catch {" + sec::with_outside(hostile.script, outside.str()) + "}\ncatch {interp delete s}\n";
    }
    rc += "set AFTER 1\n";
    document.write("xschemrc", rc);
    platform::LocalFileSystem fs;
    schem::XschemrcInput input;
    input.document_folder = document.path();
    const schem::XschemrcResult result = schem::SearchPath::from_xschemrc(fs, input);
    CHECK(result.variables.count("BEFORE") == 1);
    CHECK(result.variables.count("AFTER") == 1);
    CHECK(result.variables.count("EVIL") == 0);
    const auto denials = sec::with_code_prefix(result.diagnostics, "SCRIPT-SANDBOX-");
    REQUIRE(denials.size() == sec::hostile_operations().size());
    for (std::size_t i = 0; i < denials.size(); ++i) {
      INFO(sec::hostile_operations()[i].name);
      CHECK(denials[i].code == sec::hostile_operations()[i].code);
    }
    CHECK_FALSE(std::filesystem::exists(outside.path() / "written"));
  }
}

TEST_CASE("an xschemrc that waits forever is stopped", sec::kPermissionModel) {
  for (const std::string script : {"after 600000", "vwait ::forever"}) {
    INFO(script);
    sec::TempFolder document;
    document.write("xschemrc", "set BEFORE 1\n" + script + "\nset AFTER 1\n");
    platform::LocalFileSystem fs;
    schem::XschemrcInput input;
    input.document_folder = document.path();
    input.time_limit = milliseconds{200};
    const auto start = std::chrono::steady_clock::now();
    const auto result = schem::SearchPath::from_xschemrc(fs, input);
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds{5});
    CHECK(result.variables.count("BEFORE") == 1);
    CHECK(result.variables.count("AFTER") == 0);
    CHECK(sec::with_code_prefix(result.diagnostics, "SCHEM-LIB-010").size() == 1);
  }
}
