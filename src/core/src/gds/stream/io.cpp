// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold (openspec/changes/m5-mask-gds-cif/design.md D3/D4): the memory
// source and sink work; the file source and sink are CORE-NOTIMPL-005 stubs
// until GDSR lands the plain and gzip streams (tasks.md section 9).
// Owner: GDSR.

#include "hefesto/gds/stream/io.hpp"

#include <algorithm>

#include "hefesto/cmd/stubs.hpp"
#include "hefesto/diag/codes.hpp"

namespace hefesto::gds {

diag::Outcome<std::size_t> MemorySource::read(std::span<std::byte> out) {
  const std::uint64_t left = offset_ < bytes_.size() ? bytes_.size() - offset_ : 0;
  const std::size_t count = static_cast<std::size_t>(std::min<std::uint64_t>(left, out.size()));
  std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(offset_), count, out.begin());
  offset_ += count;
  return count;
}

diag::Status MemorySource::seek(std::uint64_t offset) {
  if (offset > bytes_.size()) {
    return diag::make(diag::codes::kPlatformRead, "seek past the end of the stream");
  }
  offset_ = offset;
  return diag::Done{};
}

diag::Status MemorySink::write(std::span<const std::byte> bytes) {
  bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
  return diag::Done{};
}

diag::Outcome<std::unique_ptr<ByteSource>> open_source(platform::FileSystem& /*fs*/, const platform::Path& /*path*/) {
  return cmd::not_implemented_diagnostic("gds::open_source");
}

diag::Outcome<std::unique_ptr<ByteSink>> open_sink(platform::FileSystem& /*fs*/, const platform::Path& /*path*/,
                                                    int /*compress_level*/) {
  return cmd::not_implemented_diagnostic("gds::open_sink");
}

}  // namespace hefesto::gds
