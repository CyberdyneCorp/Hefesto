// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs of extresist/output (output.hpp): CORE-NOTIMPL-008 until
// RESCMD lands them (openspec/changes/m7-extraction tasks.md, RESCMD).
// RESCMD replaces this file with its own sources.
//
// Owner: RESCMD (design D3).

#include "hefesto/cmd/m7_stubs.hpp"
#include "hefesto/extresist/output/output.hpp"

namespace hefesto::extresist {

diag::Outcome<InputModel> read_input(cell::LookupContext& /*ctx*/, std::string_view /*cell*/,
                                     const compat::CompatSettings& /*compat*/) {
  return cmd::m7_not_implemented_diagnostic("extresist input");
}

diag::Outcome<std::string> write_res_ext(const InputModel& /*input*/, const std::vector<Network>& /*nets*/,
                                         const compat::CompatSettings& /*compat*/) {
  return cmd::m7_not_implemented_diagnostic("extresist res.ext");
}

diag::Outcome<std::string> write_res_lump(const InputModel& /*input*/, const std::vector<Network>& /*nets*/) {
  return cmd::m7_not_implemented_diagnostic("extresist res.lump");
}

diag::Outcome<std::string> write_fasthenry(const InputModel& /*input*/, const std::vector<Network>& /*nets*/,
                                           double /*frequency*/, const compat::CompatSettings& /*compat*/) {
  return cmd::m7_not_implemented_diagnostic("extresist fasthenry");
}

diag::Outcome<std::vector<GeometryElement>> geometry_elements(const Network& /*net*/) {
  return cmd::m7_not_implemented_diagnostic("extresist geometry");
}

}  // namespace hefesto::extresist
