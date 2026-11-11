// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/script/tcl/value_conversion.hpp"

#include <tcl.h>

#include <climits>
#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace hefesto::script {

namespace {

// Tcl 8.6 measures strings in int bytes.
int tcl_length(std::size_t size) {
  return size > static_cast<std::size_t>(INT_MAX) ? INT_MAX : static_cast<int>(size);
}

}  // namespace

Tcl_Obj* to_tcl(const cmd::Value& value) {
  return std::visit(
      [](const auto& data) -> Tcl_Obj* {
        using T = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          return Tcl_NewObj();
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
          return Tcl_NewWideIntObj(static_cast<Tcl_WideInt>(data));
        } else if constexpr (std::is_same_v<T, double>) {
          return Tcl_NewDoubleObj(data);
        } else if constexpr (std::is_same_v<T, std::string>) {
          return Tcl_NewStringObj(data.data(), tcl_length(data.size()));
        } else {
          Tcl_Obj* list = Tcl_NewListObj(0, nullptr);
          for (const auto& element : data) {
            Tcl_ListObjAppendElement(nullptr, list, to_tcl(element));
          }
          return list;
        }
      },
      value.data());
}

namespace {

// Tcl's object types of numbers and lists ("wideInt" does not exist where
// Tcl_WideInt is long, as on LP64).
struct ObjTypes {
  const Tcl_ObjType* integer = Tcl_GetObjType("int");
  const Tcl_ObjType* wide = Tcl_GetObjType("wideInt");
  const Tcl_ObjType* real = Tcl_GetObjType("double");
  const Tcl_ObjType* list = Tcl_GetObjType("list");
};

const ObjTypes& obj_types() {
  static const ObjTypes types;
  return types;
}

cmd::Value string_of(Tcl_Obj* object) {
  int length = 0;
  const char* text = Tcl_GetStringFromObj(object, &length);
  return cmd::Value{std::string{text, static_cast<std::size_t>(length)}};
}

}  // namespace

namespace {

// The value of a number or list object, or nullopt for other types.
std::optional<cmd::Value> typed_value(Tcl_Obj* object) {
  const ObjTypes& types = obj_types();
  const Tcl_ObjType* type = object->typePtr;
  if (type == nullptr) {
    return std::nullopt;
  }
  if (type == types.integer || type == types.wide) {
    Tcl_WideInt number = 0;
    if (Tcl_GetWideIntFromObj(nullptr, object, &number) == TCL_OK) {
      return cmd::Value{static_cast<std::int64_t>(number)};
    }
  } else if (type == types.real) {
    double number = 0.0;
    if (Tcl_GetDoubleFromObj(nullptr, object, &number) == TCL_OK) {
      return cmd::Value{number};
    }
  } else if (type == types.list) {
    int count = 0;
    Tcl_Obj** elements = nullptr;
    if (Tcl_ListObjGetElements(nullptr, object, &count, &elements) == TCL_OK) {
      cmd::Value::List list;
      list.reserve(static_cast<std::size_t>(count));
      for (int i = 0; i < count; ++i) {
        list.push_back(from_tcl(elements[i]));
      }
      return cmd::Value{std::move(list)};
    }
  }
  return std::nullopt;
}

}  // namespace

cmd::Value from_tcl(Tcl_Obj* object) {
  // The object's current internal type decides: a number stays a number, a
  // list stays a list (also a one-element list) - as long as the typed
  // value prints exactly as Tcl's string form ("0x10", "1.50", a list with
  // its original spacing or Tcl's backslash quoting keep their string).
  // Anything else, including a number-looking string Tcl has not used as a
  // number yet, and bignums, is the string form.
  cmd::Value text = string_of(object);
  if (auto typed = typed_value(object); typed && typed->to_string() == std::get<std::string>(text.data())) {
    return std::move(*typed);
  }
  return text;
}

}  // namespace hefesto::script
