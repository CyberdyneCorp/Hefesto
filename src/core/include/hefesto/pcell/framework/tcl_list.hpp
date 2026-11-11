// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Tcl 8.6 list syntax for the gencell framework (openspec/changes/
// m4-scripting/design.md D9/D10; capability pcell-toolkit "Parameter
// dictionary model", "Generated cell properties", "Native generator
// equivalence"): the `parameters` property of a generated cell is the
// parameter dictionary's Tcl string form, and the native and Tcl paths must
// save it byte for byte alike (D-29). These functions reproduce Tcl's own
// element scanning and formatting (generic/tclUtil.c TclFindElement,
// TclCopyAndCollapse, TclScanElement, TclConvertElement with COMPAT 1) so
// that the framework neither depends on the interpreter's Tcl_Obj
// conversion nor on an approximation of it.
//
// Owner: PCELL (design D2).

#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/value.hpp"
#include "hefesto/diag/outcome.hpp"

namespace hefesto::pcell {

/// Splits `text` into its list elements (Tcl_SplitList). Errors carry
/// Tcl's message (`unmatched open brace in list`, `list element in braces
/// followed by "x" instead of space`, ...) as PCELL-LIST-001.
[[nodiscard]] diag::Outcome<std::vector<std::string>> split_tcl_list(std::string_view text);

/// One element formatted as Tcl formats it inside a list (TclScanElement +
/// TclConvertElement). `first` quotes a leading `#` (Tcl does so for the
/// first element of a list or dictionary only).
[[nodiscard]] std::string tcl_list_element(std::string_view element, bool first = true);

/// The string form of a list of `elements` (Tcl_Merge / a list object's
/// string representation).
[[nodiscard]] std::string merge_tcl_list(std::span<const std::string> elements);

/// The Tcl string form of a command value (a script result): strings as
/// they are, integers in decimal, lists formatted with merge_tcl_list().
[[nodiscard]] std::string tcl_string(const cmd::Value& value);

}  // namespace hefesto::pcell
