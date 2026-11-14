// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Gzip source and sink (see hefesto/gds/stream/gzip.hpp). Inflate follows
// cell/mag_input.cpp (gzip framing, concatenated members). Owner: GDSR.

#include "hefesto/gds/stream/gzip.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include <zlib.h>

namespace hefesto::gds {

namespace {

constexpr std::size_t kChunk = 64 * 1024;
constexpr std::uint64_t kFirstInterval = std::uint64_t{4} << 20U;  // 4 MiB of output
constexpr int kGzipWindow = 15 + 16;                              // gzip framing only

struct InflateEnd {
  void operator()(z_stream* zs) const noexcept {
    inflateEnd(zs);
    delete zs;
  }
};
using InflateStream = std::unique_ptr<z_stream, InflateEnd>;

diag::Diagnostic corrupt(const std::string& what) {
  return diag::make("GDS-STREAM-002", "The compressed GDS stream is corrupt: " + what + ".");
}

class GzipSource final : public ByteSource {
 public:
  GzipSource(std::vector<std::byte> compressed, std::size_t max_feed)
      : in_(std::move(compressed)), out_(kChunk), max_feed_(std::max<std::size_t>(max_feed, 1)) {}

  [[nodiscard]] diag::Outcome<std::size_t> read(std::span<std::byte> out) override;
  [[nodiscard]] std::uint64_t position() const noexcept override { return chunk_start_ + out_pos_; }
  [[nodiscard]] diag::Status seek(std::uint64_t offset) override;

 private:
  struct Checkpoint {
    std::uint64_t offset = 0;
    bool eof = false;
    InflateStream stream;
  };

  [[nodiscard]] diag::Status restart();
  [[nodiscard]] diag::Status refill();
  [[nodiscard]] diag::Status inflate_step();
  void feed();
  [[nodiscard]] diag::Status skip_to(std::uint64_t offset);
  [[nodiscard]] diag::Status restore(const Checkpoint& checkpoint);
  void maybe_checkpoint();

  std::vector<std::byte> in_;
  std::vector<std::byte> out_;
  InflateStream zs_;
  std::size_t out_len_ = 0;
  std::size_t out_pos_ = 0;
  std::uint64_t chunk_start_ = 0;
  bool eof_ = false;
  std::vector<Checkpoint> checkpoints_;
  std::uint64_t interval_ = kFirstInterval;
  std::size_t max_feed_;
};

diag::Status GzipSource::restart() {
  auto zs = InflateStream{new z_stream{}};
  if (inflateInit2(zs.get(), kGzipWindow) != Z_OK) {
    delete zs.release();  // never initialized: nothing to end
    return corrupt("zlib could not start inflating");
  }
  zs->next_in = reinterpret_cast<Bytef*>(in_.data());
  zs->avail_in = 0;  // fed in pieces by feed(), so inputs over 4 GiB work
  zs_ = std::move(zs);
  chunk_start_ = 0;
  out_len_ = 0;
  out_pos_ = 0;
  eof_ = in_.empty();
  return diag::Done{};
}

void GzipSource::feed() {
  z_stream& zs = *zs_;
  if (zs.avail_in != 0) {
    return;
  }
  const auto used = static_cast<std::size_t>(reinterpret_cast<std::byte*>(zs.next_in) - in_.data());
  zs.avail_in = static_cast<uInt>(std::min<std::size_t>(in_.size() - used, max_feed_));
}

diag::Status GzipSource::inflate_step() {
  feed();
  z_stream& zs = *zs_;
  zs.next_out = reinterpret_cast<Bytef*>(out_.data() + out_len_);
  zs.avail_out = static_cast<uInt>(out_.size() - out_len_);
  const int rc = inflate(&zs, Z_NO_FLUSH);
  out_len_ = out_.size() - zs.avail_out;
  if (rc == Z_STREAM_END) {
    // The magic is checked against the whole input: the feed window may end
    // inside the next member's first two bytes.
    const auto used = static_cast<std::size_t>(reinterpret_cast<std::byte*>(zs.next_in) - in_.data());
    const bool another = in_.size() - used >= 2 && in_[used] == std::byte{0x1f} && in_[used + 1] == std::byte{0x8b};
    if (another) {
      return inflateReset(&zs) == Z_OK ? diag::Status{diag::Done{}} : diag::Status{corrupt("bad member")};
    }
    eof_ = true;  // anything after the last member is ignored, as gzread does
    return diag::Done{};
  }
  if (rc == Z_BUF_ERROR) {
    // No progress with room for output: the input ended inside a member.
    return corrupt("unexpected end of the compressed data");
  }
  if (rc != Z_OK) {
    return corrupt(zs.msg != nullptr ? zs.msg : "inflate failed");
  }
  return diag::Done{};
}

void GzipSource::maybe_checkpoint() {
  const std::uint64_t last = checkpoints_.empty() ? 0 : checkpoints_.back().offset;
  if (chunk_start_ < last + interval_) {
    return;
  }
  auto copy = InflateStream{new z_stream{}};
  if (inflateCopy(copy.get(), zs_.get()) != Z_OK) {
    delete copy.release();  // not initialized: nothing to end
    return;                 // a missing checkpoint only costs time
  }
  checkpoints_.push_back(Checkpoint{chunk_start_, eof_, std::move(copy)});
  if (checkpoints_.size() >= kMaxCheckpoints) {
    // Keep every other checkpoint and double the spacing: memory stays bounded.
    std::vector<Checkpoint> kept;
    for (std::size_t i = 1; i < checkpoints_.size(); i += 2) {
      kept.push_back(std::move(checkpoints_[i]));
    }
    checkpoints_ = std::move(kept);
    interval_ *= 2;
  }
}

diag::Status GzipSource::refill() {
  chunk_start_ += out_len_;
  out_len_ = 0;
  out_pos_ = 0;
  if (!eof_) {
    maybe_checkpoint();
  }
  while (out_len_ == 0 && !eof_) {
    if (auto status = inflate_step(); !status.ok()) {
      return status;
    }
  }
  return diag::Done{};
}

diag::Outcome<std::size_t> GzipSource::read(std::span<std::byte> out) {
  if (!zs_) {
    if (auto status = restart(); !status.ok()) {
      return status.error();
    }
  }
  std::size_t done = 0;
  while (done < out.size()) {
    if (out_pos_ == out_len_) {
      if (eof_) {
        break;
      }
      if (auto status = refill(); !status.ok()) {
        return status.error();
      }
      continue;
    }
    const std::size_t count = std::min(out.size() - done, out_len_ - out_pos_);
    std::copy_n(out_.begin() + static_cast<std::ptrdiff_t>(out_pos_), count, out.begin() + static_cast<std::ptrdiff_t>(done));
    out_pos_ += count;
    done += count;
  }
  return done;
}

diag::Status GzipSource::skip_to(std::uint64_t offset) {
  std::array<std::byte, 4096> scratch{};
  while (position() < offset) {
    const auto want = static_cast<std::size_t>(std::min<std::uint64_t>(scratch.size(), offset - position()));
    auto got = read(std::span{scratch}.first(want));
    if (!got.ok()) {
      return got.error();
    }
    if (got.value() == 0) {
      return diag::make("GDS-STREAM-003", "Seek to byte " + std::to_string(offset) +
                                              " is past the end of the decompressed GDS stream (" +
                                              std::to_string(position()) + " bytes).");
    }
  }
  return diag::Done{};
}

diag::Status GzipSource::restore(const Checkpoint& checkpoint) {
  auto copy = InflateStream{new z_stream{}};
  if (inflateCopy(copy.get(), checkpoint.stream.get()) != Z_OK) {
    delete copy.release();  // not initialized: nothing to end
    return restart();
  }
  zs_ = std::move(copy);
  chunk_start_ = checkpoint.offset;
  out_len_ = 0;
  out_pos_ = 0;
  eof_ = checkpoint.eof;
  return diag::Done{};
}

diag::Status GzipSource::seek(std::uint64_t offset) {
  if (zs_ && offset >= chunk_start_ && offset <= chunk_start_ + out_len_) {
    out_pos_ = static_cast<std::size_t>(offset - chunk_start_);  // inside the current chunk
    return diag::Done{};
  }
  if (!zs_ || offset < position()) {
    const auto best = std::find_if(checkpoints_.rbegin(), checkpoints_.rend(),
                                   [&](const Checkpoint& c) { return c.offset <= offset; });
    auto status = best == checkpoints_.rend() ? restart() : restore(*best);
    if (!status.ok()) {
      return status;
    }
  }
  return skip_to(offset);
}

// ---- sink -------------------------------------------------------------------

struct DeflateEnd {
  void operator()(z_stream* zs) const noexcept {
    deflateEnd(zs);
    delete zs;
  }
};

class GzipSink final : public ByteSink {
 public:
  GzipSink(std::unique_ptr<ByteSink> inner, std::unique_ptr<z_stream, DeflateEnd> zs)
      : inner_(std::move(inner)), zs_(std::move(zs)), out_(kChunk) {}

  [[nodiscard]] diag::Status write(std::span<const std::byte> bytes) override {
    written_ += bytes.size();
    return pump(bytes, Z_NO_FLUSH);
  }
  [[nodiscard]] std::uint64_t position() const noexcept override { return written_; }
  [[nodiscard]] diag::Status finish() override {
    if (auto status = pump({}, Z_FINISH); !status.ok()) {
      return status;
    }
    return inner_->finish();
  }

 private:
  [[nodiscard]] diag::Status pump(std::span<const std::byte> bytes, int flush) {
    z_stream& zs = *zs_;
    zs.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(bytes.data()));  // zlib's API is not const
    zs.avail_in = static_cast<uInt>(bytes.size());
    int rc = Z_OK;
    do {
      zs.next_out = reinterpret_cast<Bytef*>(out_.data());
      zs.avail_out = static_cast<uInt>(out_.size());
      rc = deflate(&zs, flush);
      if (rc == Z_STREAM_ERROR) {
        return diag::make("GDS-STREAM-005", "zlib failed while compressing the GDS output.");
      }
      const std::size_t produced = out_.size() - zs.avail_out;
      if (produced > 0) {
        if (auto status = inner_->write(std::span{out_}.first(produced)); !status.ok()) {
          return status;
        }
      }
    } while (zs.avail_out == 0 || (flush == Z_FINISH && rc != Z_STREAM_END));
    return diag::Done{};
  }

  std::unique_ptr<ByteSink> inner_;
  std::unique_ptr<z_stream, DeflateEnd> zs_;
  std::vector<std::byte> out_;
  std::uint64_t written_ = 0;
};

}  // namespace

bool is_gzip(std::span<const std::byte> bytes) noexcept {
  return bytes.size() >= 2 && bytes[0] == std::byte{0x1f} && bytes[1] == std::byte{0x8b};
}

std::unique_ptr<ByteSource> make_gzip_source(std::vector<std::byte> compressed, std::size_t max_feed) {
  return std::make_unique<GzipSource>(std::move(compressed), max_feed);
}

diag::Outcome<std::unique_ptr<ByteSink>> make_gzip_sink(std::unique_ptr<ByteSink> inner, int level) {
  if (level < 1 || level > 9) {
    return diag::make("GDS-STREAM-005", "Compression level " + std::to_string(level) + " is not 1 to 9.");
  }
  auto zs = std::unique_ptr<z_stream, DeflateEnd>{new z_stream{}};
  if (deflateInit2(zs.get(), level, Z_DEFLATED, kGzipWindow, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
    delete zs.release();  // never initialized: nothing to end
    return diag::make("GDS-STREAM-005", "zlib could not start compressing the GDS output.");
  }
  return std::unique_ptr<ByteSink>{std::make_unique<GzipSink>(std::move(inner), std::move(zs))};
}

}  // namespace hefesto::gds
