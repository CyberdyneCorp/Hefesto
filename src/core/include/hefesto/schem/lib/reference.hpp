// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Symbol references to files and back (capability
// schematic-symbols-and-libraries "Symbol references", Q-SCHEM-9,
// Q-SCHEM-30).
//
// Owner: SYMLIB.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/lib/search_path.hpp"

namespace hefesto::schem {

/// How a reference was resolved; the step of "Symbol references".
enum class ResolvedVia : std::uint8_t {
  none,             // empty reference (step 1)
  web,              // `http://` or `https://`, never fetched (step 2)
  absolute,         // starts with `/` (step 3)
  relative,         // `./` or `../` against the document folder (step 5)
  alias,            // a library alias (step 6)
  search_path,      // a search-path folder (step 7)
  document_folder,  // a relative search-path element such as `.` (step 7)
  fallback,         // `<document folder>/<reference>` (step 8)
};

struct ResolvedReference {
  /// The file the reference names; empty for `none` and `web`.
  platform::Path file;
  ResolvedVia via = ResolvedVia::none;
  /// For `relative` and `document_folder`: the schematic folder that took the
  /// place of xschem's start folder (the SCHEM-LIB-001 warning names it).
  platform::Path folder;
};

/// True for references starting with `http://` or `https://`.
[[nodiscard]] bool is_web_reference(std::string_view reference) noexcept;

/// Step 4 of "Symbol references": runs of `/` collapsed, `/./` removed,
/// `dir/../` pairs removed, and a trailing `/` or `/.` removed.
[[nodiscard]] std::string normalize_reference(std::string_view reference);

/// Resolves `reference` written in a schematic whose folder is
/// `document_folder` by steps 1 to 8 of "Symbol references". Whether the
/// result exists is for the caller to check (step 8 may name a missing file).
[[nodiscard]] ResolvedReference resolve_reference(std::string_view reference, const platform::Path& document_folder,
                                                  const SearchPath& search_path, platform::FileSystem& fs);

/// The reference Hefesto writes for a symbol file it chose itself: relative
/// to the first search-path folder that holds it (in its alias form when
/// that folder has an alias), else the bare file name when it is in the
/// document folder, else the absolute path.
[[nodiscard]] std::string shortest_reference(const platform::Path& file, const platform::Path& document_folder,
                                             const SearchPath& search_path);

}  // namespace hefesto::schem
