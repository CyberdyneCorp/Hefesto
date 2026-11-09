// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The per-session CELL state (hefesto/cell/session.hpp).

#include "hefesto/cell/session.hpp"

namespace hefesto::cell {

CellSession::CellSession() = default;
CellSession::~CellSession() = default;

FileHost& CellSession::file_host() {
  if (host) {
    return *host;
  }
  if (!local_) {
    local_ = make_local_file_host();
  }
  return *local_;
}

}  // namespace hefesto::cell
