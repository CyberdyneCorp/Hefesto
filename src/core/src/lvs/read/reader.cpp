// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// read_netlist: limits, file numbers and re-reads ("Format selection"),
// path or in-memory bytes ("In-memory netlist sources"), then the reader of
// the format. SPICE and CDL are read by lvs/read/spice_reader; Verilog,
// .sim and .ext fail with CORE-NOTIMPL-009 until tasks.md 6.3.
//
// Owner: LVSREAD.

#include "hefesto/lvs/read/reader.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/lvs/not_implemented.hpp"
#include "hefesto/lvs/read/codes.hpp"
#include "hefesto/platform/file_system.hpp"
#include "lvs/read/spice_reader.hpp"

namespace hefesto::lvs {

namespace {

constexpr ReadLimits kDefaultLimits{};

bool within(std::uint64_t value, std::uint64_t default_value) noexcept {
  return value >= 1 && value <= 2 * default_value;
}

/// A file already read under `name`: its top cell.
std::optional<CellId> loaded(const NetlistDb& db, std::string_view name) {
  for (const auto& file : db.files) {
    if (file.name == name) {
      return file.top;
    }
  }
  for (std::size_t i = 0; i < db.cells.size(); ++i) {
    if (db.cells[i].name == name) {
      return static_cast<CellId>(i);
    }
  }
  return std::nullopt;
}

diag::Outcome<std::string> source_bytes(const Source& source, const ReadOptions& options) {
  if (source.bytes) {
    return *source.bytes;
  }
  if (options.files == nullptr) {
    return diag::make(read_codes::kPathUnreadable, "no file service to read '" + source.name + "'");
  }
  if (!options.files->exists(source.name)) {
    return diag::make(read_codes::kFileNotFound, "ReadNetlist: unable to find file '" + source.name + "'");
  }
  return options.files->read(source.name);
}

/// What a failed read must leave as it found: the cells, the cells of its
/// file number that existed before (a read into an existing number may
/// replace placeholders or rename duplicates), the .PARAM table of that
/// number and the next file number.
struct ReadSnapshot {
  std::size_t cells = 0;
  FileNumber next_file = 0;
  FileNumber number = 0;
  std::vector<std::pair<CellId, Cell>> file_cells;
  std::optional<ParameterTable> parameters;
};

ReadSnapshot snapshot(const NetlistDb& db, FileNumber number) {
  ReadSnapshot out{db.cells.size(), db.next_file, number, {}, std::nullopt};
  for (std::size_t id = 0; id < db.cells.size(); ++id) {
    if (db.cells[id].file == number) {
      out.file_cells.emplace_back(static_cast<CellId>(id), db.cells[id]);
    }
  }
  if (const auto it = db.parameters.find(number); it != db.parameters.end()) {
    out.parameters = it->second;
  }
  return out;
}

void roll_back(NetlistDb& db, ReadSnapshot saved) {
  for (auto& [id, cell] : saved.file_cells) {
    db.cells[id] = std::move(cell);
  }
  db.truncate_cells(saved.cells);
  db.next_file = saved.next_file;
  if (saved.parameters) {
    db.parameters[saved.number] = std::move(*saved.parameters);
  } else {
    db.parameters.erase(saved.number);
  }
}

}  // namespace

diag::Status validate_limits(const ReadLimits& limits) {
  const bool ok = within(limits.include_depth, kDefaultLimits.include_depth) &&
                  within(limits.logical_line, kDefaultLimits.logical_line) &&
                  within(limits.name_length, kDefaultLimits.name_length) &&
                  within(limits.devices, kDefaultLimits.devices);
  if (!ok) {
    return diag::make(read_codes::kBadLimit,
                      "a read limit must be at least 1 and at most twice its default (include depth 64, logical "
                      "line 16 MiB, name length 4 KiB, devices 50 million)");
  }
  return diag::Done{};
}

diag::Outcome<CellId> read_netlist(NetlistDb& db, const Source& source, Format format, const ReadOptions& options) {
  if (auto status = validate_limits(options.limits); !status) {
    return status.error();
  }
  if (format != Format::spice && format != Format::cdl) {
    return not_implemented("lvs::read_netlist (Verilog, .sim and .ext input, tasks.md 6.3)");
  }
  if (options.file_number && *options.file_number < 0) {
    return diag::make(read_codes::kFileNumber, "File number must be zero or positive.");
  }
  if (const auto existing = loaded(db, source.name)) {
    if (options.file_number && *options.file_number != db.cells[*existing].file) {
      return diag::make(read_codes::kFileNumber, "File is already loaded as a different file number.");
    }
    return *existing;
  }
  auto bytes = source_bytes(source, options);
  if (!bytes) {
    return bytes.error();
  }
  const FileNumber number = options.file_number.value_or(db.next_file);
  ReadSnapshot saved = snapshot(db, number);
  db.next_file = std::max(db.next_file, number + 1);
  spice::SpiceReader reader{db, options, number, source.name};
  auto top = reader.read_top(bytes.value());
  if (top) {
    db.files.push_back(LoadedFile{source.name, number, top.value()});
  } else {
    roll_back(db, std::move(saved));  // a failed read leaves nothing a later read could return
  }
  return top;
}

}  // namespace hefesto::lvs
