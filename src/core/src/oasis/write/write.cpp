// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// OASIS write: the prepare-option mapping (INTEGRATION) and the scaffold
// stub of the entry point (openspec/changes/m6-lef-def-oasis/design.md D4).
//
// Owner: OASW (design D2), except to_gds_write_options (INTEGRATION).

#include "hefesto/oasis/write/write.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::oasis {

gds::GdsWriteOptions to_gds_write_options(const OasisSettings& settings,
                                          const stream::StreamReadSettings& /*stream_read*/) {
  gds::GdsWriteOptions options;
  gds::GdsSettings& gds = options.settings;
  gds.magscale = settings.magscale;
  gds.library = settings.library;
  gds.labels = settings.labels;
  gds.lower = settings.lower;
  gds.arrays = settings.arrays;
  gds.addendum = settings.addendum;
  gds.allow_undefined = settings.allow_undefined;
  gds.allow_abstract = settings.allow_abstract;
  gds.contacts = settings.contacts;
  gds.merge = settings.merge;
  gds.compress = 0;
  return options;
}

cmd::CommandResult write(editor::EditorState& /*editor*/, db::CellDef& /*root*/, const platform::Path& /*path*/,
                         const OasisWriteOptions& /*options*/, const mask::Progress* /*progress*/) {
  return cmd::not_implemented("oasis write");
}

}  // namespace hefesto::oasis
