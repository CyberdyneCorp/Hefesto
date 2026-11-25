// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private: the console of `antennacheck` (Magic's TxPrintf and TxError) as
// ANTENNA-CHECK-001 and ANTENNA-CHECK-002 diagnostics.
//
// Owner: ANTENNA.

#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::antenna {

inline constexpr std::string_view kReport = "ANTENNA-CHECK-001";   // TxPrintf
inline constexpr std::string_view kMessage = "ANTENNA-CHECK-002";  // TxError

struct Console {
  std::function<void(diag::Diagnostic)> emit;

  void report(std::string text) const {
    if (emit) {
      emit(diag::make(kReport, std::move(text)));
    }
  }
  void message(std::string text) const {
    if (emit) {
      emit(diag::make(kMessage, std::move(text)));
    }
  }
  void forward(diag::Diagnostic d) const {
    if (emit) {
      emit(std::move(d));
    }
  }
};

}  // namespace hefesto::antenna
