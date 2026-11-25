// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// macOS confinement: sandbox_init with a Hefesto profile (design D9). The
// profile allows by default and then denies the network, process creation
// and every write outside the job directory; the read-only roots are denied
// for writing last, so they stay read-only even inside the job directory.
//
// Owner: SIMD.

#include <sandbox.h>
#include <stdlib.h>

#include <climits>
#include <string>

#include "sandbox/sandbox.hpp"

namespace hefesto::simd {

namespace {

// A path as SBPL string literal, after resolving symbolic links (the sandbox
// matches resolved paths: /tmp is /private/tmp).
std::string literal(const std::string& path) {
  char resolved[PATH_MAX];  // NOLINT(cppcoreguidelines-avoid-c-arrays)
  const std::string real = ::realpath(path.c_str(), resolved) != nullptr ? std::string{resolved} : path;
  std::string out = "\"";
  for (const char c : real) {
    if (c == '"' || c == '\\') {
      out += '\\';
    }
    out += c;
  }
  return out + "\"";
}

std::string profile(const Confinement& confinement) {
  std::string sbpl =
      "(version 1)\n"
      "(allow default)\n"
      "(deny network*)\n"
      "(deny system-socket)\n"
      "(deny process-fork)\n"
      "(deny process-exec*)\n"
      "(deny file-write*)\n"
      "(allow file-write* (literal \"/dev/null\") (literal \"/dev/dtracehelper\"))\n";
  if (!confinement.job_dir.empty()) {
    sbpl += "(allow file-write* (subpath " + literal(confinement.job_dir) + "))\n";
  }
  for (const std::string& root : confinement.readonly_roots) {
    sbpl += "(deny file-write* (subpath " + literal(root) + "))\n";
  }
  return sbpl;
}

}  // namespace

ConfinementResult confine(const Confinement& confinement) {
  ConfinementResult result;
  const std::string sbpl = profile(confinement);
  char* error = nullptr;
  // sandbox_init is deprecated but supported; it is what the App Sandbox
  // builds on, and the only public call that applies a custom profile.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  const int rc = ::sandbox_init(sbpl.c_str(), 0, &error);
  if (rc != 0) {
    result.error = std::string{"sandbox_init failed: "} + (error != nullptr ? error : "unknown error");
    ::sandbox_free_error(error);
    return result;
  }
#pragma GCC diagnostic pop
  result.ok = true;
  result.notes.emplace_back("confinement: macOS sandbox profile applied (no network, no process creation, writes "
                            "only in the job directory)");
  return result;
}

}  // namespace hefesto::simd
