// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/diag/sink.hpp"

#include <algorithm>
#include <utility>

namespace hefesto::diag {

void CollectingSink::emit(const Diagnostic& diagnostic) {
  const std::scoped_lock lock{mutex_};
  diagnostics_.push_back(diagnostic);
}

std::vector<Diagnostic> CollectingSink::take() {
  const std::scoped_lock lock{mutex_};
  return std::exchange(diagnostics_, {});
}

std::vector<Diagnostic> CollectingSink::snapshot() const {
  const std::scoped_lock lock{mutex_};
  return diagnostics_;
}

bool CollectingSink::has_errors() const {
  const std::scoped_lock lock{mutex_};
  return first_error(diagnostics_) != nullptr;
}

const Diagnostic* first_error(const std::vector<Diagnostic>& diagnostics) noexcept {
  const auto it = std::find_if(diagnostics.begin(), diagnostics.end(),
                               [](const Diagnostic& d) { return d.severity == Severity::error; });
  return it == diagnostics.end() ? nullptr : &*it;
}

}  // namespace hefesto::diag
