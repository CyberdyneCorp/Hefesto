// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (openspec/changes/m5-mask-gds-cif/design.md D3): the
// encoders answer zero values and an identity character map until GDSR
// ports them (tasks.md section 9). Owner: GDSR.

#include "hefesto/gds/stream/encode.hpp"

namespace hefesto::gds {

std::array<std::byte, 8> encode_real8(double /*value*/) {
  return {};
}

double decode_real8(std::span<const std::byte, 8> /*bytes*/) {
  return 0.0;
}

CharTable::CharTable(Kind kind, const compat::CompatSettings& /*compat*/) : kind_(kind) {
  for (std::size_t c = 0; c < table_.size(); ++c) {
    table_[c] = static_cast<char>(c);
  }
}

std::string CharTable::map(std::string_view text) const {
  std::string out{text};
  for (char& c : out) {
    const auto index = static_cast<unsigned char>(c);
    if (index < table_.size()) {
      c = table_[index];
    }
  }
  return out;
}

std::array<std::int16_t, 6> encode_date(std::int64_t /*time*/, std::string_view /*zone*/) {
  return {};
}

}  // namespace hefesto::gds
