// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GDSII record payload values (capabilities gds-read "Record framing and
// primitive data types", gds-write "Record encoding", "String records and
// character mapping"; openspec/changes/m5-mask-gds-cif/design.md D10):
// big-endian I2/I4 and R8 fields of a record payload, the string value of a
// record (up to the first NUL), and the encoding of a string record payload
// with the character table, the `lower` setting and the output style's
// `string-limit` (Q-MASK-207).
//
// Owner: GDSR (design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/gds/stream/encode.hpp"

namespace hefesto::gds {

/// The I2 at `index` (in I2 units) of `data`; nullopt past its end.
[[nodiscard]] std::optional<std::int16_t> i2_at(std::span<const std::byte> data, std::size_t index);
/// The I4 at `index` (in I4 units) of `data`; nullopt past its end.
[[nodiscard]] std::optional<std::int32_t> i4_at(std::span<const std::byte> data, std::size_t index);
/// The R8 at `index` (in R8 units) of `data`; nullopt past its end.
[[nodiscard]] std::optional<double> r8_at(std::span<const std::byte> data, std::size_t index);
/// Every whole I2 / I4 of `data` (a trailing partial value is dropped).
[[nodiscard]] std::vector<std::int16_t> i2_values(std::span<const std::byte> data);
[[nodiscard]] std::vector<std::int32_t> i4_values(std::span<const std::byte> data);
/// The string value of a payload: its bytes up to the first NUL.
[[nodiscard]] std::string string_value(std::span<const std::byte> data);

/// The output settings a string record depends on.
struct StringOptions {
  bool string_limit = false;  // the output style's `string-limit` option
  bool lower = true;          // `gds lower`: false writes lowercase letters upper-cased
};

/// A string record payload and what changed while encoding it.
struct EncodedString {
  std::vector<std::byte> data;  // the payload, even length
  bool truncated = false;       // string-limit dropped leading characters
  bool changed = false;         // the table changed a character ("characters changed")
  bool unprintable = false;     // a byte above 127 was written as 'X'
  std::string mapped;           // the text after the table (Magic's modified string)
};

/// The payload of a string record (LIBNAME, STRNAME, SNAME, STRING,
/// PROPVALUE) for `text` (up to its first NUL), as calmaOutStringRecord
/// writes it. Under string-limit a text longer than 32 keeps its tail: with
/// magic-compat ON the last 32 bytes of the padded text, with it OFF the
/// last 32 characters (Q-MASK-207). The caller writes the record (and
/// RecordWriter refuses a payload over 65531 bytes).
[[nodiscard]] EncodedString encode_string(std::string_view text, const CharTable& table, const StringOptions& options,
                                          const compat::CompatSettings& compat);

}  // namespace hefesto::gds
