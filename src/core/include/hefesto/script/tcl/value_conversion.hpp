// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// cmd::Value <-> Tcl_Obj (openspec/changes/m4-scripting/design.md D4):
// integers and reals keep their Tcl number types, strings stay strings and
// lists stay lists (nested lists are Tcl lists of lists); none is the empty
// object. Tcl_Obj is only forward-declared here, so this header does not
// pull tcl.h into its includers; only src/core/src/script/tcl/ uses it.
//
// from_tcl follows the object's current Tcl type: an integer or double
// object becomes an integer or real, a list object a list (recursively), and
// any other object (including a numeric-looking string Tcl has not used as
// a number) its string form. A typed value is returned only when its
// cmd::Value::to_string() equals Tcl's string form of the object, so the
// conversion never changes what a result prints as ("0x10", "1.50", a list
// with its own spacing or Tcl's backslash quoting stay strings). The two
// directions round-trip every value except none, which comes back as the
// empty string.
//
// Owner: TCL (design D2).

#pragma once

#include "hefesto/cmd/value.hpp"

struct Tcl_Obj;  // tcl.h: typedef struct Tcl_Obj {...} Tcl_Obj;

namespace hefesto::script {

/// A new Tcl object with reference count 0 holding `value`.
[[nodiscard]] Tcl_Obj* to_tcl(const cmd::Value& value);

/// The value of `object` (not null). The object's reference count is not
/// changed.
[[nodiscard]] cmd::Value from_tcl(Tcl_Obj* object);

}  // namespace hefesto::script
