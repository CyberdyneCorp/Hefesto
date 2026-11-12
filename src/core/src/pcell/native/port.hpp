// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The building blocks of the native generator ports (design D10): Tcl
// dictionaries and local variables of Val, and the engine calls the PDK
// procedures make (`box`, `paint`, `pushbox`, ...), issued through a
// NativeHost with the words Tcl would pass. Each generator port is a
// line-by-line translation of the PDK's Tcl procedure using these.
//
// Owner: PCELL (design D2).

#pragma once

#include <initializer_list>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/pcell/framework/parameters.hpp"
#include "hefesto/pcell/native/helpers.hpp"
#include "hefesto/pcell/native/registry.hpp"
#include "pcell/native/tcl_value.hpp"

namespace hefesto::pcell::native {

using List = std::vector<Val>;

/// A Tcl dictionary of values (insertion order kept, as Tcl's dict).
class Dict {
 public:
  Dict() = default;
  Dict(std::initializer_list<std::pair<std::string, Val>> entries);
  [[nodiscard]] static Dict from(const ParameterDict& parameters);
  [[nodiscard]] ParameterDict to_parameters() const;

  void set(std::string_view key, Val value);
  /// `dict get`: TclError `key "k" not known in dictionary` when absent.
  [[nodiscard]] const Val& get(std::string_view key) const;
  [[nodiscard]] bool has(std::string_view key) const;
  /// `dict merge $this $other`.
  [[nodiscard]] Dict merged(const Dict& other) const;
  [[nodiscard]] const std::vector<std::pair<std::string, Val>>& entries() const noexcept { return entries_; }

 private:
  std::vector<std::pair<std::string, Val>> entries_;
};

/// A procedure's local variables.
class Locals {
 public:
  void set(std::string_view name, Val value) { vars_.insert_or_assign(std::string{name}, std::move(value)); }
  /// `$name`: TclError `can't read "name": no such variable` when unset.
  [[nodiscard]] const Val& operator[](std::string_view name) const;
  [[nodiscard]] bool has(std::string_view name) const { return vars_.find(name) != vars_.end(); }
  /// foreach key [dict keys $parameters] {set $key [dict get $parameters $key]}
  void load(const Dict& parameters);

 private:
  std::map<std::string, Val, std::less<>> vars_;
};

/// Refuses (Unsupported) a dictionary the port cannot reproduce: a key named
/// "parameters" (it would replace the procedure argument), or a value that
/// is not a Tcl number for a key outside `text_keys`.
void require_numbers(const Dict& parameters, const std::set<std::string, std::less<>>& text_keys);

/// The engine calls of a generator.
class Engine {
 public:
  Engine(NativeHost& host, HelperFamily family) : host_(host), family_(family) {}

  /// Runs an engine command; its Tcl result.
  std::string run(std::initializer_list<std::string_view> words);
  std::string run(const std::vector<std::string>& words) { return host_.run(words); }

  void pushbox() { run({"pushbox"}); }
  void popbox() { run({"popbox"}); }
  void paint(const Val& type) { run({"paint", type.str()}); }
  void grow(std::string_view dir, const Val& v) { run({"box", "grow", dir, um(v)}); }
  void move(std::string_view dir, const Val& v) { run({"box", "move", dir, um(v)}); }
  /// `box values` as numbers.
  [[nodiscard]] List box_values();

  /// `puts stdout|stderr $text` (a newline is added).
  void puts(bool to_stderr, std::string_view text);
  /// `puts -nonewline stderr $text`.
  void puts_nonewline(bool to_stderr, std::string_view text);
  void set_variable(std::string_view name, std::string_view value) { host_.set_variable(name, value); }

  /// The PDK helpers with the current `[magic::cif scale output]`.
  [[nodiscard]] PdkHelpers helpers();
  /// magic::spice2float $v ?faultval? (a faulted value prints the PDK's message).
  [[nodiscard]] Val spice2float(const Val& v, std::string_view faultval = "0.0");
  [[nodiscard]] Val three_digit_past_decimal(const Val& v);
  [[nodiscard]] Val i2u(const Val& v);
  [[nodiscard]] Val u2i(const Val& v);

  [[nodiscard]] NativeHost& host() noexcept { return host_; }

 private:
  NativeHost& host_;
  HelperFamily family_;
};

/// A host that holds back what a `_check` port prints and sets until the
/// port finished, so a port refused midway (Unsupported) leaves no output
/// and the Tcl `_check` prints instead. Engine commands pass through.
class BufferedHost final : public NativeHost {
 public:
  explicit BufferedHost(NativeHost& target) : target_(target) {}
  std::string run(const std::vector<std::string>& words) override { return target_.run(words); }
  std::string evaluate(std::string_view script) override { return target_.evaluate(script); }
  [[nodiscard]] bool magic_compat() const override { return target_.magic_compat(); }
  [[nodiscard]] std::function<void()> keep_edit_context() override { return target_.keep_edit_context(); }
  void puts(bool to_stderr, std::string_view text) override;
  void set_variable(std::string_view name, std::string_view value) override;
  /// Replays the held output and variables on the target.
  void flush();

 private:
  struct Held {
    bool variable = false;
    bool to_stderr = false;
    std::string name;
    std::string text;
  };
  NativeHost& target_;
  std::vector<Held> held_;
};

/// `expr <text>` of a text built by substitution (an unbraced `expr`):
/// TclError for Tcl's errors, Unsupported outside the arithmetic subset.
[[nodiscard]] Val expr_text(const std::string& text);

/// `<pdk>::getbox` of open_pdks (sky130A-util.tcl:25-35, gf180mcuD-util.tcl:25-35):
/// the box in microns, each of [box values] times [cif scale out].
[[nodiscard]] List getbox(Engine& e);
/// `<pdk>::unionbox box1 box2` of open_pdks (the same in both PDKs).
[[nodiscard]] List unionbox(const List& box1, const List& box2);

/// The Tcl list form of `values` (for messages that print a list).
[[nodiscard]] std::string list_string(const List& values);

/// Runs `body` and maps the port's exceptions: Unsupported to nullopt
/// (the Tcl generator runs), TclError to a raised script error.
template <typename Body>
std::optional<cmd::CommandResult> run_port(Body&& body);

/// A raised Tcl error result (PCELL-GEN-001) with `message`.
[[nodiscard]] cmd::CommandResult tcl_error_result(std::string message);

/// Runs a `_check` port: Unsupported to nullopt, TclError to the error.
template <typename Body>
std::optional<diag::Outcome<ParameterDict>> run_check(Body&& body) {
  try {
    return diag::Outcome<ParameterDict>{body()};
  } catch (const Unsupported&) {
    return std::nullopt;
  } catch (const TclError& e) {
    return diag::Outcome<ParameterDict>{tcl_error_result(e.message).diagnostics.front()};
  }
}

template <typename Body>
std::optional<cmd::CommandResult> run_port(Body&& body) {
  try {
    return body();
  } catch (const Unsupported&) {
    return std::nullopt;
  } catch (const TclError& e) {
    return tcl_error_result(e.message);
  }
}

}  // namespace hefesto::pcell::native
