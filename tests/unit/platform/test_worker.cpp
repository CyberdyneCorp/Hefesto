// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto_test_worker: a worker for the WorkerHost tests (capability
// hefesto-architecture "Long-lived worker processes"). It speaks the
// channel's framing (4-byte little-endian length, then the body) and
// behaves as its first argument says:
//
//   echo        sends every frame back until end of input
//   exit=<n>    exits with status <n> at once
//   exit-after  echoes one frame, then exits with status 3
//   crash       aborts after reading one frame
//   hang        never reads and never exits
//   oversize    announces a 100 MiB frame
//   empty       announces a zero-length frame
//   describe    sends one frame "<cwd>\n<argc>\n<args...>\n<env count>"
//
// Owner: PLATFORM.

#include <unistd.h>

#include <array>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

extern char** environ;  // NOLINT(readability-redundant-declaration)

namespace {

bool read_exact(void* data, std::size_t size) {
  auto* p = static_cast<char*>(data);
  while (size > 0) {
    const ssize_t n = ::read(STDIN_FILENO, p, size);
    if (n > 0) {
      p += n;
      size -= static_cast<std::size_t>(n);
    } else if (n == 0 || errno != EINTR) {
      return false;
    }
  }
  return true;
}

void write_exact(const void* data, std::size_t size) {
  const auto* p = static_cast<const char*>(data);
  while (size > 0) {
    const ssize_t n = ::write(STDOUT_FILENO, p, size);
    if (n <= 0) {
      std::_Exit(9);
    }
    p += n;
    size -= static_cast<std::size_t>(n);
  }
}

void write_length(std::uint32_t size) {
  const std::array<unsigned char, 4> prefix{static_cast<unsigned char>(size & 0xFFU),
                                            static_cast<unsigned char>((size >> 8U) & 0xFFU),
                                            static_cast<unsigned char>((size >> 16U) & 0xFFU),
                                            static_cast<unsigned char>((size >> 24U) & 0xFFU)};
  write_exact(prefix.data(), prefix.size());
}

void write_frame(const std::string& body) {
  write_length(static_cast<std::uint32_t>(body.size()));
  write_exact(body.data(), body.size());
}

bool read_frame(std::string& body) {
  std::array<unsigned char, 4> prefix{};
  if (!read_exact(prefix.data(), prefix.size())) {
    return false;
  }
  std::uint32_t size = 0;
  for (std::size_t i = 0; i < prefix.size(); ++i) {
    size |= static_cast<std::uint32_t>(prefix[i]) << (8U * i);
  }
  body.resize(size);
  return read_exact(body.data(), body.size());
}

int echo(int limit) {
  std::string body;
  for (int i = 0; (limit < 0 || i < limit) && read_frame(body); ++i) {
    write_frame(body);
  }
  return 0;
}

int describe(int argc, char** argv) {
  std::array<char, PATH_MAX> cwd{};
  std::string text = ::getcwd(cwd.data(), cwd.size()) != nullptr ? std::string{cwd.data()} : std::string{};
  text += "\n" + std::to_string(argc);
  for (int i = 0; i < argc; ++i) {
    text += "\n" + std::string{argv[i]};  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }
  int count = 0;
  for (char** e = environ; e != nullptr && *e != nullptr; ++e) {  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    ++count;
  }
  text += "\n" + std::to_string(count);
  write_frame(text);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string_view mode = argc > 1 ? argv[1] : "echo";  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::string body;
  if (mode == "echo") {
    return echo(-1);
  }
  if (mode.starts_with("exit=")) {
    return std::atoi(std::string{mode.substr(5)}.c_str());
  }
  if (mode == "exit-after") {
    echo(1);
    return 3;
  }
  if (mode == "crash") {
    read_frame(body);
    std::abort();
  }
  if (mode == "hang") {
    for (;;) {
      ::pause();
    }
  }
  if (mode == "oversize") {
    write_length(100U * 1024U * 1024U);
    return echo(-1);
  }
  if (mode == "empty") {
    write_length(0);
    return echo(-1);
  }
  return mode == "describe" ? describe(argc, argv) : 2;
}
