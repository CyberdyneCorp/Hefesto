// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Window clients (capability ui-command-dispatch, requirement "Window client
// model"; design.md D7). Each client has a unique name and its own command
// table in cmd::CommandRegistry; the dispatcher looks a command up in the
// global (`*window`) table first, then in the client of the window the
// command is sent to, with Magic's unique-prefix and exact-match rules.
// Client names typed by users match as any unique prefix, case-sensitively;
// hidden clients (`*window`) are never listed nor matched.
// (Magic ref: windows/windMain.c:224-341, windows/windMain.c:396-420)
//
// Owner: DISPATCH (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "hefesto/diag/outcome.hpp"

namespace hefesto::ui {

/// The built-in clients.
enum class ClientId : std::uint8_t { window, layout, color, netlist, wind3d };

struct ClientInfo {
  ClientId id = ClientId::layout;
  std::string_view name;
  /// Names starting with `*` are never listed nor opened by users.
  bool hidden = false;
  /// An optional module: reports unavailability until its milestone
  /// (`netlist` M12, `wind3d` M10).
  bool optional_module = false;
};

/// `*window`, `layout`, `color`, `netlist`, `wind3d`, in that order.
[[nodiscard]] std::span<const ClientInfo> builtin_clients() noexcept;

/// The client with exactly this name (hidden ones included); nullptr if none.
[[nodiscard]] const ClientInfo* find_client_exact(std::string_view name) noexcept;

/// A user-typed client name: any unique prefix of a non-hidden client name,
/// case-sensitive. Fails with UI-CLIENT-001 (unknown or ambiguous), the
/// message followed by window_types_listing().
[[nodiscard]] diag::Outcome<const ClientInfo*> find_client(std::string_view prefix);

/// Every non-hidden client name, one per line, each preceded by a tab
/// (the list printed in usage messages such as `specialopen`'s).
[[nodiscard]] std::string window_types_listing();

/// True for clients whose module is part of this build (every built-in
/// client except the optional modules `netlist` and `wind3d`).
[[nodiscard]] bool client_available(const ClientInfo& client) noexcept;

}  // namespace hefesto::ui
