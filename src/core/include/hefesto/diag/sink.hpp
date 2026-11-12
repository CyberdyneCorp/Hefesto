// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic sinks: the interface every emitter writes to.

#pragma once

#include <mutex>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::diag {

class DiagnosticSink {
 public:
  DiagnosticSink() = default;
  DiagnosticSink(const DiagnosticSink&) = delete;
  DiagnosticSink& operator=(const DiagnosticSink&) = delete;
  virtual ~DiagnosticSink() = default;

  virtual void emit(const Diagnostic& diagnostic) = 0;
};

/// Keeps every diagnostic in emission order. Thread-safe, so background tasks
/// may emit into it.
class CollectingSink final : public DiagnosticSink {
 public:
  void emit(const Diagnostic& diagnostic) override;

  /// Moves the collected diagnostics out and leaves the sink empty.
  [[nodiscard]] std::vector<Diagnostic> take();
  [[nodiscard]] std::vector<Diagnostic> snapshot() const;
  /// The diagnostics collected from index `first` on (none when there are
  /// fewer).
  [[nodiscard]] std::vector<Diagnostic> snapshot_from(std::size_t first) const;
  [[nodiscard]] bool has_errors() const;

 private:
  mutable std::mutex mutex_;
  std::vector<Diagnostic> diagnostics_;
};

/// Discards everything.
class NullSink final : public DiagnosticSink {
 public:
  void emit(const Diagnostic& /*diagnostic*/) override {}
};

/// Returns the first diagnostic with severity `error`, if any.
[[nodiscard]] const Diagnostic* first_error(const std::vector<Diagnostic>& diagnostics) noexcept;

}  // namespace hefesto::diag
