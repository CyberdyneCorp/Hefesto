// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ExtStyle and StyleTable (extract/tech/style.hpp). Scaffold stubs: the
// builders answer neutral values until EXTTECH lands them
// (openspec/changes/m7-extraction tasks.md 4.1-4.6).
//
// Owner: EXTTECH (design D3, D7).

#include "hefesto/extract/tech/style.hpp"

namespace hefesto::extract {

tech::TypeMask ExtStyle::gate_types() const {
  tech::TypeMask gates;
  for (const DevicePattern& device : devices) {
    gates |= device.types;
  }
  return gates;
}

ExtStyle ExtStyle::rescale(std::int32_t /*n*/, std::int32_t /*d*/, const compat::CompatSettings& /*compat*/) const {
  return *this;
}

ExtStyle ExtStyle::micron_convert(const mask::OutputStyle* /*style*/) const {
  return *this;
}

std::shared_ptr<const ExtStyle> StyleTable::select(std::string_view /*name*/) const {
  return nullptr;
}

StyleTable load_styles(const tech::RawSection& /*section*/, const tech::Technology& /*technology*/,
                       diag::DiagnosticSink& /*sink*/, const compat::CompatSettings& /*compat*/) {
  return StyleTable{};
}

}  // namespace hefesto::extract
