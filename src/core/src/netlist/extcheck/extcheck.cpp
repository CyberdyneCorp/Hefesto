// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extcheck (netlist/extcheck/extcheck.hpp, counts.hpp; capability
// netlist-extcheck; openspec/changes/m7-extraction/design.md D15).
//
// main() of extcheck.c: EFArgs as the standalone build parses it (no
// MAGIC_WRAPPER: the usage text ends `[-T techname] rootfile`, a missing
// root or a bad flag prints it on the error stream with exit status 1, `-h`
// prints it on the output stream), EFReadFile(name, FALSE, FALSE, FALSE,
// FALSE) with no technology, EFFlatBuild with EF_FLATNODES | EF_FLATCAPS |
// EF_FLATRESISTS, the four visits and the report.
//
// The decisions applied here:
// - Q-NETLIST-36: the default thresholds are 2 fF and 10 ohms (EFargs.c),
//   not the manual page's 10 fF.
// - Q-NETLIST-50 (fix): no extraction style is loaded, so the read takes a
//   `scale` record's lscale as the style's (ReadOptions::units_per_lambda
//   unset); Magic's standalone program dereferences the unset style there.
// - A failed read prints `<program>: <code> <text>` and exits with status 1.
//   Magic goes on to EFFlatBuild with no root definition (a NULL
//   dereference); there is no magic-compat setting in a standalone program,
//   and a crash is fixed (design D15, X-2 tier 1).
//
// Messages of the read and the flattening are printed as Magic prints
// them: info (the `*** Global name ... not fully connected` report of
// efFlatGlobError, printf) on the output stream, warnings and errors
// (TxError) on the error stream, without a code.
//
// Ported from Magic 8.3.694 (fc5daa9e) extcheck/extcheck.c and
// extflat/EFargs.c, Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: SIM (design D3).

#include "hefesto/netlist/extcheck/extcheck.hpp"

#include <algorithm>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

#include "hefesto/cell/search_path.hpp"
#include "hefesto/diag/format.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extflat/args.hpp"
#include "hefesto/extflat/hierarchy.hpp"
#include "hefesto/netlist/extcheck/counts.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::netlist::extcheck {

namespace {

// EFargs.c:158-165, the non-MAGIC_WRAPPER text.
constexpr std::string_view kUsage =
    "Standard arguments: [-R] [-C] [-r rthresh] [-c cthresh] [-v]\n"
    "[-p searchpath] [-s sym=value] [-S symfile] [-t trimchars]\n"
    "[-T techname] rootfile\n";

// EFHNIsGlob, the non-Tcl test: the leaf component ends in `!`.
bool is_global(const std::vector<std::string>& name) {
  return !name.empty() && !name.back().empty() && name.back().back() == '!';
}

bool is_global(std::string_view name) {
  return !name.empty() && name.back() == '!';
}

// The capacitance and lumped resistance EFVisitNodes passes: fields EXTFLAT
// adds to FlatNode in M7 round 2 (tasks.md 10.x); the guard goes once they
// are on main.
template <class Flat>
void node_values(const Flat& flat, NodeSample& sample) {
  if constexpr (requires { flat.cap, flat.resistance; }) {
    sample.cap = static_cast<double>(flat.cap);
    sample.resist = flat.resistance;
  }
}

CheckInput check_input(const extflat::Netlist& netlist) {
  CheckInput input;
  input.devices = netlist.devices.size();
  for (const extflat::FlatCap& cap : netlist.caps) {
    input.caps.push_back(cap.cap);
  }
  for (const extflat::FlatResist& resist : netlist.resists) {
    input.resists.push_back(resist.resistance);
  }
  netlist.visit_nodes([&input](const extflat::FlatNode& flat) {
    NodeSample sample;
    sample.global = flat.names.empty() ? is_global(flat.name) : is_global(flat.names.front());
    node_values(flat, sample);
    input.nodes.push_back(sample);
  });
  return input;
}

// EFReadFile(name, FALSE, FALSE, FALSE, FALSE) (extcheck.c:81): no
// subcircuit marking, no `.res.ext` overlays, scaled. The fields are
// EXTFLAT's M7 additions to ReadOptions; the guard goes once they are on
// main.
template <class Options>
void extcheck_read(Options& options) {
  if constexpr (requires { options.subcircuits, options.resist_overlay, options.noscale; }) {
    options.subcircuits = false;
    options.resist_overlay = false;
    options.noscale = false;
  }
}

// EFFlatBuild with its messages: EXTFLAT's flatten(hierarchy, flags,
// options, sink) once it is on main, else the scaffold's flatten(), which
// reports nothing. A template so that the call is resolved when it is
// instantiated.
template <class Hierarchy>
diag::Outcome<extflat::Netlist> flatten_reporting(const Hierarchy& hierarchy, double cap_threshold,
                                                  diag::DiagnosticSink& sink) {
  constexpr extflat::FlatFlags kFlags =
      extflat::FlatFlags::flat_nodes | extflat::FlatFlags::flat_caps | extflat::FlatFlags::flat_resists;
  if constexpr (requires { extflat::flatten(hierarchy, kFlags, {cap_threshold}, sink); }) {
    return extflat::flatten(hierarchy, kFlags, {cap_threshold}, sink);
  } else {
    return extflat::flatten(hierarchy, kFlags);
  }
}

// The run's arguments, or the exit status after the usage text.
struct Parsed {
  extflat::CommonArgs args;
  std::optional<int> exit;
};

// The text EFArgs printed before the usage text (`-c requires a following
// cap threshold`), if any.
std::string usage_reason(const std::string& text) {
  const std::size_t usage = text.find("Standard arguments:");
  std::string reason = text.substr(0, usage);
  while (!reason.empty() && reason.back() == '\n') {
    reason.pop_back();
  }
  return reason;
}

template <class Args>
void print_arg_messages(const Args& args, std::ostream& err) {
  if constexpr (requires { args.messages; }) {
    for (const diag::Diagnostic& d : args.messages) {
      err << d.text << '\n';
    }
  }
}

Parsed parse(std::span<const std::string> words, std::ostream& out, std::ostream& err, const std::string& program) {
  auto args = extflat::parse_common_args(words, {});  // argsProc NULL: every other flag is a usage error
  if (!args.ok()) {
    const diag::Diagnostic& error = args.error();
    if (error.text.find("Standard arguments:") == std::string::npos) {
      err << program << ": " << diag::format_text(error) << '\n';  // not EFArgs' usage error
      return Parsed{{}, 1};
    }
    const std::string reason = usage_reason(error.text);
    const bool help = reason.empty() && std::find(words.begin(), words.end(), "-h") != words.end();
    if (!reason.empty()) {
      err << reason << '\n';
    }
    (help ? out : err) << kUsage;  // `-h`: TxPrintf; otherwise `goto usage`: TxError
    return Parsed{{}, 1};
  }
  print_arg_messages(args.value(), err);
  if (!args.value().cell) {
    err << kUsage;  // no root: `goto usage` (EFargs.c:305-316)
    return Parsed{{}, 1};
  }
  return Parsed{std::move(args).value(), std::nullopt};
}

void print_messages(const std::vector<diag::Diagnostic>& messages, std::ostream& out, std::ostream& err) {
  for (const diag::Diagnostic& d : messages) {
    (d.severity == diag::Severity::info ? out : err) << d.text << '\n';
  }
}

Thresholds thresholds(const extflat::CommonArgs& args) {
  Thresholds t;
  if (args.cap_infinite) {
    t.cap = kInfiniteCap;
  } else if (args.cap_threshold) {
    t.cap = *args.cap_threshold;
  }
  if (args.resist_infinite) {
    t.resist = kInfiniteResist;
  } else if (args.resist_threshold) {
    t.resist = *args.resist_threshold;
  }
  return t;
}

// RunStats(RS_MEM): informative, masked by the golden comparator.
std::string memory_used() {
#if defined(__unix__) || defined(__APPLE__)
  rusage usage{};
  if (getrusage(RUSAGE_SELF, &usage) == 0) {
#if defined(__APPLE__)
    const long kib = usage.ru_maxrss / 1024;  // bytes on Darwin
#else
    const long kib = usage.ru_maxrss;  // KiB on Linux
#endif
    return std::to_string(kib) + "k";
  }
#endif
  return "0k";
}

}  // namespace

Counts count(const CheckInput& input, const Thresholds& thresholds) {
  Counts c;
  c.devices = static_cast<int>(input.devices);  // devVisit
  if (thresholds.cap < kInfiniteCap) {         // IS_FINITE_F(EFCapThreshold)
    for (const double cap : input.caps) {      // capVisit
      ++c.caps;
      c.thresh_caps += cap / 1000. > thresholds.cap ? 1 : 0;
    }
  }
  if (thresholds.resist != kInfiniteResist) {
    for (const double res : input.resists) {  // resistVisit
      ++c.resists;
      c.thresh_resists += res / 1000. > static_cast<double>(thresholds.resist) ? 1 : 0;
    }
  }
  for (const NodeSample& node : input.nodes) {  // nodeVisit
    ++c.nodes;
    c.global_nodes += node.global ? 1 : 0;
    c.node_resists += (node.resist + 500) / 1000 > thresholds.resist ? 1 : 0;
    c.node_caps += (node.cap + 500) / 1000 > thresholds.cap ? 1 : 0;
  }
  return c;
}

std::string report(const Counts& c, const std::string& memory) {
  return "Memory used: " + memory + "\n" + std::to_string(c.devices) + " devices\n" + std::to_string(c.nodes) +
         " nodes (" + std::to_string(c.global_nodes) + " global, " + std::to_string(c.nodes - c.global_nodes) +
         " local)\n" + std::to_string(c.node_caps) + " nodes above capacitance threshold\n" +
         std::to_string(c.node_resists) + " nodes above resistance threshold\n" + std::to_string(c.caps) +
         " internodal capacitors (" + std::to_string(c.thresh_caps) + " above threshold)\n" +
         std::to_string(c.resists) + " explicit resistors (" + std::to_string(c.thresh_resists) +
         " above threshold)\n";
}

int run(std::span<const std::string> args, std::ostream& out, std::ostream& err, const std::string& program) {
  Parsed parsed = parse(args, out, err, program);
  if (parsed.exit) {
    return *parsed.exit;
  }
  platform::LocalFileSystem fs;
  const cell::SearchPaths paths;
  const cell::PathVariables vars;
  diag::CollectingSink sink;
  cell::LookupContext lookup{fs, paths, vars, sink};
  extflat::ReadOptions options;  // no technology, no style (Q-NETLIST-50)
  extcheck_read(options);
  options.search_path = parsed.args.search_path;
  const Thresholds limits = thresholds(parsed.args);
  auto hierarchy = extflat::read(lookup, *parsed.args.cell, options);
  diag::Outcome<extflat::Netlist> flat =
      hierarchy.ok() ? flatten_reporting(hierarchy.value(), limits.cap, sink) : hierarchy.error();
  print_messages(sink.take(), out, err);
  if (!flat.ok()) {
    err << program << ": " << diag::format_text(flat.error()) << '\n';
    return 1;
  }
  out << report(count(check_input(flat.value()), limits), memory_used());
  return 0;
}

}  // namespace hefesto::netlist::extcheck
