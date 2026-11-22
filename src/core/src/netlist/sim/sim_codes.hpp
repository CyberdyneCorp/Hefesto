// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The NETLIST-SIM codes of netlist/sim/codes.inc, and the console helpers
// the `ext2sim` option handlers and run share.
//
// Owner: SIM (openspec/changes/m7-extraction/design.md D3).

#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::netlist::sim::console {

inline constexpr std::string_view kReport = "NETLIST-SIM-001";   // TxPrintf
inline constexpr std::string_view kMessage = "NETLIST-SIM-002";  // TxError
inline constexpr std::string_view kWriteFailed = "NETLIST-SIM-003";

using Emit = std::function<void(diag::Diagnostic)>;

inline void report(const Emit& emit, std::string text) {
  if (emit) {
    emit(diag::make(kReport, std::move(text)));
  }
}

inline void message(const Emit& emit, std::string text) {
  if (emit) {
    emit(diag::make(kMessage, std::move(text)));
  }
}

}  // namespace hefesto::netlist::sim::console
