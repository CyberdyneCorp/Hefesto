// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The built-in window clients (ui-command-dispatch "Window client model").
// Owner: DISPATCH.
// (Magic ref: windows/windMain.c:224-341, 396-420)

#include "hefesto/ui/client.hpp"

#include <array>
#include <string>

#include "hefesto/ui/diag_codes.hpp"

namespace hefesto::ui {

std::span<const ClientInfo> builtin_clients() noexcept {
  static constexpr std::array kClients = {
      ClientInfo{ClientId::window, "*window", true, false},
      ClientInfo{ClientId::layout, "layout", false, false},
      ClientInfo{ClientId::color, "color", false, false},
      ClientInfo{ClientId::netlist, "netlist", false, true},
      ClientInfo{ClientId::wind3d, "wind3d", false, true},
  };
  return kClients;
}

const ClientInfo* find_client_exact(std::string_view name) noexcept {
  for (const ClientInfo& c : builtin_clients()) {
    if (c.name == name) {
      return &c;
    }
  }
  return nullptr;
}

diag::Outcome<const ClientInfo*> find_client(std::string_view prefix) {
  const ClientInfo* found = nullptr;
  int matches = 0;
  for (const ClientInfo& c : builtin_clients()) {
    if (c.hidden || !c.name.starts_with(prefix)) {
      continue;
    }
    if (c.name == prefix) {
      return &c;
    }
    found = &c;
    ++matches;
  }
  if (matches == 1) {
    return found;
  }
  const std::string what = matches == 0 ? "is not a valid window type" : "is an ambiguous window type";
  return diag::make(codes::kClient,
                    "\"" + std::string{prefix} + "\" " + what + ". Valid window types are:\n" + window_types_listing());
}

std::string window_types_listing() {
  std::string out;
  for (const ClientInfo& c : builtin_clients()) {
    if (!c.hidden) {
      out += '\t';
      out += c.name;
      out += '\n';
    }
  }
  if (!out.empty()) {
    out.pop_back();
  }
  return out;
}

bool client_available(const ClientInfo& client) noexcept {
  return !client.optional_module;
}

}  // namespace hefesto::ui
