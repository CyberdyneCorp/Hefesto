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
};

/// The host of a session without a user: every question gets its default.
class HeadlessPromptHost final : public PromptHost {
 public:
  [[nodiscard]] std::size_t ask(std::string_view question, std::span<const std::string> answers,
                                std::size_t default_answer) override;
};

/// Asks through `host`, or returns `default_answer` when `host` is null.
/// An out-of-range answer from a host is replaced by `default_answer`.
[[nodiscard]] std::size_t ask(PromptHost* host, std::string_view question, std::span<const std::string> answers,
                              std::size_t default_answer);

}  // namespace hefesto::cmd
