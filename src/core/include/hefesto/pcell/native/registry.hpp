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
// A generator is a port of the PDK's Tcl procedures: it issues the same
// engine commands with the same words (numbers formatted as Tcl formats
// them) through a NativeHost, so the database ends in the same state.
// Inputs it cannot reproduce exactly (a parameter outside Tcl's number
// subset of tcl_number.hpp) are refused before anything is drawn, and the
// framework then runs the Tcl generator.
//
// Owner: PCELL (design D2).

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/pcell/framework/parameters.hpp"
#include "hefesto/pcell/native/helpers.hpp"
#include "hefesto/script/types.hpp"

namespace hefesto::pcell {

/// What a native generator may do; implemented by the gencell framework
/// over the project's command context (pcell/framework/native_bridge.cpp)
/// and by recording fakes in tests.
class NativeHost {
 public:
  NativeHost() = default;
  NativeHost(const NativeHost&) = delete;
  NativeHost& operator=(const NativeHost&) = delete;
  virtual ~NativeHost() = default;

  /// Runs an engine command as the PDK's Tcl calls it (`::magic::<name>`
  /// with Magic's quiet status convention) and returns its result as the
  /// Tcl string. Throws native::TclError when the command raises in Tcl.
  virtual std::string run(const std::vector<std::string>& words) = 0;
  /// `puts -nonewline stdout|stderr $text`.
  virtual void puts(bool to_stderr, std::string_view text) = 0;
  /// `set <name> <value>` of a namespace variable (e.g. magic::minfo_val).
  virtual void set_variable(std::string_view name, std::string_view value) = 0;
};

class NativeGenerator {
 public:
  NativeGenerator() = default;
  NativeGenerator(const NativeGenerator&) = delete;
  NativeGenerator& operator=(const NativeGenerator&) = delete;
  virtual ~NativeGenerator() = default;

  /// `<lib>::<type>_defaults` (the same dictionary string).
  [[nodiscard]] virtual ParameterDict defaults() const = 0;
  /// `<lib>::<type>_check`: the corrected parameters (its `puts stderr`
  /// messages go through `host`) or the Tcl error it raises; nullopt when
  /// the parameters cannot be reproduced natively (nothing was printed).
  [[nodiscard]] virtual std::optional<diag::Outcome<ParameterDict>> check(NativeHost& host,
                                                                         const ParameterDict& parameters) const = 0;
  /// `<lib>::<type>_draw` into the current edit cell: the procedure's
  /// result, or its raised Tcl error; nullopt when the parameters cannot be
  /// reproduced natively (nothing was drawn).
  [[nodiscard]] virtual std::optional<cmd::CommandResult> draw(NativeHost& host,
                                                               const ParameterDict& parameters) const = 0;
};

class NativeRegistry {
 public:
  /// Registers `generator` for `device` ("sky130::sky130_fd_pr__nfet_01v8",
  /// `<library>::<type>` as gencell names it) of `pdk` (exact version). A
  /// second registration of the same key replaces it.
  void add(const script::PdkIdentity& pdk, std::string device, std::unique_ptr<NativeGenerator> generator);
  /// The generator of `device` for exactly `pdk`, or nullptr (Tcl fallback).
  [[nodiscard]] const NativeGenerator* find(const script::PdkIdentity& pdk, std::string_view device) const;
  /// True when `device` has a generator for another version of `pdk`'s
  /// family and variant (the fallback is then reported).
  [[nodiscard]] bool has_other_version(const script::PdkIdentity& pdk, std::string_view device) const;
  [[nodiscard]] std::size_t size() const noexcept { return generators_.size(); }
  /// The registered keys (PDK identity, device), in order.
  [[nodiscard]] std::vector<std::tuple<script::PdkIdentity, std::string>> keys() const;

 private:
  using Key = std::tuple<script::PdkIdentity, std::string>;
  std::map<Key, std::unique_ptr<NativeGenerator>, std::less<>> generators_;
};

/// The registry holding every native generator built into Hefesto
/// (pcell/native/builtin.cpp).
[[nodiscard]] const NativeRegistry& builtin_generators();

}  // namespace hefesto::pcell
