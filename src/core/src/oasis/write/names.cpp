// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Output names and string mapping (oasis-write "Cell names", "String
// mapping"; Q-OASIS-60, Q-OASIS-61). The GDS stream already carries the
// gds-write mapping (character table, `lower`, string-limit, XXXXX<n> for
// illegal and colliding names); OASIS adds the a-string rule (no 0x00) and
// the n-string rule (no empty name and no space).
// Owner: OASW.

#include <algorithm>
#include <utility>

#include "hefesto/oasis/codec/primitives.hpp"
#include "internal.hpp"

namespace hefesto::oasis::detail {

namespace {

std::string without_nul(std::string text) {
  std::ranges::replace(text, '\0', '_');
  return text;
}

}  // namespace

std::string unpadded(std::string_view payload) {
  if (!payload.empty() && payload.size() % 2 == 0 && payload.back() == '\0') {
    payload.remove_suffix(1);
  }
  return without_nul(std::string{payload});
}

std::string mapped_text(std::string_view payload, const gds::CharTable& table, const gds::StringOptions& options,
                        const compat::CompatSettings& compat) {
  const std::string_view text = payload.substr(0, payload.find('\0'));
  const gds::EncodedString encoded = gds::encode_string(text, table, options, compat);
  std::string data(reinterpret_cast<const char*>(encoded.data.data()), encoded.data.size());
  if (!encoded.truncated) {
    data.resize(std::min(data.size(), text.size()));  // the exact length: no padding byte
    return without_nul(std::move(data));
  }
  return unpadded(data);
}

std::string OutputNames::fresh(std::string text) {
  if (!text.empty() && legal_string(StringKind::n_string, text) && !taken_.contains(text)) {
    return text;
  }
  const std::string base = "XXXXX" + std::to_string(by_payload_.size() + 1);
  std::string name = base;
  for (std::uint64_t k = 1; taken_.contains(name); ++k) {
    name = base + "_" + std::to_string(k);
  }
  if (sink_ != nullptr && !text.empty() && legal_string(StringKind::n_string, text)) {
    sink_->emit(diag::make_native(codes::kCellName, "Warning: cell name \"" + text +
                                                        "\" is already used in this OASIS file; the cell is written "
                                                        "as \"" + name + "\"."));
  }
  return name;
}

const std::string& OutputNames::of(std::string_view payload) {
  std::string key{payload.substr(0, payload.find('\0'))};
  if (const auto found = by_payload_.find(key); found != by_payload_.end()) {
    return found->second;
  }
  std::string name = fresh(key);
  taken_.insert(name);
  return by_payload_.emplace(std::move(key), std::move(name)).first->second;
}

}  // namespace hefesto::oasis::detail
