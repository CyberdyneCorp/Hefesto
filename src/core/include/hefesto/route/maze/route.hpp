// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The maze-router entry point (capability router-maze; openspec/changes/
// m10-plot-route/design.md D4, D11, "Area contracts" MZ). Reentrant: every
// call owns its planes, heaps and arena, so separate sessions route in
// parallel (D16).
//
// Owner: MZ.

#pragma once

#include "hefesto/route/maze/types.hpp"

namespace hefesto::route::maze {

/// Routes `request`. Until MZ lands, the result has code `failure` and a
/// CORE-NOTIMPL-010 message.
[[nodiscard]] RouteResult route(const RouteRequest& request);

}  // namespace hefesto::route::maze
