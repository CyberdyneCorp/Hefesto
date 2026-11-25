// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private: the `extresist` option handlers (capability extresist-command
// "Command options") and the console of the command.
//
// Owner: RESCMD.

#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "hefesto/cmd/command.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/extresist/command/session.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::extresist::detail {

inline constexpr std::string_view kReport = "EXTRESIST-CMD-001";   // TxPrintf
inline constexpr std::string_view kMessage = "EXTRESIST-CMD-002";  // TxError
inline constexpr std::string_view kWriteFailed = "EXTRESIST-CMD-003";

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
};

/// cmdExtresisCmd's indices (RES_ALL .. RES_HELP).
enum Option : std::size_t {
  kAll,
  kThreshold,
  kMinResist,
  kMinDelay,
  kTolerance,
  kSimplify,
  kExtout,
  kLumped,
  kSilent,
  kDebug,
  kSkip,
  kForce,
  kIgnore,
  kInclude,
  kBox,
  kCell,
  kBlackbox,
  kFasthenry,
  kGeometry,
  kStats,
  kHelp,
};

/// What a setting option needs.
struct OptionContext {
  ResistOptions& options;
  const tech::Technology* tech = nullptr;
  bool magic_compat = true;
  Console console;
};

/// The setting options (everything but all, box, cell, fasthenry, geometry
/// and help): `args` are the words after the option. nullopt for an
/// option that is not a setting.
[[nodiscard]] std::optional<cmd::CommandResult> setting_option(std::size_t option, std::span<const std::string> args,
                                                               OptionContext& ctx);

/// `fasthenry [freq]`: the frequency argument (Q-NETLIST-21: 10 MHz when
/// none was ever stored).
void fasthenry_frequency(std::span<const std::string> args, OptionContext& ctx);

/// The frequency a FastHenry run uses.
[[nodiscard]] double frequency(const ResistOptions& options) noexcept;

}  // namespace hefesto::extresist::detail
