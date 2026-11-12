// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/pcell/native/registry.hpp"

#include <utility>

namespace hefesto::pcell {

void NativeRegistry::add(const script::PdkIdentity& pdk, std::string device,
                         std::unique_ptr<NativeGenerator> generator) {
  generators_.insert_or_assign(Key{pdk, std::move(device)}, std::move(generator));
}

const NativeGenerator* NativeRegistry::find(const script::PdkIdentity& pdk, std::string_view device) const {
  const auto it = generators_.find(Key{pdk, std::string{device}});
  return it == generators_.end() ? nullptr : it->second.get();
}

bool NativeRegistry::has_other_version(const script::PdkIdentity& pdk, std::string_view device) const {
  for (const auto& [key, generator] : generators_) {
    const auto& [identity, name] = key;
    if (name == device && identity.family == pdk.family && identity.variant == pdk.variant &&
        identity.version != pdk.version) {
      return true;
    }
  }
  return false;
}

std::vector<std::tuple<script::PdkIdentity, std::string>> NativeRegistry::keys() const {
  std::vector<Key> out;
  out.reserve(generators_.size());
  for (const auto& [key, generator] : generators_) {
    out.push_back(key);
  }
  return out;
}

}  // namespace hefesto::pcell
