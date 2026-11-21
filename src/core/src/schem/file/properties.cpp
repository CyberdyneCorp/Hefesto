// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (tasks.md 5.2): no attributes are found, and set_token
// returns its input unchanged.
//
// Owner: FORMAT.

#include "hefesto/schem/file/properties.hpp"

namespace hefesto::schem {

PropertyList parse_properties(std::string_view /*props*/) {
  return {};
}

std::optional<std::string> get_token(std::string_view /*props*/, std::string_view /*key*/, ValueForm /*form*/) {
  return std::nullopt;
}

std::string set_token(std::string_view props, std::string_view /*key*/, std::optional<std::string_view> /*value*/) {
  return std::string{props};
}

}  // namespace hefesto::schem
