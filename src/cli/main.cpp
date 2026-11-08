// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-cli entry point; the logic is in app.cpp.

#include <cstdio>
#include <string_view>
#include <vector>

#include "app.hpp"

int main(int argc, char** argv) {
  const std::vector<std::string_view> args(argv + (argc > 0 ? 1 : 0), argv + argc);
  return hefesto::cli::run(args, stdout, stderr);
}
