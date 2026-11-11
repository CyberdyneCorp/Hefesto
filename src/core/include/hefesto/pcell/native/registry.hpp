// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Native PCell generators (openspec/changes/m4-scripting/design.md D4/D10;
// capability hefesto-scripting "Native PCell generators validated against
// Tcl", pcell-toolkit "Native generator equivalence"; D-29): a C++
// generator registered for an exact PDK identity (family, variant, version)
// replaces the PDK's Tcl `_defaults`/`_check`/`_draw` for that version only;
// any other version falls back to Tcl. The saved .mag of both paths is
// byte-identical except the `timestamp` line.
//
// Owner: PCELL (design D2).

#pragma once

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/pcell/framework/parameters.hpp"
#include "hefesto/script/types.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::pcell {

class NativeGenerator {
 public:
  NativeGenerator() = default;
  NativeGenerator(const NativeGenerator&) = delete;
  NativeGenerator& operator=(const NativeGenerator&) = delete;
  virtual ~NativeGenerator() = default;

  /// `<lib>::<type>_defaults`.
  [[nodiscard]] virtual ParameterDict defaults() const = 0;
  /// `<lib>::<type>_check`: the corrected parameters; messages as diagnostics.
  [[nodiscard]] virtual ParameterDict check(const ParameterDict& parameters,
                                            std::vector<diag::Diagnostic>& messages) const = 0;
  /// `<lib>::<type>_draw` into the edit cell of `editor`.
  [[nodiscard]] virtual cmd::CommandResult draw(editor::EditorState& editor, const ParameterDict& parameters) const = 0;
};

class NativeRegistry {
 public:
  /// Registers `generator` for `device` ("sky130_fd_pr__nfet_01v8") of `pdk`
  /// (exact version). A second registration of the same key replaces it.
  void add(const script::PdkIdentity& pdk, std::string device, std::unique_ptr<NativeGenerator> generator);
  /// The generator of `device` for exactly `pdk`, or nullptr (Tcl fallback).
  [[nodiscard]] const NativeGenerator* find(const script::PdkIdentity& pdk, std::string_view device) const;
  [[nodiscard]] std::size_t size() const noexcept { return generators_.size(); }

 private:
  using Key = std::tuple<script::PdkIdentity, std::string>;
  std::map<Key, std::unique_ptr<NativeGenerator>, std::less<>> generators_;
};

/// The registry holding every native generator built into Hefesto.
/// SCAFFOLD: empty until PCELL tasks 7.5/7.6.
[[nodiscard]] const NativeRegistry& builtin_generators();

}  // namespace hefesto::pcell
