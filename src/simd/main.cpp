// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-simd: the simulation worker (openspec/changes/m9-lvs-simulation
// design D2, D9; capability simulation-engine-embedding "Crash containment
// on desktop", "Worker confinement"). It confines itself, then loads the
// hefesto-ngspice-adapter plugin, then serves the frames of
// hefesto/sim/engine/frames.hpp on standard input and output.
//
//   hefesto-simd [--job-dir=<dir>] [--readonly-root=<dir>]... [--adapter=<plugin>]
//                [--heartbeat-ms=<n>]
//
// The core starts it through platform::WorkerHost, which adds --job-dir and
// --readonly-root from the WorkerSpec. --adapter replaces the plugin found
// next to the executable (tests load a stand-in adapter this way); it is
// loaded after confinement like the default one. --heartbeat-ms lowers the
// 2 s heartbeat interval (it cannot raise it).
//
// Owner: SIMD.

#include <unistd.h>

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "ipc/frame_io.hpp"
#include "sandbox/sandbox.hpp"
#include "server/plugin.hpp"
#include "server/server.hpp"

namespace {

using hefesto::simd::Confinement;

constexpr std::chrono::milliseconds kHeartbeat{2000};

struct Options {
  Confinement confinement;
  std::string adapter;
  std::chrono::milliseconds heartbeat = kHeartbeat;
  std::string error;
};

bool take(std::string_view arg, std::string_view name, std::string& value) {
  if (!arg.starts_with(name)) {
    return false;
  }
  value = std::string{arg.substr(name.size())};
  return true;
}

Options parse(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg{argv[i]};  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    std::string value;
    if (take(arg, "--job-dir=", options.confinement.job_dir) || take(arg, "--adapter=", options.adapter)) {
      continue;
    }
    if (take(arg, "--readonly-root=", value)) {
      options.confinement.readonly_roots.push_back(value);
    } else if (take(arg, "--heartbeat-ms=", value)) {
      const long ms = std::strtol(value.c_str(), nullptr, 10);
      options.heartbeat = std::chrono::milliseconds{ms > 0 && ms < kHeartbeat.count() ? ms : kHeartbeat.count()};
    } else {
      options.error = "unknown argument " + std::string{arg};
    }
  }
  return options;
}

int startup_error(hefesto::simd::FrameWriter& out, const std::string& message) {
  out.write(hefesto::sim::frames::Output{hefesto::sim::frames::Stream::error, "hefesto-simd: " + message});
  return hefesto::simd::kExitStartup;
}

}  // namespace

int main(int argc, char** argv) {
  std::signal(SIGPIPE, SIG_IGN);
  hefesto::simd::FrameWriter out{STDOUT_FILENO};
  const Options options = parse(argc, argv);
  if (!options.error.empty()) {
    return startup_error(out, options.error);
  }
  // Found before confinement; loaded after it (design D9).
  const std::vector<std::string> candidates = hefesto::simd::adapter_candidates(hefesto::simd::executable_dir());
  const hefesto::simd::ConfinementResult confined = hefesto::simd::confine(options.confinement);
  if (!confined.ok) {
    return startup_error(out, "confinement failed: " + confined.error);
  }
  const hefesto::simd::LoadedPlugin plugin = hefesto::simd::load_adapter(options.adapter, candidates);
  if (plugin.api == nullptr) {
    return startup_error(out, plugin.error);
  }
  hefesto::simd::Server server{*plugin.api, out, options.heartbeat};
  return server.run(STDIN_FILENO, confined.notes);
}
