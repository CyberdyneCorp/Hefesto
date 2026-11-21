// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub (tasks.md 5.2): every evaluation fails with kTclFailure.
//
// Owner: SYMLIB.

#include "hefesto/schem/tcl/tcl_bridge.hpp"

namespace hefesto::schem {

std::string TclBridge::evaluate(std::string_view /*value*/, const TclContext& /*context*/) const {
  static_cast<void>(interpreter_);  // used once SYMLIB implements the evaluation
  return std::string{kTclFailure};
}

}  // namespace hefesto::schem
