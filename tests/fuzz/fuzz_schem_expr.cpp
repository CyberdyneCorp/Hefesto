// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for `expr` evaluation and format substitution
// (capability schematic-netlisting "expr evaluation", "Format string
// substitution"; m8-schematic design D19). The first byte picks the
// xschem-compat mode (Q-SCHEM-12) and the ev_precision; the rest is
// substituted as text with expr markers, evaluated as one expression, and
// used as an instance format string.
//
// Written by NETLIST; tests/fuzz/ is shared (INTEGRATION).

#include <cstddef>
#include <cstdint>
#include <string>

#include "fuzz_common.hpp"
#include "hefesto/schem/net/expr.hpp"
#include "hefesto/schem/net/substitute.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  namespace schem = hefesto::schem;
  const auto [mode, text] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  const bool compat = (mode & 1U) != 0;
  const int precision = (mode >> 1U) - 8;  // mode promotes to int; includes negative and large values

  (void)schem::substitute_expr(text, schem::ExprOptions{compat, precision});
  (void)schem::evaluate_expr(text, compat);

  const schem::PropertyMap symbol{{"format", std::string{text}}};
  const schem::PropertyMap templ{{"name", "R1"}, {"value", "@W"}, {"ad", "expr(@W * 2)"}};
  const schem::PropertyMap instance{{"name", "R1[1:0]"}, {"W", std::string{text.substr(0, 64)}}};
  schem::ElementContext context;
  context.instance = &instance;
  context.symbol = &symbol;
  context.templ = &templ;
  context.parent_template = &templ;
  context.symref = "res.sym";
  context.pins = {{"P", 1, "A", false, nullptr}, {"M", 2, "", false, nullptr}};
  schem::ElementOptions options;
  options.xschem_compat = compat;
  options.ev_precision = precision;
  schem::ElementRunState state;
  (void)schem::build_element(context, options, state);
  return 0;
}
