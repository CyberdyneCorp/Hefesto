// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Loading and safe saving of `.sch`/`.sym` files through the platform file
// services (capability schematic-file-format "Safe saving", "Round-trip
// guarantee"; design D6).
//
// Owner: FORMAT.

#pragma once

#include "hefesto/diag/outcome.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/file/document.hpp"
#include "hefesto/schem/file/read_write.hpp"

namespace hefesto::schem {

/// The kind a file name gives: `symbol` when it ends in `.sym`.
[[nodiscard]] FileKind file_kind_of(const platform::Path& path);

/// Reads `path` and parses it. The kind comes from the file name
/// (file_kind_of; `options.kind` is ignored) and `options.source_name`
/// defaults to the path. The document gets `path` and `disk_digest`; it is
/// not modified. Loading never writes anything.
[[nodiscard]] diag::Outcome<Document> load_document(platform::FileSystem& fs, const platform::Path& path,
                                                    ReadOptions options);

struct SaveOptions {
  /// The user confirmed overwriting a file that changed on disk since it was
  /// loaded (the save is refused with SCHEM-FILE-006 otherwise).
  bool overwrite_changed_file = false;
};

/// Writes `document` to `destination` atomically (FileSystem::write_atomic:
/// a temporary file in the destination folder, then a rename). A failed
/// write leaves the previous file and the document unchanged and returns the
/// error. When `destination` is the document's own path and the file there
/// no longer has the bytes the document was loaded from or last saved as,
/// the save is refused with SCHEM-FILE-006 unless
/// `options.overwrite_changed_file`. On success the document's `path`,
/// `disk_digest` and `modified` are updated, and its layer-5 boxes take the
/// order they were written in (sort_pin_boxes; `modified` is not set by it). Saving is always an explicit
/// request: nothing in this area saves on its own.
[[nodiscard]] diag::Status save_document(platform::FileSystem& fs, Document& document,
                                         const platform::Path& destination, const SaveOptions& options = {});

}  // namespace hefesto::schem
