// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A non-owning reference to a callable (the C++26 std::function_ref): two
// pointers, no allocation, one indirect call. For callbacks that are only
// called during the call that receives them (tile searches, design D16 of
// openspec/changes/m3-drc: the DRC kernel runs millions of plane searches
// per check, and std::function costs an allocation for a lambda that
// captures more than two references with libstdc++).

#pragma once

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace hefesto::util {

template <typename Signature>
class FunctionRef;

template <typename R, typename... Args>
class FunctionRef<R(Args...)> {
 public:
  /// Refers to `f`, which must outlive every call through this reference.
  template <typename F>
    requires(!std::is_same_v<std::remove_cvref_t<F>, FunctionRef> && !std::is_function_v<std::remove_reference_t<F>> &&
             std::is_invocable_r_v<R, std::remove_reference_t<F>&, Args...>)
  FunctionRef(F&& f) noexcept  // NOLINT(google-explicit-constructor): used like std::function
      : object_(const_cast<void*>(static_cast<const void*>(std::addressof(f)))),  // NOLINT: restored below
        call_([](void* object, Args... args) -> R {
          return std::invoke(*static_cast<std::remove_reference_t<F>*>(object), std::forward<Args>(args)...);
        }) {}

  R operator()(Args... args) const { return call_(object_, std::forward<Args>(args)...); }

 private:
  void* object_;
  R (*call_)(void*, Args...);
};

}  // namespace hefesto::util
