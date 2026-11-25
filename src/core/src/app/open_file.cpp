// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The generic open path (see hefesto/app/open_file.hpp; capability
// oasis-command "Format recognition on open").
//
// Owner: OASCMD (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2, D15).

#include "hefesto/app/open_file.hpp"

#include "hefesto/cmd/value.hpp"
#include "hefesto/gds/stream/io.hpp"

namespace hefesto::app {

std::string OpenCommand::line() const {
  cmd::Value::List list;
  for (const std::string& word : words) {
    list.emplace_back(word);
  }
  return cmd::Value{std::move(list)}.to_string();
}

diag::Outcome<OpenCommand> open_command(platform::FileSystem& fs, const platform::Path& path) {
  auto source = gds::open_source(fs, path);  // gzip-wrapped files are inflated
  if (!source.ok()) {
    return source.error();
  }
  OpenCommand open;
  open.format = oasis::sniff_format(*source.value());
  const std::string file = path.string();
  switch (open.format) {
    case oasis::FileFormat::oasis:
      open.words = {"oasis", "read", file};
      break;
    case oasis::FileFormat::gds:
      open.words = {"gds", "read", file};
      break;
    case oasis::FileFormat::magic:
      open.words = {"load", file};
      break;
    case oasis::FileFormat::unknown:
      return diag::make("OASIS-CMD-005", "Cannot open " + file +
                                             ": unknown format (not OASIS, GDS or a Magic cell file).");
  }
  return open;
}

}  // namespace hefesto::app
