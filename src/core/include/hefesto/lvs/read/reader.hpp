// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LVS netlist readers (openspec/changes/m9-lvs-simulation design.md D6,
// D18, "Area contracts" LVSREAD; capability lvs-netlist-input). SPICE and
// CDL, structural Verilog, `.sim` and legacy `.ext` are read into a
// NetlistDb; includes resolve through the platform file service.
//
// M9 round 1 implements SPICE and CDL (tasks.md 6.1, 6.2). Verilog, `.sim`
// and `.ext` (task 6.3) still fail with CORE-NOTIMPL-009.
//
// Owner: LVSREAD.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/lvs/net/netlist_db.hpp"
#include "hefesto/lvs/read/source.hpp"

namespace hefesto::diag {
class DiagnosticSink;
}

namespace hefesto::platform {
class FileSystem;
}

namespace hefesto::lvs {

/// The reader caps of design D18 (Q-LVS-36): each a setting that may be
/// lowered, never raised past twice the default. They apply in both
/// netgen-compat modes.
struct ReadLimits {
  std::size_t include_depth = 64;                    // files open at once, the top file included
  std::size_t logical_line = std::size_t{16} << 20;  // 16 MiB
  std::size_t name_length = 4096;                    // 4 KiB
  std::uint64_t devices = kMaxDevicesPerRun;         // instances in the database
};

/// LVS-READ-022 when a limit is zero or above twice its default.
[[nodiscard]] diag::Status validate_limits(const ReadLimits& limits);

/// An `ignore` entry of the setup file (lvs-setup-file): calls of `name`
/// are dropped (`class`), or dropped when all their connections are one net
/// (`shorted`).
struct IgnoredClass {
  enum class Kind : std::uint8_t { class_, shorted };
  std::string name;
  Kind kind = Kind::class_;
};

struct ReadOptions {
  bool netgen_compat = true;  // project setting netgen-compat (design D5)
  ReadLimits limits;
  platform::FileSystem* files = nullptr;  // paths and includes; required for a path Source
  /// Warnings and notes of the read (the messages netgen prints); nullptr
  /// drops them. The error that stops a read is the returned Outcome's.
  diag::DiagnosticSink* sink = nullptr;
  /// readnet's explicit file number; nullopt takes the next one.
  std::optional<FileNumber> file_number;
  /// readlib: every .SUBCKT defines a module, its body is skipped and
  /// element cards outside definitions are ignored.
  bool library = false;
  /// `model blackbox on` / `lvs -blackbox`: a subcircuit closed with no
  /// instances becomes a module.
  bool auto_blackbox = false;
  /// Skip C and R cards.
  bool ignore_rc = false;
  std::vector<IgnoredClass> ignore;
  /// Expands a leading `~` (empty user) or `~user` of an .INCLUDE path
  /// (Q-LVS-14): desktop hosts return the home directory, iPadOS the
  /// Hefesto documents root for `~` and nullopt for `~user`. Unset or
  /// nullopt: LVS-READ-024.
  std::function<std::optional<std::string>(std::string_view user)> home_directory;
};

/// Reads `source` as `format` into `db`. Returns the file's top cell: the
/// cell named after the file when the file has elements outside
/// definitions, else the last top-level .SUBCKT, else the (empty) cell
/// named after the file. A file whose cell already exists is not read
/// again; its cell is returned. The cell's `file` is the file number.
[[nodiscard]] diag::Outcome<CellId> read_netlist(NetlistDb& db, const Source& source, Format format,
                                                 const ReadOptions& options);

/// The format of `source` ("Format selection") from its name's suffix, or
/// from the first character of its in-memory bytes (`*` SPICE, `|` .sim):
/// an error when it is unknown.
[[nodiscard]] diag::Outcome<Format> detect_format(const Source& source);

/// A path Source and the format of its file after automatic detection.
struct DetectedSource {
  Source source;
  Format format = Format::spice;
};

/// The whole "Automatic detection" of a path: known suffix, then each
/// suffix appended (the first existing file is used), then the file's first
/// character. In-memory sources go through detect_format().
[[nodiscard]] diag::Outcome<DetectedSource> resolve_source(const Source& source, platform::FileSystem& files);

/// readnet's format keyword by unique prefix of `automatic ext extflat sim
/// prm ntk spice verilog netgen actel xilinx` (an exact keyword wins):
/// nullopt for `automatic`, LVS-READ-003 for an unknown or ambiguous word
/// and for the netgen formats Hefesto does not read.
[[nodiscard]] diag::Outcome<std::optional<Format>> parse_format_keyword(std::string_view word);

}  // namespace hefesto::lvs
