// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `tcleval(...)` and `@tcleval(...)` values in the sandboxed Tcl
// interpreter (openspec/changes/m8-schematic design.md D7, D14, "Area
// contracts" SYMLIB; capability schematic-symbols-and-libraries "tcleval
// values", "Script execution permission", Q-SCHEM-8, Q-SCHEM-25).
//
// Owner: SYMLIB.

#pragma once

#include <string>
#include <string_view>

namespace hefesto::script {
class Interpreter;
}

namespace hefesto::schem {

/// The value of a failed or refused evaluation: `?` followed by LF.
inline constexpr std::string_view kTclFailure = "?\n";

/// The Tcl globals set before an evaluation ("tcleval values" step 3 and the
/// `@tcleval` token).
struct TclContext {
  std::string path;          // hierarchical path without the leading `.`
  std::string sch_basename;  // base name of the schematic being netlisted
  std::string instname;      // `@tcleval` only: the instance name
  std::string symname;       // `@tcleval` only: the symbol reference
};

class TclBridge {
 public:
  /// `interpreter` is the sandboxed interpreter holding the inventoried
  /// procedures; nullptr makes every evaluation fail. Not owned.
  explicit TclBridge(script::Interpreter* interpreter = nullptr) noexcept : interpreter_(interpreter) {}

  /// Evaluates a value that begins with `tcleval(`; kTclFailure on failure.
  [[nodiscard]] std::string evaluate(std::string_view value, const TclContext& context) const;

 private:
  script::Interpreter* interpreter_;
};

}  // namespace hefesto::schem
