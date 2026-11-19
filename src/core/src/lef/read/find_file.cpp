// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF file lookup (capability lef-read "lef read side effects"; Q-LEFDEF-24
// platform): lefFileOpen for reading (lef/lefWrite.c:150-235), each try
// through PaOpen over the cell search path and the library path
// (cell::search_file).
//
// Owner: LEFR (design D2).

#include "hefesto/lef/read/find_file.hpp"

#include <string>

namespace hefesto::lef {

namespace {

constexpr std::string_view kSuffix = ".lef";

std::optional<cell::FoundFile> search(cell::LookupContext& ctx, const std::string& file) {
  return cell::search_file(ctx, file, ctx.paths.path, &ctx.paths.library);
}

}  // namespace

std::string lef_candidate(std::string_view name) {
  const std::size_t slash = name.rfind('/');
  const std::size_t base = slash == std::string_view::npos ? 0 : slash + 1;
  const std::size_t dot = name.rfind('.');
  if (dot == std::string_view::npos || dot < base) {
    return std::string{name} + std::string{kSuffix};
  }
  if (name.substr(dot) == kSuffix) {
    return std::string{name};
  }
  return std::string{name.substr(0, dot)} + std::string{kSuffix};
}

std::optional<cell::FoundFile> find_lef_file(cell::LookupContext& ctx, std::string_view name) {
  const std::size_t slash = name.rfind('/');
  const std::size_t base = slash == std::string_view::npos ? 0 : slash + 1;
  const std::size_t dot = name.rfind('.');
  if (dot != std::string_view::npos && dot >= base && name.substr(dot) != kSuffix) {
    // An unusual extension (".tlef") is tried as given first.
    if (auto found = search(ctx, std::string{name})) {
      return found;
    }
  }
  return search(ctx, lef_candidate(name));
}

}  // namespace hefesto::lef
