// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs of the M7 extflat contracts (hierarchy.hpp, args.hpp,
// names.hpp): CORE-NOTIMPL-008 or neutral values until EXTFLAT lands them
// (openspec/changes/m7-extraction tasks.md 10.x). EXTFLAT replaces this
// file with its own sources.
//
// Owner: EXTFLAT (design D3).

#include "hefesto/cmd/m7_stubs.hpp"
#include "hefesto/extflat/args.hpp"
#include "hefesto/extflat/hierarchy.hpp"
#include "hefesto/extflat/names.hpp"

namespace hefesto::extflat {

namespace {

std::string joined(std::span<const std::string> components) {
  std::string out;
  for (const std::string& part : components) {
    out += (out.empty() ? "" : "/") + part;
  }
  return out;
}

}  // namespace

diag::Outcome<Hierarchy> read(cell::LookupContext& /*ctx*/, std::string_view /*root*/,
                              const ReadOptions& /*options*/) {
  return cmd::m7_not_implemented_diagnostic("extflat read");
}

Netlist flatten(const Hierarchy& /*hierarchy*/, FlatFlags /*flags*/) {
  return Netlist{};
}

diag::Outcome<std::vector<OverlayRecord>> read_res_overlay(cell::LookupContext& /*ctx*/, std::string_view /*cell*/,
                                                           const ReadOptions& /*options*/) {
  return cmd::m7_not_implemented_diagnostic("extflat res.ext overlay");
}

diag::Outcome<CommonArgs> parse_common_args(std::span<const std::string> /*args*/, const ExtraFlags& /*extra*/) {
  return cmd::m7_not_implemented_diagnostic("extflat arguments");
}

std::string name_to_spice(std::span<const std::string> components, SpiceDialect /*dialect*/, NameFlags& /*flags*/,
                          const compat::CompatSettings& /*compat*/) {
  return joined(components);
}

std::string name_to_sim(std::span<const std::string> components, NameFlags& /*flags*/,
                        const compat::CompatSettings& /*compat*/) {
  return joined(components);
}

}  // namespace hefesto::extflat
