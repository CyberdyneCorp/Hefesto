// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Consent before a PDK script runs (decision X-3;
// openspec/changes/m4-scripting/design.md D4/D7; capability hefesto-security
// "Consent before implicit script execution"): explicit startup (a project
// pinned to a PDK, `-rcfile`) and the implicit `<tech>.magicrc` of
// technology auto-discovery ask the ConsentHost unless the PDK version is
// trusted. Hosts: the desktop dialog (SHELL), the CLI policy (`--trust-pdk`,
// batch declines; PDKINT 6.2) and TestConsentHost.
//
// Owner: PDKINT (design D2); the ConsentHost interface is shared.

#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/platform/file_system.hpp"
#include "hefesto/script/types.hpp"

namespace hefesto::script {

enum class ConsentDecision : std::uint8_t {
  decline,        // do not run; the file opens read-only with the `minimum` tech
  run_once,       // run this time only
  trust_version,  // run and trust this PDK version from now on (TrustStore)
};

class ConsentHost {
 public:
  ConsentHost() = default;
  ConsentHost(const ConsentHost&) = delete;
  ConsentHost& operator=(const ConsentHost&) = delete;
  virtual ~ConsentHost() = default;

  /// Asks whether `script` of `pdk` may run.
  [[nodiscard]] virtual ConsentDecision ask(const PdkIdentity& pdk, const platform::Path& script) = 0;
};

/// The batch (CLI) policy (design D7): nothing can be asked, so a script
/// runs only when its PDK matches a `--trust-pdk <id>` selector; everything
/// else is declined. A selector is a family (`sky130`), a variant
/// (`sky130A`), `<variant>@<hash prefix>` or the identity's full
/// `family/variant@version`. Matching runs the script for this session only
/// (run_once); nothing is recorded in the PDK store.
class BatchConsentHost final : public ConsentHost {
 public:
  explicit BatchConsentHost(std::vector<std::string> selectors) : selectors_(std::move(selectors)) {}

  [[nodiscard]] ConsentDecision ask(const PdkIdentity& pdk, const platform::Path& script) override;
  /// True when one of the selectors names `pdk`.
  [[nodiscard]] bool matches(const PdkIdentity& pdk) const;

 private:
  std::vector<std::string> selectors_;
};

/// A test host: answers from a queue, then `fallback`; records every
/// question.
class TestConsentHost final : public ConsentHost {
 public:
  explicit TestConsentHost(ConsentDecision fallback = ConsentDecision::decline) : fallback_(fallback) {}

  void push_answer(ConsentDecision answer) { answers_.push_back(answer); }
  [[nodiscard]] ConsentDecision ask(const PdkIdentity& pdk, const platform::Path& script) override;
  [[nodiscard]] const std::vector<ConsentRequest>& requests() const noexcept { return requests_; }

 private:
  ConsentDecision fallback_;
  std::deque<ConsentDecision> answers_;
  std::vector<ConsentRequest> requests_;
};

}  // namespace hefesto::script
