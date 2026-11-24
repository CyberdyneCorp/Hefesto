// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The shorted-ports warning of extOutputNodes (ExtBasic.c:815-838;
// Q-EXTRACT-22 per mode; tasks.md 6.7). The node writer calls it for each
// further port label of a node.
//
// Owner: EXTCAP.

#include <string>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/extract/cap/caps.hpp"
#include "hefesto/extract/cap/compat_branches.hpp"

namespace hefesto::extract {

namespace {

std::string_view cut_unique(std::string_view name) {
  const std::size_t at = name.find("_uq");
  return at == std::string_view::npos ? name : name.substr(0, at);
}

}  // namespace

std::optional<diag::Diagnostic> shorted_ports_warning(std::string_view node_name, std::string_view label,
                                                      const ExtractOptions& options,
                                                      const compat::CompatSettings& compat) {
  if (node_name == label || cap::shorted_ports_warning_suppressed(compat, options.lumped, options.extresist)) {
    return std::nullopt;
  }
  const std::string_view a = cut_unique(node_name);
  const std::string_view b = cut_unique(label);
  if (a == b) {
    return std::nullopt;
  }
  return diag::make("EXTRACT-CAP-001", "Warning:  Ports \"" + std::string{a} + "\" and \"" + std::string{b} +
                                           "\" are electrically shorted.");
}

}  // namespace hefesto::extract
