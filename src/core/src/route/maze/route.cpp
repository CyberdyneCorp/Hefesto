// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub (m10-plot-route tasks.md 3.1): route answers `failure`
// with one CORE-NOTIMPL-010 message until MZ (section 10) lands it.
//
// Owner: MZ.

#include "hefesto/route/maze/route.hpp"

#include "hefesto/route/not_implemented.hpp"

namespace hefesto::route::maze {

RouteResult route(const RouteRequest& /*request*/) {
  RouteResult result;
  result.code = ResultCode::failure;
  result.messages.push_back(not_implemented("route::maze::route"));
  return result;
}

}  // namespace hefesto::route::maze
