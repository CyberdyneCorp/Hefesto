// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Owner: SIMD.

#include "ipc/frame_io.hpp"

#include <unistd.h>

#include <array>
#include <cerrno>

namespace hefesto::simd {

namespace {

bool read_exact(int fd, std::byte* data, std::size_t size) {
  while (size > 0) {
    const ssize_t n = ::read(fd, data, size);
    if (n > 0) {
      data += n;
      size -= static_cast<std::size_t>(n);
    } else if (n == 0 || errno != EINTR) {
      return false;
    }
  }
  return true;
}

bool write_all(int fd, const std::byte* data, std::size_t size) {
  while (size > 0) {
    const ssize_t n = ::write(fd, data, size);
    if (n > 0) {
      data += n;
      size -= static_cast<std::size_t>(n);
    } else if (n < 0 && errno != EINTR) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool FrameWriter::write(const sim::frames::Message& message) {
  const std::vector<std::byte> body = sim::frames::encode(message);
  std::vector<std::byte> frame;
  frame.reserve(body.size() + 4);
  sim::frames::append_length(frame, body);
  std::lock_guard lock{mutex_};
  return write_all(fd_, frame.data(), frame.size());
}

ReadResult read_frame(int fd, std::vector<std::byte>& body, std::string& detail, std::uint32_t limit) {
  std::array<std::byte, 4> prefix{};
  if (!read_exact(fd, prefix.data(), prefix.size())) {
    return ReadResult::end;
  }
  const auto [size, error] = sim::frames::parse_length(prefix, limit);
  if (error != sim::frames::Error::none) {
    detail = std::string{sim::frames::error_name(error)} + " frame length " + std::to_string(size);
    return ReadResult::error;
  }
  body.resize(size);
  return read_exact(fd, body.data(), body.size()) ? ReadResult::frame : ReadResult::end;
}

}  // namespace hefesto::simd
