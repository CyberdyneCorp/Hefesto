// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Outcome<T>: a value or the error diagnostic that explains why there is none.
// Used by platform services and settings instead of exceptions, so a failure
// (e.g. an unsupported service) is always reported, never a crash.

#pragma once

#include <cassert>
#include <utility>
#include <variant>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::diag {

/// Value type of an Outcome that carries no value.
struct Done {
  friend bool operator==(Done, Done) = default;
};

template <typename T>
class [[nodiscard]] Outcome {
 public:
  Outcome(T value) : state_(std::move(value)) {}            // NOLINT(google-explicit-constructor)
  Outcome(Diagnostic error) : state_(std::move(error)) {}   // NOLINT(google-explicit-constructor)

  [[nodiscard]] bool ok() const noexcept { return std::holds_alternative<T>(state_); }
  explicit operator bool() const noexcept { return ok(); }

  [[nodiscard]] T& value() & {
    assert(ok());
    return std::get<T>(state_);
  }
  [[nodiscard]] const T& value() const& {
    assert(ok());
    return std::get<T>(state_);
  }
  [[nodiscard]] T&& value() && {
    assert(ok());
    return std::get<T>(std::move(state_));
  }
  [[nodiscard]] const Diagnostic& error() const& {
    assert(!ok());
    return std::get<Diagnostic>(state_);
  }

 private:
  std::variant<T, Diagnostic> state_;
};

using Status = Outcome<Done>;

}  // namespace hefesto::diag
