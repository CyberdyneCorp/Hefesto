// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private helpers of the OASIS codec (capability oasis-format;
// openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D11): the byte input the
// primitive decoders read from, the error that stops decoding, the
// collector of the error that drops one element, and checked coordinate
// arithmetic. Inside the codec, an error that stops decoding is thrown as
// CodecError and turned into a diag::Outcome at the public entry points,
// so the record decoders stay flat.
//
// Owner: OASFMT (design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/oasis/codec/elements.hpp"
#include "hefesto/oasis/codec/primitives.hpp"

namespace hefesto::oasis::detail {

/// An error that stops decoding.
struct CodecError {
  diag::Diagnostic diagnostic;
};

[[noreturn]] void fail(std::string_view code, std::string text);

/// The bytes the primitive decoders read. get() throws OASIS-CODEC-002 at
/// the end of the input.
class Input {
 public:
  Input() = default;
  Input(const Input&) = delete;
  Input& operator=(const Input&) = delete;
  virtual ~Input() = default;

  virtual std::uint8_t get() = 0;
};

/// An Input over a ByteCursor.
class CursorInput final : public Input {
 public:
  explicit CursorInput(ByteCursor& cursor) : cursor_(&cursor) {}
  std::uint8_t get() override;

 private:
  ByteCursor* cursor_;
};

/// The first error that drops the element being decoded.
struct Drop {
  std::optional<diag::Diagnostic> diagnostic;

  void set(std::string_view code, std::string text);
  [[nodiscard]] bool dropped() const noexcept { return diagnostic.has_value(); }
};

/// a + b, or Drop (OASIS-CODEC-008) when it does not fit in 64 bits.
[[nodiscard]] std::int64_t add(std::int64_t a, std::int64_t b, Drop& drop);
[[nodiscard]] Point64 add(Point64 a, Point64 b, Drop& drop);
/// Drop (OASIS-CODEC-008) unless both components fit in signed 32 bits.
void check_delta(Point64 delta, Drop& drop);
/// `value` as a signed 64-bit length, or Drop (OASIS-CODEC-008).
[[nodiscard]] std::int64_t to_length(std::uint64_t value, Drop& drop);

// --- primitive decoders (throw CodecError) ---------------------------------

[[nodiscard]] std::uint64_t read_unsigned(Input& in);
/// A field the format declares 32-bit (OASIS-CODEC-005 above 2^32 - 1).
[[nodiscard]] std::uint32_t read_u32(Input& in, std::string_view field);
[[nodiscard]] std::int64_t read_signed(Input& in);
[[nodiscard]] double read_real(Input& in);
/// The data of a real whose type code `type` was already read.
[[nodiscard]] double read_real_data(Input& in, std::uint64_t type);
[[nodiscard]] std::string read_string(Input& in);
/// Reads and discards a string (END padding).
void skip_string(Input& in);
[[nodiscard]] Point64 read_delta(Input& in, DeltaForm form);
/// A point list; a count of 0 drops the element, a type above 5 stops, a
/// count above kMaxListEntries drops the element (OASIS-CODEC-030) and
/// comes back `capped` with no vertices.
[[nodiscard]] PointList read_point_list(Input& in, Drop& drop);
/// A repetition; type 0 returns `modal` (undefined: OASIS-CODEC-020). A
/// list type above kMaxListEntries displacements drops the element
/// (OASIS-CODEC-030) and comes back with no offsets (see capped()).
[[nodiscard]] Repetition read_repetition(Input& in, const std::optional<Repetition>& modal, Drop& drop);
/// Whether `repetition` is a list repetition dropped by the entry cap (a
/// list type with no offsets; a decoded one always has at least one).
[[nodiscard]] bool capped(const Repetition& repetition) noexcept;
[[nodiscard]] Interval read_interval(Input& in);

/// The bytes of `value` (the unsigned and signed encodings), for tests of
/// the encoder's choices.
[[nodiscard]] std::size_t unsigned_length(std::uint64_t value) noexcept;

/// The diagnostic text of a string with an illegal byte, or nullopt.
[[nodiscard]] std::optional<diag::Diagnostic> check_string(StringKind kind, std::string_view text);

// --- encoders --------------------------------------------------------------

/// Writes `repetition` (type 1-11); false when its parameters cannot be
/// represented in its type.
[[nodiscard]] bool write_repetition(std::vector<std::byte>& out, const Repetition& repetition);
/// Writes a point list of `type` for `points` (as in Element::points);
/// false when `points` cannot be represented in that type.
[[nodiscard]] bool write_point_list(std::vector<std::byte>& out, std::uint8_t type, std::span<const Point64> points,
                                    bool polygon);
/// The narrowest point-list type for `points` (oasis-write "Boundary
/// translation" rule 2 for a polygon).
[[nodiscard]] std::uint8_t narrowest_point_list(std::span<const Point64> points, bool polygon) noexcept;
/// The vertex a type 0/1 polygon list synthesizes after `last`, the last
/// delta being horizontal when `horizontal`.
[[nodiscard]] Point64 synthesized_vertex(Point64 last, bool horizontal) noexcept;
/// Whether delta i of a type 0 (or 1, when `vertical_first`) list is horizontal.
[[nodiscard]] constexpr bool horizontal_delta(std::uint64_t index, bool vertical_first) noexcept {
  return ((index % 2) == 0) != vertical_first;
}

}  // namespace hefesto::oasis::detail
