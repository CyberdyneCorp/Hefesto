// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Read-only OASIS vendor views (see hefesto/oasis/read/vendor.hpp) and the
// OASIS file lookup of `oasis read` (hefesto/oasis/read/find_file.hpp;
// capability oasis-read "Read command and file lookup").
//
// Owner: OASR (openspec/changes/m6-lef-def-oasis/design.md D2).

#include "hefesto/oasis/read/vendor.hpp"

#include <array>

#include "hefesto/gds/stream/io.hpp"
#include "hefesto/oasis/read/find_file.hpp"
#include "hefesto/oasis/read/stamps.hpp"

namespace hefesto::oasis {

namespace {

constexpr std::array<std::string_view, 4> kExtensions{".oas", ".oas.gz", ".oasis", ""};

const std::string* string_property(const db::CellDef& def, std::string_view name) {
  const db::PropertyValue* value = def.properties().find(name);
  return value != nullptr ? std::get_if<std::string>(value) : nullptr;
}

}  // namespace

std::optional<cell::FoundFile> find_oasis_file(cell::LookupContext& ctx, std::string_view name) {
  for (const std::string_view extension : kExtensions) {
    const std::string candidate = std::string{name} + std::string{extension};
    auto found = cell::search_file(ctx, candidate, ctx.paths.path, nullptr);
    // As find_gds_file: a compressed counterpart is found only through the
    // ".oas.gz" extension.
    if (found && found->path.filename() == platform::Path{candidate}.filename()) {
      return found;
    }
  }
  return std::nullopt;
}

bool is_oasis_vendor(const db::CellDef& def) {
  return string_property(def, kOasisFile) != nullptr;
}

std::uint32_t lookup_stamp(const gds::StampTable& table, std::string_view name) {
  for (const gds::StructureStamp& s : table) {
    if (s.name == name) {
      return s.stamp;
    }
  }
  return 0;
}

std::optional<platform::Path> VendorStamper::find(const db::CellDef& def, const std::string& name) {
  if (auto found = cell::search_file(ctx_, name, ctx_.paths.path, &ctx_.paths.library)) {
    return found->path;
  }
  const std::string mag = def.file_path().generic_string();
  const auto slash = mag.rfind('/');
  if (name.front() == '/' || name.front() == '~' || name.front() == '$' || slash == std::string::npos) {
    return std::nullopt;
  }
  if (auto found = cell::open_candidate(ctx_, mag.substr(0, slash + 1) + name)) {
    return found->path;
  }
  return std::nullopt;
}

const gds::StampTable* VendorStamper::table_for(const platform::Path& path) {
  const auto [it, inserted] = cache_.try_emplace(path.generic_string());
  if (inserted) {
    auto source = gds::open_source(ctx_.fs, path);
    if (source.ok()) {
      it->second = oasis::content_stamps(*source.value(), magic_compat_);
    }
  }
  return it->second ? &*it->second : nullptr;
}

std::uint32_t VendorStamper::stamp(const db::CellDef& def) {
  const std::string* file = string_property(def, kOasisFile);
  const std::string* cell = string_property(def, kOasisCell);
  if (file == nullptr || file->empty() || cell == nullptr) {
    return 0;
  }
  const auto path = find(def, *file);
  if (!path) {
    return 0;
  }
  const gds::StampTable* table = table_for(*path);
  return table != nullptr ? lookup_stamp(*table, *cell) : 0;
}

}  // namespace hefesto::oasis
