// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The messages of an OASIS read (capability oasis-read "Read diagnostics",
// "Post-order reading and flattening options" rule 1): gds-read's counters
// and warning levels (CalmaReadError, CIFReadError, CIFReadWarning, the
// limit of 100 and the redirect file) applied to OASR's messages and to
// those of the GDS reader, which runs at the default level. A GDS reader
// message's byte position, an offset in the GDS-equivalent stream, becomes
// the position of the OASIS record the element came from. Messages about
// the GDS-equivalent stream itself (its release and library name) and the
// "Rewinding input" error are dropped: the pass 1 index replaces the file
// search, so an OASIS read never rewinds.
//
// Owner: OASR (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <algorithm>

#include "internal.hpp"

namespace hefesto::oasis::read_detail {

namespace {

constexpr std::string_view kCountedPrefix = "Error while reading cell \"";
constexpr std::string_view kPositionField = " (byte position ";
constexpr std::string_view kCifError = "CIF file read error: ";
constexpr std::string_view kCifWarning = "CIF file read warning: ";
constexpr std::string_view kErrorLimitText = "Error limit set:  Remaining errors will not be reported.";

/// Messages about the GDS-equivalent stream rather than the OASIS file.
bool stream_only(const diag::Diagnostic& d) {
  if (d.code == "GDS-READ-004") {
    return true;  // "Library written using GDS-II Release 6.0"
  }
  if (d.code == "GDS-READ-008" && d.text.starts_with("Library name")) {
    return true;
  }
  return d.text.find("Rewinding input.") != std::string::npos;
}

/// The GDS offset in a counted message replaced by its OASIS position.
void map_position(diag::Diagnostic& d, const ReadMessages::PositionMap& positions) {
  const std::size_t field = d.text.find(kPositionField);
  if (field == std::string::npos || positions.empty()) {
    return;
  }
  const std::size_t from = field + kPositionField.size();
  const std::size_t to = d.text.find(')', from);
  if (to == std::string::npos) {
    return;
  }
  std::uint64_t offset = 0;
  for (std::size_t i = from; i < to; ++i) {
    if (d.text[i] < '0' || d.text[i] > '9') {
      return;
    }
    offset = offset * 10 + static_cast<std::uint64_t>(d.text[i] - '0');
  }
  auto it = std::upper_bound(positions.begin(), positions.end(), offset,
                             [](std::uint64_t v, const auto& entry) { return v < entry.first; });
  if (it != positions.begin()) {
    --it;
  }
  d.text.replace(from, to - from, position_text(it->second));
}

}  // namespace

bool ReadMessages::admit(std::int64_t count, std::string_view limit_text) {
  if (warnings_.level == mask::WarningLevel::none) {
    return false;
  }
  if (count < 100 || warnings_.level != mask::WarningLevel::limit) {
    return true;
  }
  if (count == 100) {
    printed_.push_back(diag::make("GDS-READ-007", std::string{limit_text}));
  }
  return false;
}

void ReadMessages::emit(diag::Diagnostic d) {
  if (warnings_.level == mask::WarningLevel::redirect) {
    if (!warnings_.redirect_file.empty()) {
      redirected_ += d.text;
      if (!d.no_newline) {
        redirected_ += "\n";
      }
    }
    return;
  }
  printed_.push_back(std::move(d));
}

void ReadMessages::route(Kind kind, diag::Diagnostic d) {
  switch (kind) {
    case Kind::counted:
      if (admit(++calma_errors_, kErrorLimitText)) {
        emit(std::move(d));
      }
      return;
    case Kind::detail:
      if (warnings_.level != mask::WarningLevel::none &&
          !(warnings_.level == mask::WarningLevel::limit && calma_errors_ >= 100)) {
        emit(std::move(d));
      }
      return;
    case Kind::cif_error:
      if (admit(++cif_errors_, kErrorLimitText)) {
        printed_.push_back(std::move(d));
      }
      return;
    case Kind::cif_warning:
      if (admit(++cif_warnings_, "Warning limit set:  Remaining warnings will not be reported.")) {
        printed_.push_back(std::move(d));
      }
      return;
    case Kind::print:
      printed_.push_back(std::move(d));
      return;
  }
}

void ReadMessages::counted(std::string_view cell, const StreamPosition& position, diag::Diagnostic d) {
  d.text = std::string{kCountedPrefix} + std::string{cell} + "\"" + std::string{kPositionField} +
           position_text(position) + "): " + d.text;
  route(Kind::counted, std::move(d));
}

void ReadMessages::print(diag::Diagnostic d) {
  route(Kind::print, std::move(d));
}

void ReadMessages::forward(std::vector<diag::Diagnostic> gds_messages, const PositionMap& positions) {
  for (diag::Diagnostic& d : gds_messages) {
    if (stream_only(d)) {
      continue;
    }
    if (d.text.starts_with(kCountedPrefix)) {
      map_position(d, positions);
      route(Kind::counted, std::move(d));
    } else if (d.code == "GDS-READ-006" && d.text.starts_with("    Expected ")) {
      route(Kind::detail, std::move(d));
    } else if (d.text.starts_with(kCifError)) {
      route(Kind::cif_error, std::move(d));
    } else if (d.text.starts_with(kCifWarning)) {
      route(Kind::cif_warning, std::move(d));
    } else {
      route(Kind::print, std::move(d));
    }
  }
}

}  // namespace hefesto::oasis::read_detail
