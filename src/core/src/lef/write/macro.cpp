// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF macro construction and number output, scaffold stubs
// (openspec/changes/m6-lef-def-oasis/design.md D7).
//
// Owner: LEFW (design D2).

#include "hefesto/lef/write/macro.hpp"
#include "hefesto/lef/write/number.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::lef {

namespace {

constexpr std::string_view kWhat = "lef write";

}  // namespace

diag::Status MacroBuilder::flatten() {
  return cmd::not_implemented_status(kWhat);
}

diag::Status MacroBuilder::type_sets() {
  return cmd::not_implemented_status(kWhat);
}

diag::Status MacroBuilder::pins() {
  return cmd::not_implemented_status(kWhat);
}

diag::Status MacroBuilder::antenna() {
  return cmd::not_implemented_status(kWhat);
}

diag::Status MacroBuilder::geometry(std::string& /*out*/) {
  return cmd::not_implemented_status(kWhat);
}

std::string format_distance(float /*value*/, LefUnits /*units*/) {
  return {};
}

}  // namespace hefesto::lef
