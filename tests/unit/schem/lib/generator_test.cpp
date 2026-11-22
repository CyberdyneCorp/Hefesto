// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Symbol generators (capability schematic-symbols-and-libraries "Symbol
// generators"; Q-SCHEM-11): Tcl generators run in the sandbox and their
// output is the symbol; other generators are missing symbols.
//
// Owner: SYMLIB.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <iterator>
#include <string>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/lib/generator.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"
#include "hefesto/schem/net/substitute.hpp"

namespace schem = hefesto::schem;
namespace diag = hefesto::diag;
namespace platform = hefesto::platform;

namespace {

constexpr const char* kGenerators = "[cap:schematic-symbols-and-libraries][req:Symbol generators]";

// Prints a symbol with argv[0] inout pins named p0, p1, ...
constexpr std::string_view kPinsGenerator = R"tcl(
set n [lindex $argv 0]
puts "v {xschem version=3.4.8RC file_version=1.2}"
puts "K {type=subcircuit format=\"@name @pinlist @symname\" template=\"name=x1\"}"
for {set i 0} {$i < $n} {incr i} {
  puts "B 5 [expr {$i * 20}] 0 [expr {$i * 20 + 5}] 5 {name=p$i dir=inout}"
}
puts -nonewline "T {args=$argc:[join $argv ,]} 0 -20 0 0 0.2 0.2 {}\n"
puts stderr "not part of the symbol"
)tcl";

std::vector<diag::Diagnostic> with_code(const std::vector<diag::Diagnostic>& all, std::string_view code) {
  std::vector<diag::Diagnostic> out;
  std::copy_if(all.begin(), all.end(), std::back_inserter(out), [code](const auto& d) { return d.code == code; });
  return out;
}

struct Library {
  platform::MemoryFileSystem fs;
  schem::SearchPath path{{"/lib"}};
  schem::Document top;

  Library() {
    REQUIRE(fs.write_atomic("/lib/pins.tcl", kPinsGenerator).ok());
    top.path = "/w/top.sch";
  }
};

}  // namespace

TEST_CASE("Generator name in netlist", kGenerators) {
  CHECK(schem::symbol_name("ramdec(4, 8)") == "ramdec_4_8");
  CHECK(schem::symbol_name("res.tcl(1, 2.5)") == "res_tcl_1_2_5");
}

TEST_CASE("generator references are recognised and split", kGenerators) {
  CHECK(schem::is_generator_reference("ramdec(4, 8)"));
  CHECK(schem::is_generator_reference("res.tcl(1,2) \t"));
  CHECK(schem::is_generator_reference("lib/gen()"));
  CHECK_FALSE(schem::is_generator_reference("(4)"));
  CHECK_FALSE(schem::is_generator_reference("a b(4)"));
  CHECK_FALSE(schem::is_generator_reference("gen(4(5))"));
  CHECK_FALSE(schem::is_generator_reference("gen(4) x"));
  CHECK_FALSE(schem::is_generator_reference("inv.sym"));
  CHECK(schem::parse_generator("ramdec(4, 8)") == schem::GeneratorCall{"ramdec", {"4", "8"}});
  CHECK(schem::parse_generator("res.tcl( a ,b c)") == schem::GeneratorCall{"res.tcl", {"a", "b", "c"}});
  CHECK(schem::parse_generator("gen()") == schem::GeneratorCall{"gen", {}});
  CHECK_FALSE(schem::parse_generator("inv.sym").has_value());
}

TEST_CASE("a Tcl generator's output is its symbol", kGenerators) {
  for (const bool compat : {true, false}) {
    CAPTURE(compat);
    Library library;
    diag::CollectingSink sink;
    schem::SymbolCache cache;
    const schem::Symbol& symbol = cache.resolve({"pins.tcl(3, 8)"}, library.top, {library.path, library.fs, &sink, compat});
    REQUIRE_FALSE(symbol.placeholder);
    CHECK(symbol.reference == "pins.tcl(3, 8)");
    CHECK(symbol.file == "/lib/pins.tcl");
    REQUIRE(symbol.pins.size() == 3);
    CHECK(symbol.pins[2].name == "p2");
    REQUIRE(symbol.document.texts.size() == 1);
    CHECK(symbol.document.texts[0].text == "args=2:3,8");
    CHECK(sink.take().empty());
    // Generated once per reference; another argument list runs it again.
    CHECK(&cache.resolve({"pins.tcl(3, 8)"}, library.top, {library.path, library.fs, nullptr, compat}) == &symbol);
    CHECK(cache.resolve({"pins.tcl(1)"}, library.top, {library.path, library.fs, nullptr, compat}).pins.size() == 1);
  }
}

TEST_CASE("a generator that runs an external program is a missing symbol", kGenerators) {
  Library library;
  REQUIRE(library.fs.write_atomic("/lib/ramdec", "#!/bin/sh\necho 'v {}'\n").ok());
  REQUIRE(library.fs.write_atomic("/lib/tclgen", "#!/usr/bin/env tclsh\nputs {v {xschem version=3.4.8RC file_version=1.2}}\n").ok());
  CHECK_FALSE(schem::is_tcl_generator("/lib/ramdec", library.fs));
  CHECK(schem::is_tcl_generator("/lib/tclgen", library.fs));
  CHECK(schem::is_tcl_generator("/lib/pins.tcl", library.fs));
  diag::CollectingSink sink;
  schem::SymbolCache cache;
  const schem::ResolveContext context{library.path, library.fs, &sink};
  CHECK(cache.resolve({"ramdec(4, 8)"}, library.top, context).placeholder);
  CHECK(cache.resolve({"ramdec(2)"}, library.top, context).placeholder);
  const auto external = with_code(sink.take(), "SCHEM-LIB-011");
  REQUIRE(external.size() == 1);
  CHECK(external[0].text == "Symbol ramdec(4, 8): symbol generators that run external programs are not supported");
  CHECK_FALSE(cache.resolve({"tclgen(1)"}, library.top, context).placeholder);
  CHECK(cache.resolve({"nope(1)"}, library.top, context).placeholder);
  CHECK(with_code(sink.take(), "SCHEM-LIB-003").size() == 1);
}

TEST_CASE("a generator runs in the sandbox", kGenerators) {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/lib/evil.tcl",
                          "if {[catch {exec ls}]} { puts exec_denied }\n"
                          "if {[catch {open /etc/hosts r}]} { puts read_denied }\n"
                          "if {[catch {open /tmp/hefesto-generator-out w}]} { puts write_denied }\n"
                          "puts $env(HOME)\n")
              .ok());
  const auto output = schem::run_tcl_generator(fs, {"/lib/evil.tcl", {}, "/w/s"});
  REQUIRE(output.text.has_value());
  CHECK(*output.text == "exec_denied\nread_denied\nwrite_denied\n/w/s\n");
}

// Hangs and unbounded output are bounded in both modes: an endless
// generator would have hung the opening of every schematic that places it.
TEST_CASE("an endless or verbose generator is stopped", kGenerators) {
  CHECK(schem::kGeneratorTimeLimit == std::chrono::seconds{10});
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/lib/loop.tcl", "while 1 { catch { while 1 {} } }\n").ok());
  REQUIRE(fs.write_atomic("/lib/chatty.tcl", "while 1 { catch { puts [string repeat x 100] } }\n").ok());
  schem::GeneratorInput loop{"/lib/loop.tcl", {}, "/w"};
  loop.time_limit = std::chrono::milliseconds{200};
  const auto start = std::chrono::steady_clock::now();
  const auto stopped = schem::run_tcl_generator(fs, loop);
  CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds{10});
  CHECK_FALSE(stopped.text.has_value());
  REQUIRE(stopped.diagnostics.size() == 1);
  CHECK(stopped.diagnostics[0].code == "SCHEM-LIB-012");
  CHECK(stopped.diagnostics[0].text == "Symbol generator /lib/loop.tcl failed: stopped after 200 ms");

  schem::GeneratorInput chatty{"/lib/chatty.tcl", {}, "/w"};
  chatty.max_output = 1000;
  chatty.time_limit = std::chrono::milliseconds{2000};
  const auto verbose = schem::run_tcl_generator(fs, chatty);
  CHECK_FALSE(verbose.text.has_value());
  REQUIRE(verbose.diagnostics.size() == 1);
  CHECK(verbose.diagnostics[0].text == "Symbol generator /lib/chatty.tcl failed: it printed more than 1000 bytes");
}

TEST_CASE("a failing generator is a missing symbol, run once", kGenerators) {
  Library library;
  REQUIRE(library.fs.write_atomic("/lib/bad.tcl", "error {no such cell}\n").ok());
  diag::CollectingSink sink;
  schem::SymbolCache cache;
  const schem::ResolveContext context{library.path, library.fs, &sink};
  CHECK(cache.resolve({"bad.tcl(1)"}, library.top, context).placeholder);
  CHECK(cache.resolve({"bad.tcl(1)"}, library.top, context).placeholder);
  const auto failed = with_code(sink.take(), "SCHEM-LIB-012");
  REQUIRE(failed.size() == 1);
  CHECK(failed[0].text.find("no such cell") != std::string::npos);
  cache.forget("/lib/bad.tcl");
  CHECK(cache.resolve({"bad.tcl(1)"}, library.top, context).placeholder);
  CHECK(with_code(sink.take(), "SCHEM-LIB-012").size() == 1);
}
