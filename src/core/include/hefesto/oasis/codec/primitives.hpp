// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The OASIS primitive encodings (capability oasis-format "Unsigned and
// signed integers", "Real numbers", "Strings and character sets", "Delta
// encodings", "Point lists", "Repetitions", "Intervals";
// openspec/changes/m6-lef-def-oasis/design.md D11), over byte spans. The
// decoder uses the same code over its record stream; these entry points
// serve the writer's transcoders, the tests and the fuzz targets.
//
// Decoding never allocates from a count read from the input: lists grow
// one decoded entry at a time, so their size is bounded by the input.
//
// Owner: OASFMT (design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/oasis/codec/elements.hpp"

namespace hefesto::oasis {

/// A read position in a byte span.
struct ByteCursor {
  std::span<const std::byte> bytes;
  std::size_t offset = 0;
};

enum class StringKind : std::uint8_t { a_string, b_string, n_string };

/// Whether `text` is legal for `kind`: a-string 0x20-0x7E, n-string
/// 0x21-0x7E and not empty, b-string anything.
[[nodiscard]] bool legal_string(StringKind kind, std::string_view text) noexcept;

/// The delta forms of oasis-format "Delta encodings"; a 1-delta lies along
/// the axis its context implies.
enum class DeltaForm : std::uint8_t { one_horizontal, one_vertical, two, three, g };

/// The most entries a decoded point list, or a type 4, 5, 6, 7, 10 or 11
/// repetition, may list (Q-OASIS-14, security review M6-S3). Above it the
/// entries are read and discarded and the element is dropped
/// (OASIS-CODEC-030), in both modes.
inline constexpr std::uint64_t kMaxListEntries = std::uint64_t{1} << 24;

/// A decoded point list: its type and the vertices P1..Pn relative to the
/// implicit P0 = (0, 0) (no polygon closure). `capped`: the list had more
/// than kMaxListEntries points, so `vertices` is empty.
struct PointList {
  std::uint8_t type = 0;
  std::vector<Point64> vertices;
  bool capped = false;

  friend bool operator==(const PointList&, const PointList&) = default;
};

/// A decoded interval: the closed range low..high of unsigned 32-bit
/// values; empty when low > high.
struct Interval {
  std::uint8_t type = 0;
  std::uint32_t low = 0;
  std::uint32_t high = 0xffffffffU;

  [[nodiscard]] bool empty() const noexcept { return low > high; }
  friend bool operator==(const Interval&, const Interval&) = default;
};

// --- decoding (errors: the OASIS-CODEC codes of the requirement) ---------

[[nodiscard]] diag::Outcome<std::uint64_t> read_unsigned(ByteCursor& cursor);
[[nodiscard]] diag::Outcome<std::int64_t> read_signed(ByteCursor& cursor);
[[nodiscard]] diag::Outcome<double> read_real(ByteCursor& cursor);
/// The string's bytes, whatever its kind (check them with legal_string).
[[nodiscard]] diag::Outcome<std::string> read_string(ByteCursor& cursor);
/// A delta; a component outside the signed 32-bit range is an error
/// (OASIS-CODEC-008).
[[nodiscard]] diag::Outcome<Point64> read_delta(ByteCursor& cursor, DeltaForm form);
[[nodiscard]] diag::Outcome<PointList> read_point_list(ByteCursor& cursor);
/// A repetition of type 1-11 (type 0 is an error here: no modal state).
[[nodiscard]] diag::Outcome<Repetition> read_repetition(ByteCursor& cursor);
[[nodiscard]] diag::Outcome<Interval> read_interval(ByteCursor& cursor);

// --- encoding (always the minimal form) ------------------------------------

void write_unsigned(std::vector<std::byte>& out, std::uint64_t value);
/// `value` must not be INT64_MIN.
void write_signed(std::vector<std::byte>& out, std::int64_t value);
/// The narrowest form of oasis-format "Real numbers" rule 4.
void write_real(std::vector<std::byte>& out, double value);
void write_string(std::vector<std::byte>& out, std::string_view text);
/// A delta in `form`; it must be representable in that form (axis-parallel
/// for 1- and 2-deltas, octangular for 3-deltas) with |component| < 2^59.
void write_delta(std::vector<std::byte>& out, Point64 delta, DeltaForm form);

}  // namespace hefesto::oasis
