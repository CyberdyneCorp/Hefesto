// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Byte sources and sinks (see hefesto/gds/stream/io.hpp). platform::
// FileSystem reads and writes whole files, so a file source holds the file
// in memory (inflated on the fly when it is gzip) and a file sink writes
// the file atomically when it finishes. Owner: GDSR.

#include "hefesto/gds/stream/io.hpp"

#include <algorithm>
#include <string>

#include "hefesto/gds/stream/gzip.hpp"

namespace hefesto::gds {

namespace {

/// Collects the output and replaces the file on finish().
class FileSink final : public ByteSink {
 public:
  FileSink(platform::FileSystem& fs, platform::Path path) : fs_(fs), path_(std::move(path)) {}

  [[nodiscard]] diag::Status write(std::span<const std::byte> bytes) override {
    data_.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return diag::Done{};
  }
  [[nodiscard]] std::uint64_t position() const noexcept override { return data_.size(); }
  [[nodiscard]] diag::Status finish() override {
    auto status = fs_.write_atomic(path_, data_);
    data_.clear();
    data_.shrink_to_fit();
    return status;
  }

 private:
  platform::FileSystem& fs_;
  platform::Path path_;
  std::string data_;
};

}  // namespace

diag::Outcome<std::size_t> MemorySource::read(std::span<std::byte> out) {
  const std::uint64_t left = offset_ < bytes_.size() ? bytes_.size() - offset_ : 0;
  const std::size_t count = static_cast<std::size_t>(std::min<std::uint64_t>(left, out.size()));
  std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(offset_), count, out.begin());
  offset_ += count;
  return count;
}

diag::Status MemorySource::seek(std::uint64_t offset) {
  if (offset > bytes_.size()) {
    return diag::make("GDS-STREAM-003", "Seek to byte " + std::to_string(offset) +
                                            " is past the end of the GDS stream (" + std::to_string(bytes_.size()) +
                                            " bytes).");
  }
  offset_ = offset;
  return diag::Done{};
}

diag::Status MemorySink::write(std::span<const std::byte> bytes) {
  bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
  return diag::Done{};
}

diag::Outcome<std::unique_ptr<ByteSource>> open_source(platform::FileSystem& fs, const platform::Path& path) {
  auto text = fs.read(path);
  if (!text.ok()) {
    return text.error();
  }
  std::vector<std::byte> bytes(text.value().size());
  std::copy_n(reinterpret_cast<const std::byte*>(text.value().data()), bytes.size(), bytes.begin());
  text = std::string{};  // release the copy before inflating
  if (is_gzip(bytes)) {
    return make_gzip_source(std::move(bytes));
  }
  return std::unique_ptr<ByteSource>{std::make_unique<MemorySource>(std::move(bytes))};
}

diag::Outcome<std::unique_ptr<ByteSink>> open_sink(platform::FileSystem& fs, const platform::Path& path,
                                                    int compress_level) {
  auto file = std::make_unique<FileSink>(fs, path);
  if (compress_level == 0) {
    return std::unique_ptr<ByteSink>{std::move(file)};
  }
  return make_gzip_sink(std::move(file), compress_level);
}

}  // namespace hefesto::gds
