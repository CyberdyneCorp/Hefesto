// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The SPICE/CDL reader of the LVS engine (lvs-netlist-input "SPICE lexical
// structure" through "Library and black-box reads", "Input limits").
// One SpiceReader reads one top file and its includes into one file number.
// Private to lvs/read.
//
// Owner: LVSREAD.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/lvs/net/netlist_db.hpp"
#include "hefesto/lvs/read/reader.hpp"

namespace hefesto::lvs::spice {

using Tokens = std::vector<std::string>;

/// .INCLUDE reads in one read_netlist call: a guard against an include tree
/// that multiplies (each file including the next one twice) below the
/// depth limit, which would otherwise read 2^depth files.
inline constexpr std::size_t kMaxIncludes = 65536;

/// The bytes all .INCLUDE reads of one read_netlist call may lex, as a
/// multiple of ReadLimits::logical_line (1 GiB with the defaults): a few
/// large files included many times would otherwise lex terabytes below
/// kMaxIncludes.
inline constexpr std::uint64_t kIncludeBytesPerLogicalLine = 64;

/// The file being read: its name (as diagnostics and includes see it), the
/// current line and its warning count.
struct FileState {
  std::string name;
  std::uint32_t line = 0;
  std::uint32_t warnings = 0;
  bool control = false;  // inside .CONTROL ... .ENDC
};

class SpiceReader {
 public:
  SpiceReader(NetlistDb& db, const ReadOptions& options, FileNumber file, std::string top_name);

  /// Reads the top file's bytes, closes what is still open and returns the
  /// file's top cell.
  diag::Outcome<CellId> read_top(std::string_view bytes);

  // The card handlers (spice_reader.cpp, spice_cards.cpp).
  diag::Status read_text(FileState& file, std::string_view bytes);
  diag::Status process(FileState& file, const Tokens& tokens);
  diag::Status dot_card(FileState& file, const Tokens& tokens);
  diag::Status subckt(FileState& file, const Tokens& tokens);
  void ends(FileState& file);
  void global(const Tokens& tokens);
  void param(const Tokens& tokens);
  void model(const Tokens& tokens);
  diag::Status include(FileState& file, const Tokens& tokens);
  diag::Status primitive(FileState& file, const Tokens& tokens);
  diag::Status call(FileState& file, const Tokens& tokens);
  diag::Status add_call(FileState& file, const std::string& name, const std::string& class_name,
                        const std::vector<std::string>& nodes, PropertyRecord parameters);

  // Shared helpers.
  void warn(FileState& file, std::string_view code, std::string text);
  void note(const FileState& file, std::string_view code, std::string text);
  [[nodiscard]] diag::Diagnostic error(const FileState& file, std::string_view code, std::string text) const;
  diag::Status check_tokens(const FileState& file, const Tokens& tokens) const;
  /// The cell elements go into: the innermost open definition, else the
  /// cell named after the file (opened on first use).
  CellId current_cell();
  [[nodiscard]] bool inside_definition() const noexcept { return !open_.empty(); }
  /// Counts one more device against the devices limit.
  diag::Status add_device(const FileState& file);
  [[nodiscard]] Cell& cell(CellId id) { return db_.cells.at(id); }
  [[nodiscard]] NetlistDb& db() noexcept { return db_; }
  [[nodiscard]] const ReadOptions& options() const noexcept { return options_; }
  [[nodiscard]] FileNumber file_number() const noexcept { return file_; }
  [[nodiscard]] bool case_insensitive() const noexcept { return case_insensitive_; }
  /// A new cell of this file with the reader's case sensitivity.
  CellId new_cell(std::string name, DeviceClass device_class);
  /// The innermost open definition, if any.
  [[nodiscard]] std::optional<CellId> defining() const noexcept;

 private:
  void close_definition(CellId id);
  void finish_ports(Cell& cell);
  void finish_instances();
  std::optional<CellId> reuse_placeholder(FileState& file, std::string_view name, diag::Status& status);
  void rename_duplicate(FileState& file, CellId existing);
  diag::Status skip_library_body(FileState& file, const Tokens& tokens);
  [[nodiscard]] CellId top_cell();

  NetlistDb& db_;
  const ReadOptions& options_;
  FileNumber file_;
  std::string top_name_;
  bool case_insensitive_ = true;
  std::optional<CellId> file_cell_;
  std::vector<CellId> open_;  // open definitions, innermost last
  std::optional<CellId> last_top_definition_;
  std::size_t include_depth_ = 1;
  std::size_t includes_ = 0;  // .INCLUDE reads so far (kMaxIncludes)
  std::uint64_t include_bytes_ = 0;  // bytes of those reads (kIncludeBytesPerLogicalLine)
  std::size_t library_skip_ = 0;  // nesting of a skipped .SUBCKT body (readlib)
  std::uint64_t devices_ = 0;
  CellId first_new_cell_ = 0;
};

/// Card names and keywords compare case-insensitively.
[[nodiscard]] bool keyword_equal(std::string_view token, std::string_view keyword) noexcept;

}  // namespace hefesto::lvs::spice
