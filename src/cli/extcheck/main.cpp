// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-extcheck entry point (openspec/changes/m7-extraction/design.md
// D15): passes argv to netlist::extcheck::run, which prints the usage text
// with argv[0] as the program name.
//
// Owner: SIM (design D3).

#include <iostream>
#include <string>
#include <vector>

#include "hefesto/netlist/extcheck/extcheck.hpp"

int main(int argc, char** argv) {
  const std::vector<std::string> args(argv + (argc > 0 ? 1 : 0), argv + argc);
  const std::string program = argc > 0 ? std::string{argv[0]} : std::string{"hefesto-extcheck"};
  return hefesto::netlist::extcheck::run(args, std::cout, std::cerr, program);
}
