// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Read-only vendor views (see hefesto/gds/read/vendor.hpp). Ported from
// Magic 8.3.694 calma/CalmaRead.c (calmaParseUnits' read-only check,
// calmaStampLookup, calmaHashStamps, CalmaVendorStamp),
// calma/CalmaWrite.c (CalmaOpenVendorGDS) and database/DBprop.c
// (DBPropGetDouble); Magic is (c) the Regents of the University of
// California, distributed under its permissive license
// (THIRD_PARTY_NOTICES.md). Owner: GDSR.

#include "hefesto/gds/read/vendor.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <vector>

#include "hefesto/gds/stream/io.hpp"

namespace hefesto::gds {

namespace {

/// sscanf("%lld"): optional spaces and sign, then digits; 0 without digits.
std::int64_t leading_integer(std::string_view text) {
  std::size_t i = 0;
  while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])) != 0) {
    ++i;
  }
  const bool negative = i < text.size() && text[i] == '-';
  if (i < text.size() && (text[i] == '-' || text[i] == '+')) {
    ++i;
  }
  std::uint64_t value = 0;
  for (; i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])) != 0; ++i) {
    value = value * 10 + static_cast<std::uint64_t>(text[i] - '0');  // wraps on overflow
  }
  const auto magnitude = static_cast<std::int64_t>(value);
  return negative ? -magnitude : magnitude;
}

std::string format_g(double value) {
  std::array<char, 64> buffer{};
  std::snprintf(buffer.data(), buffer.size(), "%g", value);
  return buffer.data();
}

/// The whole decompressed contents of `source`.
diag::Outcome<std::vector<std::byte>> read_all(ByteSource& source) {
  std::vector<std::byte> out;
  std::array<std::byte, 64 * 1024> chunk{};
  for (;;) {
    auto got = source.read(chunk);
    if (!got.ok()) {
      return got.error();
    }
    if (got.value() == 0) {
      return out;
    }
    out.insert(out.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(got.value()));
  }
}

}  // namespace

std::optional<diag::Diagnostic> readonly_units_error(double mpd, bool angstroms) {
  const double scaled = 0.5 + mpd * (angstroms ? 1e12 : 1e11);
  // (int)(...) != 100; a value outside int's range never equals 100.
  if (scaled >= 100.0 && scaled < 101.0) {
    return std::nullopt;
  }
  return diag::make("GDS-READ-003", "Incompatible scale factor of " + format_g(mpd) + ", must be " +
                                        (angstroms ? "1e-10." : "1e-9."));
}

std::string gds_file_value(std::string_view path, const cell::PathVariables& vars) {
  return cell::substitute_path(path, path, vars, /*magic_compat=*/false);
}

std::optional<std::int64_t> property_integer(const db::CellDef& def, std::string_view name) {
  const db::PropertyValue* value = def.properties().find(name);
  if (value == nullptr) {
    return std::nullopt;
  }
  if (const auto* doubles = std::get_if<db::DoubleList>(value)) {
    return doubles->values.empty() ? 0 : doubles->values.front();
  }
  if (const auto* text = std::get_if<std::string>(value)) {
    return leading_integer(*text);
  }
  return std::nullopt;
}

std::uint32_t cell_content_stamp(const StampTable& table, const db::CellDef& def) {
  if (!property_integer(def, "GDS_END")) {
    return 0;  // dumped from the whole file: no single structure to hash
  }
  const auto start = property_integer(def, "GDS_START");
  if (!start || *start < 0) {
    return 0;
  }
  return lookup_stamp(table, static_cast<std::uint64_t>(*start));
}

void assign_content_stamps(const StampTable& table, std::span<db::CellDef* const> cells) {
  for (db::CellDef* def : cells) {
    if (def == nullptr || !def->flags().vendor_gds || def->flags().fixed_timestamp) {
      continue;
    }
    const std::uint32_t stamp = cell_content_stamp(table, *def);
    if (stamp == 0) {
      continue;
    }
    def->set_timestamp(stamp);
    def->flags().needs_new_timestamp = false;
  }
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

const StampTable* VendorStamper::table_for(const platform::Path& path) {
  const auto [it, inserted] = cache_.try_emplace(path.generic_string());
  if (inserted) {
    auto source = open_source(ctx_.fs, path);
    if (source.ok()) {
      auto bytes = read_all(*source.value());
      if (bytes.ok()) {
        it->second = content_stamps(bytes.value());
      }
    }
  }
  return it->second ? &*it->second : nullptr;
}

std::uint32_t VendorStamper::stamp(const db::CellDef& def) {
  const db::PropertyValue* value = def.properties().find("GDS_FILE");
  const auto* name = value != nullptr ? std::get_if<std::string>(value) : nullptr;
  if (name == nullptr || name->empty()) {
    return 0;
  }
  const auto path = find(def, *name);
  if (!path) {
    return 0;
  }
  const StampTable* table = table_for(*path);
  return table != nullptr ? cell_content_stamp(*table, def) : 0;
}

}  // namespace hefesto::gds
