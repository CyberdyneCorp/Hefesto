// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `tcleval(...)` values evaluated in the sandboxed interpreter (capability
// schematic-symbols-and-libraries "tcleval values", steps 1-5; Q-SCHEM-25:
// they always run, in the sandbox).
//
// Owner: SYMLIB.

#include "hefesto/schem/tcl/tcl_bridge.hpp"

#include "hefesto/cmd/value.hpp"
#include "hefesto/script/interp.hpp"

namespace hefesto::schem {

namespace {

constexpr std::string_view kPrefix = "tcleval(";

bool is_blank(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

}  // namespace

std::string tcleval_body(std::string_view value) {
  std::string body;
  body.reserve(value.size());
  // Step 1: every `\}` becomes `}`.
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '\\' && i + 1 < value.size() && value[i + 1] == '}') continue;
    body.push_back(value[i]);
  }
  // Step 2: the leading `tcleval(` and the last `)` with the blanks after it.
  if (std::string_view{body}.starts_with(kPrefix)) body.erase(0, kPrefix.size());
  const std::size_t close = body.rfind(')');
  if (close != std::string::npos) {
    std::size_t end = close + 1;
    while (end < body.size() && is_blank(body[end])) ++end;
    body.erase(close, end - close);
  }
  return body;
}

std::string TclBridge::evaluate(std::string_view value, const TclContext& context) const {
  if (interpreter_ == nullptr) return std::string{kTclFailure};
  // Step 3: the globals; instname and symname only for `@tcleval`.
  interpreter_->set_var("::path", cmd::Value{context.path});
  interpreter_->set_var("::sch_basename", cmd::Value{context.sch_basename});
  if (context.instname.empty() && context.symname.empty()) {
    (void)interpreter_->evaluate("unset -nocomplain ::instname ::symname");
  } else {
    interpreter_->set_var("::instname", cmd::Value{context.instname});
    interpreter_->set_var("::symname", cmd::Value{context.symname});
  }
  // Steps 4 and 5: the body of `subst {...}` in the global scope.
  const auto result = interpreter_->evaluate("subst {" + tcleval_body(value) + "}");
  return result.ok() ? result.value.to_string() : std::string{kTclFailure};
}

}  // namespace hefesto::schem
