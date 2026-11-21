// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Loading and safe saving (schematic-file-format "Safe saving"); see save.hpp.
//
// Owner: FORMAT.

#include "hefesto/schem/file/save.hpp"

#include <string>
#include <utility>

#include "codes.hpp"
#include "hefesto/pdk/sha256.hpp"

namespace hefesto::schem {

namespace {

std::string digest_of(std::string_view bytes) {
  pdk::Sha256 sha;
  sha.update(bytes);
  return sha.finish_hex();
}

// Whether the file at the document's own path differs from what the
// document was loaded from or last saved as. A file that is gone, or a
// document never tied to disk, has nothing to lose.
bool changed_on_disk(platform::FileSystem& fs, const Document& document, const platform::Path& destination) {
  if (document.disk_digest.empty() || destination != document.path || !fs.exists(destination)) {
    return false;
  }
  const auto current = fs.read(destination);
  return !current.ok() || digest_of(current.value()) != document.disk_digest;
}

}  // namespace

FileKind file_kind_of(const platform::Path& path) {
  return path.extension() == ".sym" ? FileKind::symbol : FileKind::schematic;
}

diag::Outcome<Document> load_document(platform::FileSystem& fs, const platform::Path& path, ReadOptions options) {
  auto bytes = fs.read(path);
  if (!bytes.ok()) {
    return bytes.error();
  }
  options.kind = file_kind_of(path);
  if (options.source_name.empty()) {
    options.source_name = path.string();
  }
  auto read = read_document(bytes.value(), options);
  if (!read.ok()) {
    return read.error();
  }
  Document document = std::move(read).value();
  document.path = path;
  document.disk_digest = digest_of(bytes.value());
  document.modified = false;
  return document;
}

diag::Status save_document(platform::FileSystem& fs, Document& document, const platform::Path& destination,
                           const SaveOptions& options) {
  if (!options.overwrite_changed_file && changed_on_disk(fs, document, destination)) {
    return diag::make(file_codes::kChangedOnDisk,
                      destination.string() + " changed on disk since it was loaded; not overwritten");
  }
  const std::string text = write_document(document);
  auto written = fs.write_atomic(destination, text);
  if (!written.ok()) {
    return written.error();
  }
  document.path = destination;
  document.disk_digest = digest_of(text);
  document.modified = false;
  return diag::Done{};
}

}  // namespace hefesto::schem
