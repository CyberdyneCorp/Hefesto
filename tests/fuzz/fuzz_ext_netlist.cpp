// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the netlisters over hostile `.ext` input (task 19.2,
// m7-extraction design D22; capabilities ext-flattening-read,
// netlist-spice-options, netlist-spice-devices, netlist-spice-hierarchy,
// netlist-sim; hefesto-testing-strategy "Parser fuzzing"). It extends
// fuzz_ext_reader past the read: the flattened netlist is written as flat
// SPICE (write_flat over from_netlist) and as a `.sim` file (write_sim over
// from_netlist), and with bit 5 the files are netlisted hierarchically by
// the `ext2spice hierarchy on` command on an in-memory file system.
//
// The input is a set of `.ext` files in /w, as fuzz_ext_reader's: the text
// before the first line `@@ <cell>` is `top.ext`, each `@@ <cell>` line
// starts `<cell>.ext`. The first byte is a mode:
//   bit 0  magic-compat OFF
//   bit 1  `short resistor`, `resistor tee on` (Q-NETLIST-31, Q-NETLIST-60)
//   bit 2  format: ngspice and merge aggressive (else spice3, no merge)
//   bit 3  global nets `vdd!` and `gnd!`
//   bit 4  ext2sim: format SU, merge conservative, aliases and labels
//   bit 5  the `ext2spice hierarchy on` command path
//   bit 6  infinite thresholds (else cthresh 0 and rthresh 0: every
//          coupling and resistor is written)
//
// Resource caps (harness, design D22; recorded as approved under the
// standing rule of 2026-11-25 in docs/decisions/m7-decision-table.md):
// inputs above 64 KiB are ignored, and the read runs with
// extflat::ReadLimits lowered to 4096 instances, 65536 flat names and
// merge elements, 64 cells and depth 16 (the command path through
// NetlistSession::read_limits), so a time-out is a real hang. The port
// number cap (2^20, Q-NETLIST-62) is the product's own.
//
// Owner: INTEGRATION (tests/fuzz/, design D2).

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

#include "ext_fuzz_files.hpp"
#include "fuzz_common.hpp"
#include "hefesto/cell/search_path.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/extflat/read.hpp"
#include "hefesto/netlist/sim/writer.hpp"
#include "hefesto/netlist/spice/input.hpp"
#include "hefesto/netlist/spice/writer.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/ui/line.hpp"

namespace {

namespace extflat = hefesto::extflat;
namespace spice = hefesto::netlist::spice;
namespace sim = hefesto::netlist::sim;

constexpr std::uint8_t kCompatOff = 0x01;
constexpr std::uint8_t kShorts = 0x02;
constexpr std::uint8_t kNgspice = 0x04;
constexpr std::uint8_t kGlobals = 0x08;
constexpr std::uint8_t kSim = 0x10;
constexpr std::uint8_t kHierarchy = 0x20;
constexpr std::uint8_t kInfinite = 0x40;

extflat::ReadLimits fuzz_limits() {
  extflat::ReadLimits limits;
  limits.max_depth = 16;
  limits.max_cells = 64;
  limits.max_instances = 4096;
  limits.max_flat_names = 65536;
  limits.max_connections = 65536;
  return limits;
}

extflat::ReadOptions read_options(std::uint8_t mode) {
  extflat::ReadOptions options;
  options.magic_compat = (mode & kCompatOff) == 0;
  options.search_path = {"/w"};
  options.limits = fuzz_limits();
  options.spice = true;
  options.shorts = (mode & kShorts) != 0 ? extflat::ReadOptions::PortShorts::resistor
                                         : extflat::ReadOptions::PortShorts::none;
  if ((mode & kGlobals) != 0) {
    options.global_nets = {"vdd!", "gnd!"};
  }
  return options;
}

spice::SpiceSettings spice_settings(std::uint8_t mode) {
  spice::SpiceSettings settings;
  settings.resistor_tee = (mode & kShorts) != 0;
  settings.shorts = (mode & kShorts) != 0 ? spice::ShortMode::resistor : spice::ShortMode::none;
  if ((mode & kNgspice) != 0) {
    settings.format = spice::Dialect::ngspice;
    settings.merge = spice::MergeMode::aggressive;
  }
  if ((mode & kInfinite) != 0) {
    settings.cthresh = std::numeric_limits<double>::infinity();
  } else {
    settings.cthresh = 0.0;
    settings.rthresh = 0;
  }
  return settings;
}

sim::WriteOptions sim_options(std::uint8_t mode) {
  sim::WriteOptions options;
  options.settings.format = sim::SimFormat::su;
  options.settings.merge = sim::SimMerge::conservative;
  options.settings.alias = true;
  options.settings.labels = true;
  if ((mode & kInfinite) != 0) {
    options.cap_threshold = sim::kInfiniteCap;
    options.resist_threshold = sim::kInfiniteResist;
  } else {
    options.cap_threshold = 0.0F;
    options.resist_threshold = 0;
  }
  return options;
}

/// The flat read, then flat SPICE and, with kSim, a `.sim` file.
void write_flat_outputs(std::uint8_t mode, std::string_view files) {
  hefesto::platform::MemoryFileSystem fs;
  hefesto::fuzz::store_marked_files(fs, files, "/w", "top", ".ext");
  const hefesto::cell::SearchPaths paths{{"/w"}, {}, {}};
  const hefesto::cell::PathVariables vars;
  hefesto::diag::NullSink sink;
  hefesto::cell::LookupContext ctx{fs, paths, vars, sink};
  const auto netlist = extflat::read_flat(ctx, "top", read_options(mode));
  if (!netlist.ok()) {
    return;
  }
  hefesto::compat::CompatSettings compat;
  compat.magic_compat = (mode & kCompatOff) == 0;
  extflat::NameFlags names;
  const spice::SpiceInput input = spice::from_netlist(netlist.value(), "top");
  (void)spice::write_flat(input, spice_settings(mode), spice::RunFlags{}, compat, names);
  if ((mode & kSim) != 0) {
    extflat::NameFlags sim_names;
    (void)sim::write_sim(sim::from_netlist(netlist.value()), sim_options(mode), sim_names, compat);
  }
}

/// `ext2spice hierarchy on` through the command, as a user runs it.
void write_hierarchy(std::uint8_t mode, std::string_view files) {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  project.compat.magic_compat = (mode & kCompatOff) == 0;
  hefesto::cmd::ScriptSession script{project};
  hefesto::cmd::CommandRegistry registry;
  if (!hefesto::cmd::register_all_commands(registry)) {
    return;
  }
  hefesto::editor::EditorState state;
  state.netlist.read_limits = fuzz_limits();
  hefesto::cmd::Dispatcher dispatcher{registry, services.view(), project, script};
  dispatcher.set_editor(&state);
  hefesto::fuzz::store_marked_files(services.view().files, files, "/w", "top", ".ext");
  const bool shorts = (mode & kShorts) != 0;
  for (const std::string_view line :
       {std::string_view{"ext2spice hierarchy on"}, shorts ? std::string_view{"ext2spice short resistor"} : "",
        (mode & kNgspice) != 0 ? std::string_view{"ext2spice format ngspice"} : "",
        std::string_view{"ext2spice -p /w -o /w/out.spice top"}}) {
    if (!line.empty()) {
      (void)hefesto::ui::run_command_line(dispatcher, line, hefesto::cmd::Origin::script);
    }
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size > hefesto::fuzz::kMaxExtInput) {
    return 0;
  }
  const auto [mode, rest] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  write_flat_outputs(mode, rest);
  if ((mode & kHierarchy) != 0) {
    write_hierarchy(mode, rest);
  }
  return 0;
}
