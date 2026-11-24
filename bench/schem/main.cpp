// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-bench-schem: load, save and netlist time of one schematic
// (openspec/changes/m8-schematic task 14.1; capability hefesto-performance).
//
// Times --repeat runs of each step through the public schematic API:
//   - load: schem::load_document (read and parse the top schematic);
//   - save: schem::save_document of the loaded document to --out-dir;
//   - netlist: schem::netlist over a NetlistEnvironment with a fresh
//     SymbolCache per run (so symbol and subcircuit loading is timed too),
//     the search path of the PDK's xschemrc (--pdk-root, --pdk) or of the
//     system library and --library folders, and a TclBridge over a
//     TclEnvironment loaded with that xschemrc (tcleval values evaluated);
//   - flat (--flat): the same in `flat` mode.
// It prints a JSON report: the median and every run of each step, the
// document's record counts, the netlist size and line count, the
// diagnostics by severity, and the peak resident set.
// bench/schem/schem_bench.py picks the workloads and compares the report
// with bench/schem/baseline.json.
//
//   hefesto-bench-schem --schematic FILE --sharedir DIR --out-dir DIR [--name NAME]
//                       [--pdk-root DIR --pdk NAME] [--library DIR]... [--repeat N]
//                       [--flat] [--json FILE]
//
// Owner: PERF (m8-schematic task 14.1).

#include <sys/resource.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/file/save.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"
#include "hefesto/schem/lib/xschemrc.hpp"
#include "hefesto/schem/net/netlist.hpp"
#include "hefesto/schem/tcl/tcl_environment.hpp"

namespace {

namespace schem = hefesto::schem;
namespace platform = hefesto::platform;
namespace diag = hefesto::diag;
namespace fs = std::filesystem;
using nlohmann::json;

struct Options {
  std::string name;
  fs::path schematic;
  fs::path sharedir;  // XSCHEM_SHAREDIR: <CAD_ROOT>/assets/schematic
  fs::path out_dir;
  fs::path pdk_root;
  std::string pdk;
  std::vector<fs::path> libraries;
  int repeat = 3;
  bool flat = false;
  std::string json_path;
};

void usage() {
  std::cerr << "usage: hefesto-bench-schem --schematic FILE --sharedir DIR --out-dir DIR [--name NAME]\n"
               "                           [--pdk-root DIR --pdk NAME] [--library DIR]... [--repeat N]\n"
               "                           [--flat] [--json FILE]\n";
}

bool apply_option(Options& o, std::string_view flag, const char* value) {
  static const std::map<std::string_view, std::function<void(Options&, const char*)>> kSetters{
      {"--name", [](Options& x, const char* v) { x.name = v; }},
      {"--schematic", [](Options& x, const char* v) { x.schematic = v; }},
      {"--sharedir", [](Options& x, const char* v) { x.sharedir = v; }},
      {"--out-dir", [](Options& x, const char* v) { x.out_dir = v; }},
      {"--pdk-root", [](Options& x, const char* v) { x.pdk_root = v; }},
      {"--pdk", [](Options& x, const char* v) { x.pdk = v; }},
      {"--library", [](Options& x, const char* v) { x.libraries.emplace_back(v); }},
      {"--repeat", [](Options& x, const char* v) { x.repeat = std::max(1, std::atoi(v)); }},
      {"--json", [](Options& x, const char* v) { x.json_path = v; }},
  };
  const auto setter = kSetters.find(flag);
  if (setter == kSetters.end()) return false;
  setter->second(o, value);
  return true;
}

std::optional<Options> parse(int argc, char** argv) {
  Options o;
  for (int i = 1; i < argc; ++i) {
    const std::string_view flag = argv[i];
    if (flag == "--flat") {
      o.flat = true;
      continue;
    }
    if (i + 1 >= argc || !apply_option(o, flag, argv[i + 1])) return std::nullopt;
    ++i;
  }
  if (o.schematic.empty() || o.sharedir.empty() || o.out_dir.empty()) return std::nullopt;
  if (o.pdk_root.empty() != o.pdk.empty()) return std::nullopt;
  if (o.name.empty()) o.name = o.schematic.stem().string();
  o.schematic = fs::absolute(o.schematic).lexically_normal();
  o.sharedir = fs::absolute(o.sharedir).lexically_normal();
  return o;
}

using Clock = std::chrono::steady_clock;

double seconds_since(Clock::time_point start) {
  return std::chrono::duration<double>(Clock::now() - start).count();
}

// The median and every run of one timed step.
json timing(std::vector<double> runs) {
  std::vector<double> sorted = runs;
  std::sort(sorted.begin(), sorted.end());
  const std::size_t n = sorted.size();
  const double median = n % 2 == 1 ? sorted[n / 2] : (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0;
  return json{{"median", median}, {"runs", std::move(runs)}};
}

std::uint64_t peak_rss_bytes() {
  rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
#ifdef __APPLE__
  return static_cast<std::uint64_t>(usage.ru_maxrss);  // bytes on macOS
#else
  return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024U;  // KiB on Linux
#endif
}

// The search path, the xschemrc variables and the tcleval interpreter of
// one document, as `schematic netlist` builds them for an open document.
class Environment {
 public:
  explicit Environment(const Options& o) : tcl_(editor_, policy(o), files_) {
    std::vector<platform::Path> front(o.libraries.begin(), o.libraries.end());
    if (!o.pdk.empty()) {
      const schem::XschemrcResult rc = tcl_.load_xschemrc(input_);
      rc_diagnostics_ = rc.diagnostics.size();
      search_path_ = rc.search_path.prepended(schem::SearchPath{front});
      return;
    }
    front.push_back(o.sharedir / "xschem_library" / "devices");
    search_path_ = schem::SearchPath{front};
  }

  [[nodiscard]] platform::FileSystem& files() noexcept { return files_; }
  [[nodiscard]] const schem::SearchPath& search_path() const noexcept { return search_path_; }
  [[nodiscard]] schem::TclBridge bridge() const noexcept { return tcl_.bridge(); }
  [[nodiscard]] const platform::Path& pdk_root() const noexcept { return pdk_root_; }
  [[nodiscard]] std::size_t rc_diagnostics() const noexcept { return rc_diagnostics_; }

 private:
  hefesto::script::SandboxPolicy policy(const Options& o) {
    const platform::Path folder = o.schematic.parent_path();
    if (o.pdk.empty()) {
      input_.document_folder = folder;
      input_.variables = {{"XSCHEM_SHAREDIR", o.sharedir.generic_string()}};
    } else {
      input_ = schem::pdk_xschemrc_input(o.pdk_root, o.pdk, o.sharedir, folder / ".xschem", folder);
      pdk_root_ = o.pdk_root;
    }
    return schem::xschemrc_policy(input_);
  }

  hefesto::editor::EditorState editor_;
  platform::LocalFileSystem files_;
  schem::XschemrcInput input_;
  platform::Path pdk_root_;  // empty without a PDK
  schem::TclEnvironment tcl_;
  schem::SearchPath search_path_;
  std::size_t rc_diagnostics_ = 0;
};

schem::ReadOptions read_options(const fs::path& file, diag::DiagnosticSink& sink) {
  schem::ReadOptions options;
  options.warnings = &sink;
  options.source_name = file.generic_string();
  return options;
}

json document_counts(const schem::Document& d) {
  return json{{"instances", d.instances.size()}, {"wires", d.wires.size()}, {"texts", d.texts.size()}};
}

// Loads the schematic --repeat times; the last document, or nullopt.
std::optional<schem::Document> time_load(Environment& env, const Options& o, json& result) {
  std::vector<double> runs;
  std::optional<schem::Document> document;
  for (int i = 0; i < o.repeat; ++i) {
    diag::NullSink sink;
    const auto start = Clock::now();
    auto loaded = schem::load_document(env.files(), o.schematic, read_options(o.schematic, sink));
    runs.push_back(seconds_since(start));
    if (!loaded.ok()) {
      result["error"] = "load failed: " + loaded.error().text;
      return std::nullopt;
    }
    document = std::move(loaded.value());
  }
  result["load_s"] = timing(std::move(runs));
  result["file_bytes"] = fs::file_size(o.schematic);
  result["document"] = document_counts(*document);
  return document;
}

bool time_save(Environment& env, const Options& o, const schem::Document& document, json& result) {
  std::vector<double> runs;
  std::string file = o.name;  // workload names may hold `/` (pdk/folder/cell)
  std::replace(file.begin(), file.end(), '/', '_');
  const fs::path destination = o.out_dir / (file + ".sch");
  for (int i = 0; i < o.repeat; ++i) {
    schem::Document copy = document;
    const auto start = Clock::now();
    const diag::Status saved = schem::save_document(env.files(), copy, destination, {true});
    runs.push_back(seconds_since(start));
    if (!saved.ok()) {
      result["error"] = "save failed: " + saved.error().text;
      return false;
    }
  }
  result["save_s"] = timing(std::move(runs));
  return true;
}

json severity_counts(const std::vector<diag::Diagnostic>& diagnostics) {
  json counts{{"error", 0}, {"warning", 0}, {"info", 0}};
  for (const diag::Diagnostic& d : diagnostics) {
    const std::string key{diag::severity_name(d.severity)};
    counts[key] = counts.value(key, 0) + 1;
  }
  return counts;
}

json netlist_summary(const schem::NetlistResult& r) {
  const auto lines = std::count(r.text.begin(), r.text.end(), '\n');
  return json{{"bytes", r.text.size()},
              {"lines", lines},
              {"failed", r.failed},
              {"diagnostics", severity_counts(r.diagnostics)}};
}

// Netlists --repeat times, each with a fresh symbol cache.
json time_netlist(Environment& env, const Options& o, const schem::Document& document, schem::Hierarchy mode) {
  std::vector<double> runs;
  schem::NetlistResult last;
  const schem::TclBridge bridge = env.bridge();
  schem::NetlistOptions options;
  options.hierarchy = mode;
  for (int i = 0; i < o.repeat; ++i) {
    schem::SymbolCache symbols;
    const schem::NetlistEnvironment environment{symbols, env.search_path(), env.files(), &bridge, env.pdk_root()};
    const auto start = Clock::now();
    last = schem::netlist(environment, document, options, hefesto::compat::CompatSettings{});
    runs.push_back(seconds_since(start));
  }
  json out = timing(std::move(runs));
  out["output"] = netlist_summary(last);
  return out;
}

json run(const Options& o) {
  json result{{"workload", o.name}, {"schematic", o.schematic.generic_string()}, {"pdk", o.pdk}, {"repeat", o.repeat}};
  fs::create_directories(o.out_dir);
  Environment env{o};
  result["xschemrc_diagnostics"] = env.rc_diagnostics();
  const std::optional<schem::Document> document = time_load(env, o, result);
  if (!document || !time_save(env, o, *document, result)) return result;
  result["netlist_s"] = time_netlist(env, o, *document, schem::Hierarchy::hier);
  if (o.flat) result["flat_s"] = time_netlist(env, o, *document, schem::Hierarchy::flat);
  result["peak_rss_bytes"] = peak_rss_bytes();
  return result;
}

}  // namespace

int main(int argc, char** argv) {
  const std::optional<Options> options = parse(argc, argv);
  if (!options) {
    usage();
    return 2;
  }
  const json result = run(*options);
  const json report{{"bench", "schem"}, {"results", json::array({result})}};
  if (!options->json_path.empty()) {
    std::ofstream out(options->json_path);
    out << report.dump(2) << '\n';
  }
  std::cout << report.dump(2) << '\n';
  return result.contains("error") ? 1 : 0;
}
