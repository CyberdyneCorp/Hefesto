// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// PromptHost: the front end's answer to Magic's TxDialog prompts
// (openspec/changes/m2-interactive-editing/design.md D4): crash recovery,
// `flush` without `-noprompt`, `quit`/`load` with a modified color map,
// `quit` with modified cells. Commands reach the host through
// editor::EditorState::prompt and ask through cmd::ask(), which gives the
// default answer when no host is installed. Headless front ends (hefesto-cli,
// tests) install none or a HeadlessPromptHost; the desktop shell shows a
// Raven message box (SHELL task 10.6).
// (Magic ref: textio/txInput.c TxDialog)
//
// Owner: INTEGRATION (shared interface); SHELL implements the desktop host.

#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace hefesto::cmd {

class PromptHost {
 public:
  PromptHost() = default;
  PromptHost(const PromptHost&) = delete;
  PromptHost& operator=(const PromptHost&) = delete;
  virtual ~PromptHost() = default;

  /// Asks `question` and returns the index of the chosen entry of `answers`
  /// (e.g. {"no", "yes"}). `default_answer` is returned when the user
  /// cancels; it is always a valid index of `answers`.
  [[nodiscard]] virtual std::size_t ask(std::string_view question, std::span<const std::string> answers,
                                        std::size_t default_answer) = 0;

  /// ask(), or nullopt when the question could not be asked at all (Magic's
  /// Tcl TxDialog returns -1 when `tk_dialog` gives no integer, e.g. under
  /// `-dnull`). The default asks.
  [[nodiscard]] virtual std::optional<std::size_t> try_ask(std::string_view question,
                                                           std::span<const std::string> answers,
                                                           std::size_t default_answer) {
    return ask(question, answers, default_answer);
  }
};

/// The host of a session without a user: every question gets its default.
class HeadlessPromptHost final : public PromptHost {
 public:
  [[nodiscard]] std::size_t ask(std::string_view question, std::span<const std::string> answers,
                                std::size_t default_answer) override;
  /// Nobody can be asked: always nullopt.
  [[nodiscard]] std::optional<std::size_t> try_ask(std::string_view question, std::span<const std::string> answers,
                                                   std::size_t default_answer) override;
};

/// Asks through `host`, or returns `default_answer` when `host` is null.
/// An out-of-range answer from a host is replaced by `default_answer`.
[[nodiscard]] std::size_t ask(PromptHost* host, std::string_view question, std::span<const std::string> answers,
                              std::size_t default_answer);

/// A `no`/`yes` confirmation (default `no`) of a caller that, in Magic,
/// stops only on the answer `no` (`tech load`, `save` over another file,
/// `flush`, the color map's "lose the changes?" on `quit` and `load`, `quit`
/// with modified cells). True to go ahead. A question that cannot be asked (no host,
/// a headless host, or a script host without `tk_dialog`) goes ahead with
/// `magic_compat` ON, as Magic's -1 does, and is declined with it OFF
/// (Q-SCRIPT-48).
[[nodiscard]] bool confirm(PromptHost* host, std::string_view question, bool magic_compat);

/// The answer to that confirmation: true for any answer but `no`, nullopt
/// when it could not be asked.
[[nodiscard]] std::optional<bool> try_confirm(PromptHost* host, std::string_view question);

}  // namespace hefesto::cmd
