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

const NativeRegistry& builtin_generators() {
  // SCAFFOLD (PCELL tasks 7.5/7.6): no native generator yet.
  static const NativeRegistry kRegistry;
  return kRegistry;
}

}  // namespace hefesto::pcell
